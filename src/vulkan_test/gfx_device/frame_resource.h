#pragma once

#include "descriptor_pool.h"
#include "common/scratch_allocator.h"

namespace VKN {

    class Device;
    struct Buffer;
    class Frame_resource {
      public:
        Frame_resource(Device& gfx_device)
            : m_gfx_device(gfx_device)
            , m_descriptor_pool(gfx_device)
            , m_frame_scratch_allocator(gfx_device,
                  16 * 1024 * 1024,
                  vk::BufferUsageFlagBits::eUniformBuffer |
                      vk::BufferUsageFlagBits::eStorageBuffer) // 16MB default page size for frame scratch allocator
        {
        }

        void destroy_resources();

        void begin_frame();
        void end_frame();

        Scratch_allocator& frame_scratch_allocator() { return m_frame_scratch_allocator; }
        const Scratch_allocator& frame_scratch_allocator() const { return m_frame_scratch_allocator; }

        vk::CommandBuffer m_command_buffer;
        bool m_command_buffer_opened = false;

        vk::Semaphore m_image_available_semaphore;
        vk::Fence m_inflight_fence;

        // descriptor pool per flight
        Descriptor_pool m_descriptor_pool;
      private:
        Device& m_gfx_device;

        Scratch_allocator m_frame_scratch_allocator;
    };

} // namespace VKN
