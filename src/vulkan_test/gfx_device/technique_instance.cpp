#include "technique_instance.h"

#include <deque>

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

    bool Technique_instance::apply()
    {
        auto&& device          = m_tech.m_gfx_device.m_device;
        auto&& frame_resource  = m_tech.m_gfx_device.curr_frame_resource();
        auto&& descriptor_pool = frame_resource.m_descriptor_pool;
        auto&& command_buffer  = frame_resource.m_command_buffer;

        // Use to fetch persistent textures and samplers when binding.
        auto&& resource_manager = m_tech.m_gfx_device.m_resource_manager;

        // scans m_sampled_image_array_map and computes, per set layout index, how many sampled-image descriptors you
        // actually need to allocate.
        // This is needed because bindless uses variable descriptor count, and that count is passed at descriptor set
        // allocation time.
        std::unordered_map<uint32_t, uint32_t> variable_count_by_set_index;
        for (const auto& [reflected_name, texture_names] : m_sampled_image_array_map) {
            const auto* reflected = m_tech.find_binding(reflected_name);
            if (!reflected) {
                return false;
            }
            variable_count_by_set_index[reflected->m_set_layout_index] = std::max(
                variable_count_by_set_index[reflected->m_set_layout_index], static_cast<uint32_t>(texture_names.size()));
        }

        // Why deque?
        struct Pending_set {
            vk::DescriptorSet descriptor_set{};
            std::vector<vk::DescriptorBufferInfo> buffer_infos;
            std::deque<vk::DescriptorImageInfo> image_infos;
            std::vector<vk::WriteDescriptorSet> writes;
        };

        std::unordered_map<uint32_t, Pending_set> pending_by_set_index;

        // Bind constant buffers
        for (const auto& [reflected_name, weak_buffer] : m_constant_buffer_map) {
            const auto* reflected = m_tech.find_binding(reflected_name);
            if (!reflected) {
                return false;
            }
            if (reflected->m_descriptor_type != vk::DescriptorType::eUniformBuffer) {
                return false;
            }

            auto buffer = weak_buffer.lock();
            if (!buffer) {
                return false;
            }

            auto& pending = pending_by_set_index[reflected->m_set_layout_index];
            if (!pending.descriptor_set) {
                uint32_t variable_count = 0;
                auto it                 = variable_count_by_set_index.find(reflected->m_set_layout_index);
                if (it != variable_count_by_set_index.end()) {
                    variable_count = it->second;
                }

                pending.descriptor_set = descriptor_pool.create_descriptor_set(
                    m_tech.m_descriptorset_layouts[reflected->m_set_layout_index], variable_count);
            }

            pending.buffer_infos.push_back(vk::DescriptorBufferInfo{
                .buffer = buffer->m_buffer,
                .offset = 0,
                .range  = VK_WHOLE_SIZE,
            });

            const auto& buffer_info = pending.buffer_infos.back();
            pending.writes.push_back(vk::WriteDescriptorSet{
                .dstSet          = pending.descriptor_set,
                .dstBinding      = reflected->m_binding_number,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType  = reflected->m_descriptor_type,
                .pBufferInfo     = &buffer_info,
            });
        }

        // Bind textures
        for (const auto& [reflected_name, texture_names] : m_sampled_image_array_map) {
            const auto* reflected = m_tech.find_binding(reflected_name);
            if (!reflected) {
                return false;
            }
            if (reflected->m_descriptor_type != vk::DescriptorType::eSampledImage) {
                return false;
            }
            if (texture_names.empty()) {
                continue;
            }

            auto& pending = pending_by_set_index[reflected->m_set_layout_index];
            if (!pending.descriptor_set) {
                uint32_t variable_count = static_cast<uint32_t>(texture_names.size());
                pending.descriptor_set  = descriptor_pool.create_descriptor_set(
                    m_tech.m_descriptorset_layouts[reflected->m_set_layout_index], variable_count);
            }

            const size_t start = pending.image_infos.size();
            for (const auto& texture_name : texture_names) {
                auto texture = resource_manager->get_texture(texture_name);
                pending.image_infos.push_back(vk::DescriptorImageInfo{
                    .sampler     = vk::Sampler{},
                    .imageView   = texture.m_view,
                    .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                });
            }

            pending.writes.push_back(vk::WriteDescriptorSet{
                .dstSet          = pending.descriptor_set,
                .dstBinding      = reflected->m_binding_number,
                .dstArrayElement = 0,
                .descriptorCount = static_cast<uint32_t>(texture_names.size()),
                .descriptorType  = vk::DescriptorType::eSampledImage,
                .pImageInfo      = &pending.image_infos[start],
            });
        }

        // Bind samplers
        for (const auto& [reflected_name, sampler_name] : m_sampler_map) {
            const auto* reflected = m_tech.find_binding(reflected_name);
            if (!reflected) {
                return false;
            }
            if (reflected->m_descriptor_type != vk::DescriptorType::eSampler) {
                return false;
            }

            auto sampler = resource_manager->get_sampler(sampler_name);

            auto& pending = pending_by_set_index[reflected->m_set_layout_index];
            if (!pending.descriptor_set) {
                pending.descriptor_set =
                    descriptor_pool.create_descriptor_set(m_tech.m_descriptorset_layouts[reflected->m_set_layout_index], 0);
            }

            pending.image_infos.push_back(vk::DescriptorImageInfo{
                .sampler     = sampler,
                .imageView   = vk::ImageView{},
                .imageLayout = vk::ImageLayout::eUndefined,
            });

            pending.writes.push_back(vk::WriteDescriptorSet{
                .dstSet          = pending.descriptor_set,
                .dstBinding      = reflected->m_binding_number,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType  = vk::DescriptorType::eSampler,
                .pImageInfo      = &pending.image_infos.back(),
            });
        }

        for (auto& [_, pending] : pending_by_set_index) {
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
