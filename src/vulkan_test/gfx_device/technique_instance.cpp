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
            return false;
        }

        if (!data || size == 0) {
            return false;
        }

        auto&& frame_resource = m_tech.m_gfx_device.curr_frame_resource();

        const vk::PhysicalDeviceProperties props = m_tech.m_gfx_device.m_physical_device.getProperties();
        const vk::DeviceSize required_alignment =
            std::max<vk::DeviceSize>(16u, static_cast<vk::DeviceSize>(props.limits.minUniformBufferOffsetAlignment));

        auto allocation = frame_resource.frame_scratch_allocator().allocate_and_copy(
            data, static_cast<vk::DeviceSize>(size), required_alignment);

        if (!allocation.m_buffer) {
            return false;
        }

        assert((allocation.m_offset % required_alignment) == 0);
        assert(allocation.m_size >= static_cast<vk::DeviceSize>(size));

        m_constant_buffer_map[reflected_name] = allocation;
        return true;
    }

    bool Technique_instance::bind_sampled_image_by_name(
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

    bool Technique_instance::bind_sampled_image_by_name(const std::string& reflected_name, const std::string& texture_name)
    {
        if (texture_name.empty()) {
            return false;
        }

        return bind_sampled_image_by_name(reflected_name, std::vector<std::string>{texture_name});
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

    bool Technique_instance::bind_storage_image_by_name(const std::string& reflected_name, const std::string& texture_name)
    {
        const auto* reflected = m_tech.find_binding(reflected_name);
        if (!reflected)
            return false;

        if (reflected->m_descriptor_type != vk::DescriptorType::eStorageImage) {
            return false;
        }

        m_storage_image_map[reflected_name] = texture_name;
        return true;
    }

    bool Technique_instance::bind_storage_buffer_by_name(const std::string& reflected_name, const std::string& buffer_name)
    {
        const auto* reflected = m_tech.find_binding(reflected_name);
        if (!reflected) {
            return false;
        }
        if (reflected->m_descriptor_type != vk::DescriptorType::eStorageBuffer) {
            return false;
        }
        if (buffer_name.empty()) {
            return false;
        }

        m_storage_buffer_map[reflected_name] = buffer_name;
        return true;
    }

    bool Technique_instance::bind_acceleration_structure_by_name(
        const std::string& reflected_name, const std::string& tlas_name)
    {
        const auto* reflected = m_tech.find_binding(reflected_name);
        if (!reflected) {
            return false;
        }
        if (reflected->m_descriptor_type != vk::DescriptorType::eAccelerationStructureKHR) {
            return false;
        }
        if (tlas_name.empty()) {
            return false;
        }

        // blas already created and stored in resource manager, so we just need to store the name for later use in apply()
        m_acceleration_structure_map[reflected_name] = tlas_name;
        return true;
    }

    namespace {

        enum class Pending_write_kind { Buffer, Image, AccelerationStructure };

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
            std::vector<vk::AccelerationStructureKHR> as_handles;
            std::vector<vk::WriteDescriptorSetAccelerationStructureKHR> as_infos;
            std::vector<Pending_write_plan> plans;
            // This will use Pending_write_plan to generate the actual vk::WriteDescriptorSet objects, which will be used to
            // update the descriptor set.
            std::vector<vk::WriteDescriptorSet> writes;
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

        bool build_variable_descriptor_count_by_set_index_lookup(
            const Technique_instance& inst, std::unordered_map<uint32_t, uint32_t>& variable_descriptor_count_by_set_index)
        {
            for (const auto& [reflected_name, texture_names] : inst.m_sampled_image_array_map) {
                const auto* reflected = find_binding_checked(inst.m_tech, reflected_name, vk::DescriptorType::eSampledImage);
                if (!reflected) {
                    return false;
                }

                // in reality, each set should only have one variable descriptor count binding, but we will take the max just
                // in case there are multiple (which should be an error in shader reflection or shader writing)
                if (reflected->m_is_variable_descriptor_count) {
                    auto& current_count = variable_descriptor_count_by_set_index[reflected->m_set_layout_index];
                    current_count       = std::max(current_count, static_cast<uint32_t>(texture_names.size()));
                }
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

        std::unordered_map<uint32_t, uint32_t> variable_descriptor_count_by_set_index;
        if (!build_variable_descriptor_count_by_set_index_lookup(*this, variable_descriptor_count_by_set_index)) {
            return false;
        }

        std::unordered_map<uint32_t, Pending_set> pending_by_set_index;

        auto ensure_set_allocated = [&](uint32_t set_layout_index, uint32_t fallback_variable_count) -> Pending_set& {
            auto& pending = pending_by_set_index[set_layout_index];
            if (pending.descriptor_set) {
                return pending;
            }

            uint32_t variable_count = fallback_variable_count;
            auto it                 = variable_descriptor_count_by_set_index.find(set_layout_index);
            if (it != variable_descriptor_count_by_set_index.end() && it->second > 0) {
                variable_count = it->second;
            }

            pending.descriptor_set =
                descriptor_pool.create_descriptor_set(m_tech.m_descriptorset_layouts[set_layout_index], variable_count);

            return pending;
        };

        for (const auto& [reflected_name, allocation] : m_constant_buffer_map) {
            const auto* reflected = find_binding_checked(m_tech, reflected_name, vk::DescriptorType::eUniformBuffer);
            if (!reflected) {
                return false;
            }

            auto& pending = ensure_set_allocated(reflected->m_set_layout_index, 0);

            const size_t start = pending.buffer_infos.size();
            pending.buffer_infos.push_back(vk::DescriptorBufferInfo{
                .buffer = allocation.m_buffer,
                .offset = allocation.m_offset,
                .range  = allocation.m_size,
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

            const uint32_t consecutive_write_count = static_cast<uint32_t>(texture_names.size());
            const size_t start                     = pending.image_infos.size();
            // Allocate one contiguous span for this binding write.
            pending.image_infos.resize(start + consecutive_write_count);

            for (uint32_t i = 0; i < consecutive_write_count; ++i) {
                auto texture                   = resource_manager->get_texture(texture_names[i]);
                pending.image_infos[start + i] = vk::DescriptorImageInfo{
                    .sampler     = vk::Sampler{},
                    .imageView   = texture.m_view,
                    .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                };
            }

            pending.plans.push_back(Pending_write_plan{
                .kind             = Pending_write_kind::Image,
                .binding          = reflected->m_binding_number,
                .descriptor_type  = vk::DescriptorType::eSampledImage,
                .descriptor_count = consecutive_write_count,
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

        for (const auto& [reflected_name, uav_name] : m_storage_image_map) {
            const auto* reflected = m_tech.find_binding(reflected_name);
            if (!reflected) {
                return false;
            }

            if (reflected->m_descriptor_type == vk::DescriptorType::eStorageImage) {
                auto& pending = ensure_set_allocated(reflected->m_set_layout_index, 0);

                auto texture = resource_manager->get_texture(uav_name);

                const size_t start = pending.image_infos.size();
                pending.image_infos.push_back(vk::DescriptorImageInfo{
                    .sampler     = vk::Sampler{},
                    .imageView   = texture.m_view,
                    .imageLayout = vk::ImageLayout::eGeneral,
                });

                pending.plans.push_back(Pending_write_plan{
                    .kind             = Pending_write_kind::Image,
                    .binding          = reflected->m_binding_number,
                    .descriptor_type  = vk::DescriptorType::eStorageImage,
                    .descriptor_count = 1,
                    .start_index      = start,
                });
            }
            else {
                return false;
            }
        }

        for (const auto& [reflected_name, buffer_name] : m_storage_buffer_map) {
            const auto* reflected = find_binding_checked(m_tech, reflected_name, vk::DescriptorType::eStorageBuffer);
            if (!reflected) {
                return false;
            }

            auto storage_buffer = resource_manager->get_storage_buffer(buffer_name);
            if (!storage_buffer.m_buffer || storage_buffer.m_size == 0) {
                return false;
            }

            auto& pending = ensure_set_allocated(reflected->m_set_layout_index, 0);

            const size_t start = pending.buffer_infos.size();
            pending.buffer_infos.push_back(vk::DescriptorBufferInfo{
                .buffer = storage_buffer.m_buffer,
                .offset = 0,
                .range  = static_cast<vk::DeviceSize>(storage_buffer.m_size),
            });

            pending.plans.push_back(Pending_write_plan{
                .kind             = Pending_write_kind::Buffer,
                .binding          = reflected->m_binding_number,
                .descriptor_type  = vk::DescriptorType::eStorageBuffer,
                .descriptor_count = 1,
                .start_index      = start,
            });
        }

        for (const auto& [reflected_name, tlas_name] : m_acceleration_structure_map) {
            const auto* reflected =
                find_binding_checked(m_tech, reflected_name, vk::DescriptorType::eAccelerationStructureKHR);
            if (!reflected) {
                return false;
            }

            auto tlas = resource_manager->get_tlas(tlas_name);
            if (!tlas.m_accel_struct.m_accel_struct) {
                return false;
            }

            auto& pending = ensure_set_allocated(reflected->m_set_layout_index, 0);

            const size_t start = pending.as_handles.size();
            pending.as_handles.push_back(tlas.m_accel_struct.m_accel_struct);

            vk::WriteDescriptorSetAccelerationStructureKHR as_info{
                .sType                      = vk::StructureType::eWriteDescriptorSetAccelerationStructureKHR,
                .pNext                      = nullptr,
                .accelerationStructureCount = 1,
                .pAccelerationStructures    = nullptr, // will be set later when we build the vk::WriteDescriptorSet objects
            };
            pending.as_infos.push_back(as_info);

            pending.plans.push_back(Pending_write_plan{
                .kind             = Pending_write_kind::AccelerationStructure,
                .binding          = reflected->m_binding_number,
                .descriptor_type  = vk::DescriptorType::eAccelerationStructureKHR,
                .descriptor_count = 1,
                .start_index      = start,
            });
        }

        //  update all decriptor sets
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
                else if (plan.kind == Pending_write_kind::Image) {
                    write.pImageInfo = &pending.image_infos[plan.start_index];
                }
                else if (plan.kind == Pending_write_kind::AccelerationStructure) { // AccelerationStructure
                    auto& as_info                   = pending.as_infos[plan.start_index];
                    as_info.pAccelerationStructures = &pending.as_handles[plan.start_index];
                    write.pNext                     = &as_info;
                }
                else {
                    assert(false && "Unknown Pending_write_kind");
                }

                pending.writes.push_back(write);
            }

            if (!pending.writes.empty()) {
                device.updateDescriptorSets(pending.writes, {});
            }
        }

        for (const auto& [set_layout_index, pending] : pending_by_set_index) {
            command_buffer.bindDescriptorSets(
                m_tech.m_bind_point, m_tech.m_pipeline_layout, set_layout_index, pending.descriptor_set, {});
        }

        return true;
    }

} // namespace VKN
