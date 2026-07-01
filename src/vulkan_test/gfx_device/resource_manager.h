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

        void destroy_buffer(Buffer& buffer);

        // test function
        void create_mesh();

        void create_storage_buffer(
            const std::string& name, const void* data, size_t size, vk::BufferUsageFlags additional_usage_flags = {});

        template <typename T>
        void create_storage_buffer_typed(const std::string& name, const std::vector<T>& data, vk::BufferUsageFlags additional_usage_flags = {})
        {
            // "Element type must be 4-byte aligned"
            assert(sizeof(T) % 4 == 0);

            size_t total_size = data.size() * sizeof(T);
            create_storage_buffer(name, data.data(), total_size, additional_usage_flags);

            OutputDebugStringA(DBG::Format("Created buffer '%s': %zu elements x %zu bytes = %zu total\n",
                name.c_str(),
                data.size(),
                sizeof(T),
                total_size)
                    .c_str());
        }

        Buffer m_vertex_buffer;
        Buffer m_index_buffer;

        // persistent create
        void create_texture(
            const std::string& name, const TextureData& texture_data, vk::ImageUsageFlags additional_usage_flags = {});
        void create_linear_wrap_sampler();

        Buffer get_storage_buffer(const std::string& name) const;
        Texture get_texture(const std::string& name) const;
        vk::Sampler get_sampler(const std::string& name) const;

        BLAS get_blas(const std::string& name) const;
        TLAS get_tlas(const std::string& name) const;

        // Build a Bottom-Level Acceleration Structure from vertex/index buffers
        // triangles: array of indices (each 3 consecutive indices = 1 triangle)
        // vertices: raw vertex positions (float3 data)
        // Returns handle to BLAS in acceleration structure storage
        VKN::BLAS build_blas_from_buffers(const std::string& name,
            const void* triangle_indices, // uint32_t array
            uint32_t triangle_count,      // number of triangles
            const void* vertex_positions, // float3 array
            uint32_t vertex_count);

        // Build a Top-Level Acceleration Structure from BLAS instances
        // blas_data: array of (BLAS + transform) pairs
        // Returns handle to TLAS
        VKN::TLAS build_tlas_from_blas_instances(
            const std::string& name, const std::vector<std::pair<const VKN::BLAS*, VkTransformMatrixKHR>>& blas_instances);

        void create_depth_buffer(uint32_t width, uint32_t height, vk::Format format = vk::Format::eD24UnormS8Uint);
        Texture depth_buffer() const { return m_depth_buffer; }

      private:
        Buffer create_buffer(const Buffer_create_info& create_info);

        Device& m_gfx_device;
        Scratch_allocator m_upload_scratch_allocator;

        // Persistent resources
        std::unordered_map<std::string, Texture> m_textures;
        std::unordered_map<std::string, vk::Sampler> m_samplers;
        std::unordered_map<std::string, Buffer> m_storage_buffers;

        std::unordered_map<std::string, BLAS> m_blas_map;
        std::unordered_map<std::string, TLAS> m_tlas_map;

        Texture m_depth_buffer;
    };

} // namespace VKN
