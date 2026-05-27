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

    bool Technique_instance::apply()
    {
        auto&& device          = m_tech.m_gfx_device.m_device;
        auto&& frame_resource  = m_tech.m_gfx_device.curr_frame_resource();
        auto&& descriptor_pool = frame_resource.m_descriptor_pool;
        auto&& command_buffer  = frame_resource.m_command_buffer;

        struct Pending_set {
            vk::DescriptorSet descriptor_set{};
            std::vector<vk::DescriptorBufferInfo> buffer_infos;
            std::vector<vk::WriteDescriptorSet> writes;
        };

        std::unordered_map<uint32_t, Pending_set> pending_by_set_index;

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
                pending.descriptor_set = descriptor_pool.create_descriptor_set(
                    m_tech.m_descriptorset_layouts[reflected->m_set_layout_index]);
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

        for (auto& [_, pending] : pending_by_set_index) {
            if (!pending.writes.empty()) {
                device.updateDescriptorSets(pending.writes, {});
            }
        }

        for (const auto& [set_layout_index, pending] : pending_by_set_index) {
            command_buffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                m_tech.m_pipeline_layout,
                set_layout_index,
                pending.descriptor_set,
                {});
        }

        return true;
    }

} // namespace VKN
