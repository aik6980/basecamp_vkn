#pragma once

#include "buffer.h"
#include "common/scratch_allocator.h"

namespace VKN {

    class Device;

    class Resource_manager {
      public:
        friend class Device;

        Resource_manager(Device& gfx_device)
            : m_gfx_device(gfx_device)
            , m_upload_scratch_allocator(gfx_device,
                  16 * 1024 * 1024,
                  vk::BufferUsageFlagBits::eTransferSrc) // 16MB default page size for staging buffer
        {
        }

        void destroy();

        // test function
        void create_mesh();

        void destroy_buffer(Buffer& buffer);

        // I think we can remove this function? since now we have scratch allocator for dynamic buffer creation, and we can
        // directly use create_buffer for static buffer creation
        Buffer create_constant_buffer(const void* src_data, size_t size);

        void create_storage_buffer(
            const std::string& name, const void* data, size_t size, vk::BufferUsageFlags additional_usage_flags = {});

        Buffer m_vertex_buffer;
        Buffer m_index_buffer;

        // persistent create
        void create_texture(
            const std::string& name, const TextureData& texture_data, vk::ImageUsageFlags additional_usage_flags = {});
        void create_linear_wrap_sampler();

        Buffer get_storage_buffer(const std::string& name) const;
        Texture get_texture(const std::string& name) const;
        vk::Sampler get_sampler(const std::string& name) const;

      private:
        Buffer create_buffer(const Buffer_create_info& create_info);

        Device& m_gfx_device;
        Scratch_allocator m_upload_scratch_allocator;

        // Persistent resources
        std::unordered_map<std::string, Texture> m_textures;
        std::unordered_map<std::string, vk::Sampler> m_samplers;
        std::unordered_map<std::string, Buffer> m_storage_buffers;
    };

} // namespace VKN
