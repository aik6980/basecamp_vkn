#include "technique_instance.h"

#include "device.h"
#include "resource_manager.h"
#include "shader.h"
#include "technique.h"

namespace VKN {

    bool Technique_instance::bind_constant_by_name(const std::string& reflected_name, const void* data, size_t size)
    {
        const auto* reflected = m_tech.find_binding(reflected_name);
        if (!reflected) {
            return false;
        }

        if (reflected->m_descriptor_type != vk::DescriptorType::eUniformBuffer) {
            // Current descriptor pool only supports uniform buffers.
            return false;
        }

        auto&& resource_manager = m_tech.m_gfx_device.m_resource_manager;
        auto&& frame_resource   = m_tech.m_gfx_device.curr_frame_resource();

        auto&& buffer = std::make_shared<Buffer>(resource_manager->create_constant_buffer(data, size));
        frame_resource.m_buffers.emplace_back(buffer);
        m_constant_buffer_map[reflected_name] = buffer;

        return true;
    }

    bool Technique_instance::bind_sampled_images_by_name(
        const std::string& reflected_name, const std::vector<std::string>& texture_names)
    {
        const auto* reflected = m_tech.find_binding(reflected_name);
        if (!reflected) {
            return false;
        }
        if (reflected->m_descriptor_type != vk::DescriptorType::eSampledImage) {
            return false;
        }
        if (texture_names.size() > reflected->m_descriptor_count) {
            return false;
        }

        m_sampled_image_array_map[reflected_name] = texture_names;
        return true;
    }

    bool Technique_instance::bind_sampler_by_name(const std::string& reflected_name, const std::string& sampler_name)
    {
        const auto* reflected = m_tech.find_binding(reflected_name);
        if (!reflected) {
            return false;
        }
        if (reflected->m_descriptor_type != vk::DescriptorType::eSampler) {
            return false;
        }

        m_sampler_map[reflected_name] = sampler_name;
        return true;
    }

    namespace {

        enum class Pending_write_kind { Buffer, Image };

        struct Pending_write_plan {
            Pending_write_kind kind{};
            uint32_t binding                   = 0;
            vk::DescriptorType descriptor_type = static_cast<vk::DescriptorType>(-1);
            uint32_t descriptor_count          = 0;
            size_t start_index                 = 0;
        };

        struct Pending_set {
            vk::DescriptorSet descriptor_set{};
            std::vector<vk::DescriptorBufferInfo> buffer_infos;
            std::vector<vk::DescriptorImageInfo> image_infos;
            std::vector<Pending_write_plan> plans;
            std::vector<vk::WriteDescriptorSet> writes;
        };

        struct Set_stats {
            uint32_t variable_descriptor_count = 0;
            uint32_t total_image_infos         = 0;
        };

        const Reflected_descriptor_binding* find_binding_checked(
            const Technique& tech, const std::string& reflected_name, vk::DescriptorType expected_type)
        {
            const auto* reflected = tech.find_binding(reflected_name);
            if (!reflected) {
                return nullptr;
            }
            if (reflected->m_descriptor_type != expected_type) {
                return nullptr;
            }
            return reflected;
        }

        bool build_set_stats(const Technique_instance& inst, std::unordered_map<uint32_t, Set_stats>& stats_by_set_index)
        {
            for (const auto& [reflected_name, texture_names] : inst.m_sampled_image_array_map) {
                const auto* reflected = find_binding_checked(inst.m_tech, reflected_name, vk::DescriptorType::eSampledImage);
                if (!reflected) {
                    return false;
                }

                auto& stats = stats_by_set_index[reflected->m_set_layout_index];
                stats.variable_descriptor_count =
                    std::max(stats.variable_descriptor_count, static_cast<uint32_t>(texture_names.size()));
                stats.total_image_infos += static_cast<uint32_t>(texture_names.size());
            }

            for (const auto& [reflected_name, sampler_name] : inst.m_sampler_map) {
                (void)sampler_name;
                const auto* reflected = find_binding_checked(inst.m_tech, reflected_name, vk::DescriptorType::eSampler);
                if (!reflected) {
                    return false;
                }

                auto& stats = stats_by_set_index[reflected->m_set_layout_index];
                stats.total_image_infos += 1u;
            }

            return true;
        }

    } // namespace

    bool Technique_instance::apply()
    {
        auto&& device           = m_tech.m_gfx_device.m_device;
        auto&& frame_resource   = m_tech.m_gfx_device.curr_frame_resource();
        auto&& descriptor_pool  = frame_resource.m_descriptor_pool;
        auto&& command_buffer   = frame_resource.m_command_buffer;
        auto&& resource_manager = m_tech.m_gfx_device.m_resource_manager;

        std::unordered_map<uint32_t, Set_stats> stats_by_set_index;
        if (!build_set_stats(*this, stats_by_set_index)) {
            return false;
        }

        std::unordered_map<uint32_t, Pending_set> pending_by_set_index;

        auto ensure_set_allocated = [&](uint32_t set_layout_index, uint32_t fallback_variable_count) -> Pending_set& {
            auto& pending = pending_by_set_index[set_layout_index];
            if (pending.descriptor_set) {
                return pending;
            }

            uint32_t variable_count = fallback_variable_count;
            auto stats_it           = stats_by_set_index.find(set_layout_index);
            if (stats_it != stats_by_set_index.end() && stats_it->second.variable_descriptor_count > 0) {
                variable_count = stats_it->second.variable_descriptor_count;
            }

            pending.descriptor_set =
                descriptor_pool.create_descriptor_set(m_tech.m_descriptorset_layouts[set_layout_index], variable_count);

            if (stats_it != stats_by_set_index.end() && stats_it->second.total_image_infos > 0) {
                pending.image_infos.reserve(stats_it->second.total_image_infos);
            }

            return pending;
        };

        for (const auto& [reflected_name, weak_buffer] : m_constant_buffer_map) {
            const auto* reflected = find_binding_checked(m_tech, reflected_name, vk::DescriptorType::eUniformBuffer);
            if (!reflected) {
                return false;
            }

            auto buffer = weak_buffer.lock();
            if (!buffer) {
                return false;
            }

            auto& pending = ensure_set_allocated(reflected->m_set_layout_index, 0);

            const size_t start = pending.buffer_infos.size();
            pending.buffer_infos.push_back(vk::DescriptorBufferInfo{
                .buffer = buffer->m_buffer,
                .offset = 0,
                .range  = VK_WHOLE_SIZE,
            });

            pending.plans.push_back(Pending_write_plan{
                .kind             = Pending_write_kind::Buffer,
                .binding          = reflected->m_binding_number,
                .descriptor_type  = reflected->m_descriptor_type,
                .descriptor_count = 1,
                .start_index      = start,
            });
        }

        for (const auto& [reflected_name, texture_names] : m_sampled_image_array_map) {
            const auto* reflected = find_binding_checked(m_tech, reflected_name, vk::DescriptorType::eSampledImage);
            if (!reflected) {
                return false;
            }
            if (texture_names.empty()) {
                continue;
            }

            auto& pending = ensure_set_allocated(reflected->m_set_layout_index, static_cast<uint32_t>(texture_names.size()));

            const size_t start = pending.image_infos.size();
            for (const auto& texture_name : texture_names) {
                auto texture = resource_manager->get_texture(texture_name);
                pending.image_infos.push_back(vk::DescriptorImageInfo{
                    .sampler     = vk::Sampler{},
                    .imageView   = texture.m_view,
                    .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                });
            }

            pending.plans.push_back(Pending_write_plan{
                .kind             = Pending_write_kind::Image,
                .binding          = reflected->m_binding_number,
                .descriptor_type  = vk::DescriptorType::eSampledImage,
                .descriptor_count = static_cast<uint32_t>(texture_names.size()),
                .start_index      = start,
            });
        }

        for (const auto& [reflected_name, sampler_name] : m_sampler_map) {
            const auto* reflected = find_binding_checked(m_tech, reflected_name, vk::DescriptorType::eSampler);
            if (!reflected) {
                return false;
            }

            auto sampler  = resource_manager->get_sampler(sampler_name);
            auto& pending = ensure_set_allocated(reflected->m_set_layout_index, 0);

            const size_t start = pending.image_infos.size();
            pending.image_infos.push_back(vk::DescriptorImageInfo{
                .sampler     = sampler,
                .imageView   = vk::ImageView{},
                .imageLayout = vk::ImageLayout::eUndefined,
            });

            pending.plans.push_back(Pending_write_plan{
                .kind             = Pending_write_kind::Image,
                .binding          = reflected->m_binding_number,
                .descriptor_type  = vk::DescriptorType::eSampler,
                .descriptor_count = 1,
                .start_index      = start,
            });
        }

        for (auto& [set_layout_index, pending] : pending_by_set_index) {
            (void)set_layout_index;

            pending.writes.clear();
            pending.writes.reserve(pending.plans.size());

            for (const auto& plan : pending.plans) {
                vk::WriteDescriptorSet write{
                    .dstSet          = pending.descriptor_set,
                    .dstBinding      = plan.binding,
                    .dstArrayElement = 0,
                    .descriptorCount = plan.descriptor_count,
                    .descriptorType  = plan.descriptor_type,
                };

                if (plan.kind == Pending_write_kind::Buffer) {
                    write.pBufferInfo = &pending.buffer_infos[plan.start_index];
                }
                else {
                    write.pImageInfo = pending.image_infos.data() + plan.start_index;
                }

                pending.writes.push_back(write);
            }

            if (!pending.writes.empty()) {
                device.updateDescriptorSets(pending.writes, {});
            }
        }

        for (const auto& [set_layout_index, pending] : pending_by_set_index) {
            command_buffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics, m_tech.m_pipeline_layout, set_layout_index, pending.descriptor_set, {});
        }

        return true;
    }

} // namespace VKN
