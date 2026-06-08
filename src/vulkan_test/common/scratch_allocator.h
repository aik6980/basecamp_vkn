#pragma once

#include "gfx_device/buffer.h"

namespace VKN {
    class Device;

    class Scratch_allocator {
      public:
        struct Allocation {
            vk::Buffer m_buffer{};
            vk::DeviceSize m_offset = 0;
            void* m_mapped_ptr      = nullptr;
            vk::DeviceSize m_size   = 0;
        };

        Scratch_allocator(Device& gfx_device, vk::DeviceSize default_page_size);
        ~Scratch_allocator();

        Allocation allocate(vk::DeviceSize size, vk::DeviceSize alignment = 16);
        Allocation allocate_and_copy(const void* src, vk::DeviceSize size, vk::DeviceSize alignment = 16);

        void reset();
        void destroy();

      private:
        struct Page {
            Buffer m_buffer{};
            void* m_mapped_ptr        = nullptr;
            vk::DeviceSize m_capacity = 0;
            vk::DeviceSize m_head     = 0;
        };

        Page& create_page(vk::DeviceSize min_capacity);

        Device& m_gfx_device;
        vk::DeviceSize m_default_page_size = 0;
        std::vector<Page> m_pages;
    };
} // namespace VKN