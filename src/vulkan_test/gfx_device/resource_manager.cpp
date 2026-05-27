#include "resource_manager.h"

#include "common/common_cpp.h"

#include "device.h"
#include "vma/vma.h"

namespace VKN {
    void Resource_manager::destroy()
    {
        destroy_buffer(m_vertex_buffer);
        destroy_buffer(m_index_buffer);

        for (auto&& buffer : m_staging_buffers) {
            destroy_buffer(buffer);
        }
        m_staging_buffers.clear();

        for (auto&& [name, sampler] : m_samplers) {
            m_gfx_device.m_device.destroySampler(sampler);
        }
        m_samplers.clear();

        // todo: make this into function and call when we want to destroy individual textures as well
        for (auto&& [name, tex] : m_textures) {
            if (tex.m_view) {
                m_gfx_device.m_device.destroyImageView(tex.m_view);
            }
            if (tex.m_alloc) {
                m_gfx_device.m_vma_allocator.freeMemory(tex.m_alloc);
            }
            if (tex.m_image) {
                m_gfx_device.m_device.destroyImage(tex.m_image);
            }
        }
        m_textures.clear();
    }

    void Resource_manager::create_mesh()
    {
        auto&& mesh = MeshDataGenerator::create_unit_cube();

        // create vertex buffer
        auto&& vb_data = MeshDataGenerator::to_p1c1(mesh.m_vertices);
        auto&& vb      = create_buffer({.m_usage_flags = vk::BufferUsageFlagBits::eVertexBuffer,
            .m_data                               = vb_data.data(),
            .m_size                               = vb_data.size() * sizeof(vb_data[0])});

        // create index buffer
        auto&& ib_data = mesh.m_indices.m_indices32;
        auto&& ib      = create_buffer({.m_usage_flags = vk::BufferUsageFlagBits::eIndexBuffer,
            .m_data                               = ib_data.data(),
            .m_size                               = ib_data.size() * sizeof(ib_data[0])});

        m_vertex_buffer = vb;
        m_index_buffer  = ib;
    }

    void Resource_manager::destroy_buffer(Buffer& buffer)
    {
        m_gfx_device.m_device.destroyBuffer(buffer.m_buffer);
        m_gfx_device.m_vma_allocator.freeMemory(buffer.m_allocation);
    }

    Buffer Resource_manager::create_buffer(const Buffer_create_info& create_info)
    {
        auto&& usage_flags = create_info.m_usage_flags;
        auto&& data        = create_info.m_data;
        auto&& size        = create_info.m_size;

        auto&& vma_allocator  = m_gfx_device.m_vma_allocator;
        auto&& command_buffer = m_gfx_device.m_single_use_command_buffer;

        // create staging buffer
        vk::Buffer staging_buffer;
        vma::Allocation staging_buffer_alloc;
        {
            auto&& create_info = vk::BufferCreateInfo{
                .size        = size,
                .usage       = vk::BufferUsageFlagBits::eTransferSrc,
                .sharingMode = vk::SharingMode::eExclusive,
            };
            auto&& alloc_create_info = vma::AllocationCreateInfo();
            alloc_create_info.setUsage(vma::MemoryUsage::eAuto);
            alloc_create_info.setFlags(
                vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped);

            vma::AllocationInfo buffer_alloc_info;

            std::tie(staging_buffer_alloc, staging_buffer) =
                vma_allocator.createBuffer(create_info, alloc_create_info, buffer_alloc_info);

            // copy src data
            std::memcpy(buffer_alloc_info.pMappedData, data, size);

            m_staging_buffers.emplace_back(Buffer{.m_buffer = staging_buffer, .m_allocation = staging_buffer_alloc});
        }

        vk::Buffer buffer;
        vma::Allocation buffer_alloc;
        {
            auto&& create_info = vk::BufferCreateInfo{
                .size        = size,
                .usage       = vk::BufferUsageFlagBits::eTransferDst | usage_flags,
                .sharingMode = vk::SharingMode::eExclusive,
            };
            auto&& alloc_create_info = vma::AllocationCreateInfo();

            std::tie(buffer_alloc, buffer) = vma_allocator.createBuffer(create_info, alloc_create_info);
        }

        // copy buffer
        auto&& copy_region = vk::BufferCopy(0, 0, size);
        command_buffer.copyBuffer(staging_buffer, buffer, copy_region);

        return Buffer{.m_buffer = buffer, .m_allocation = buffer_alloc};
    }

    Buffer Resource_manager::create_constant_buffer(const void* src_data, size_t size)
    {
        auto&& vma_allocator = m_gfx_device.m_vma_allocator;

        auto&& usage_flags = vk::BufferUsageFlagBits::eUniformBuffer;

        auto&& create_info = vk::BufferCreateInfo{
            .size        = size,
            .usage       = vk::BufferUsageFlagBits::eTransferSrc | usage_flags,
            .sharingMode = vk::SharingMode::eExclusive,
        };

        auto&& alloc_create_info = vma::AllocationCreateInfo();
        alloc_create_info.setUsage(vma::MemoryUsage::eAuto);
        alloc_create_info.setFlags(
            vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped);

        vk::Buffer buffer;
        vma::Allocation buffer_alloc;
        vma::AllocationInfo buffer_alloc_info;
        std::tie(buffer_alloc, buffer) = vma_allocator.createBuffer(create_info, alloc_create_info, buffer_alloc_info);

        // copy src data
        std::memcpy(buffer_alloc_info.pMappedData, src_data, size);

        return Buffer{.m_buffer = buffer, .m_allocation = buffer_alloc, .m_size = buffer_alloc_info.size};
    }

    void Resource_manager::create_texture(const std::string& name, const TextureData& texture_data)
    {
        auto&& device         = m_gfx_device.m_device;
        auto&& vma_allocator  = m_gfx_device.m_vma_allocator;
        auto&& command_buffer = m_gfx_device.m_single_use_command_buffer;

        if (texture_data.m_width == 0 || texture_data.m_height == 0) {
            return;
        }

        const vk::DeviceSize size_in_bytes = static_cast<vk::DeviceSize>(texture_data.pixel_size_in_byte()) *
                                             static_cast<vk::DeviceSize>(texture_data.m_width) *
                                             static_cast<vk::DeviceSize>(texture_data.m_height);

        if (texture_data.m_data.empty() || size_in_bytes == 0) {
            return;
        }

        // 1) Staging buffer
        vk::Buffer staging_buffer{};
        vma::Allocation staging_alloc{};
        {
            vk::BufferCreateInfo buffer_ci{
                .size        = size_in_bytes,
                .usage       = vk::BufferUsageFlagBits::eTransferSrc,
                .sharingMode = vk::SharingMode::eExclusive,
            };

            vma::AllocationCreateInfo alloc_ci{};
            alloc_ci.setUsage(vma::MemoryUsage::eAuto);
            alloc_ci.setFlags(
                vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped);

            vma::AllocationInfo alloc_info{};
            std::tie(staging_alloc, staging_buffer) = vma_allocator.createBuffer(buffer_ci, alloc_ci, alloc_info);

            std::memcpy(alloc_info.pMappedData, texture_data.m_data.data(), static_cast<size_t>(size_in_bytes));

            // Keep staging alive until Resource_manager::destroy
            m_staging_buffers.emplace_back(Buffer{
                .m_buffer     = staging_buffer,
                .m_allocation = staging_alloc,
                .m_size       = static_cast<size_t>(size_in_bytes),
            });
        }

        // 2) GPU image
        Texture tex{};
        tex.m_format = vk::Format::eR8G8B8A8Unorm;
        {
            vk::ImageCreateInfo image_ci{
                .imageType     = vk::ImageType::e2D,
                .format        = tex.m_format,
                .extent        = vk::Extent3D{texture_data.m_width, texture_data.m_height, 1},
                .mipLevels     = 1,
                .arrayLayers   = 1,
                .samples       = vk::SampleCountFlagBits::e1,
                .tiling        = vk::ImageTiling::eOptimal,
                .usage         = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                .sharingMode   = vk::SharingMode::eExclusive,
                .initialLayout = vk::ImageLayout::eUndefined,
            };

            vma::AllocationCreateInfo alloc_ci{};
            alloc_ci.setUsage(vma::MemoryUsage::eAutoPreferDevice);

            std::tie(tex.m_alloc, tex.m_image) = vma_allocator.createImage(image_ci, alloc_ci);
        }

        // 3) Transition undefined -> transfer dst
        {
            vk::ImageMemoryBarrier2 barrier{
                .srcStageMask        = vk::PipelineStageFlagBits2::eTopOfPipe,
                .srcAccessMask       = vk::AccessFlagBits2::eNone,
                .dstStageMask        = vk::PipelineStageFlagBits2::eTransfer,
                .dstAccessMask       = vk::AccessFlagBits2::eTransferWrite,
                .oldLayout           = vk::ImageLayout::eUndefined,
                .newLayout           = vk::ImageLayout::eTransferDstOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = tex.m_image,
                .subresourceRange =
                    vk::ImageSubresourceRange{
                        .aspectMask     = vk::ImageAspectFlagBits::eColor,
                        .baseMipLevel   = 0,
                        .levelCount     = 1,
                        .baseArrayLayer = 0,
                        .layerCount     = 1,
                    },
            };

            vk::DependencyInfo dep{
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers    = &barrier,
            };
            command_buffer.pipelineBarrier2(dep);
        }

        // 4) Copy staging -> image
        {
            vk::BufferImageCopy copy_region{
                .bufferOffset      = 0,
                .bufferRowLength   = 0,
                .bufferImageHeight = 0,
                .imageSubresource =
                    vk::ImageSubresourceLayers{
                        .aspectMask     = vk::ImageAspectFlagBits::eColor,
                        .mipLevel       = 0,
                        .baseArrayLayer = 0,
                        .layerCount     = 1,
                    },
                .imageOffset = vk::Offset3D{0, 0, 0},
                .imageExtent = vk::Extent3D{texture_data.m_width, texture_data.m_height, 1},
            };

            command_buffer.copyBufferToImage(
                staging_buffer, tex.m_image, vk::ImageLayout::eTransferDstOptimal, 1, &copy_region);
        }

        // 5) Transition transfer dst -> shader read
        {
            vk::ImageMemoryBarrier2 barrier{
                .srcStageMask        = vk::PipelineStageFlagBits2::eTransfer,
                .srcAccessMask       = vk::AccessFlagBits2::eTransferWrite,
                .dstStageMask        = vk::PipelineStageFlagBits2::eFragmentShader,
                .dstAccessMask       = vk::AccessFlagBits2::eShaderSampledRead,
                .oldLayout           = vk::ImageLayout::eTransferDstOptimal,
                .newLayout           = vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = tex.m_image,
                .subresourceRange =
                    vk::ImageSubresourceRange{
                        .aspectMask     = vk::ImageAspectFlagBits::eColor,
                        .baseMipLevel   = 0,
                        .levelCount     = 1,
                        .baseArrayLayer = 0,
                        .layerCount     = 1,
                    },
            };

            vk::DependencyInfo dep{
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers    = &barrier,
            };
            command_buffer.pipelineBarrier2(dep);
        }

        // I think we might need multiple image views for some cases (e.g. different view of the same image for different
        // shader stages, or different view of the same image with different format), but for this simple case we just create
        // one image view. 6) Image view
        {
            vk::ImageViewCreateInfo view_ci{
                .image    = tex.m_image,
                .viewType = vk::ImageViewType::e2D,
                .format   = tex.m_format,
                .subresourceRange =
                    vk::ImageSubresourceRange{
                        .aspectMask     = vk::ImageAspectFlagBits::eColor,
                        .baseMipLevel   = 0,
                        .levelCount     = 1,
                        .baseArrayLayer = 0,
                        .layerCount     = 1,
                    },
            };
            tex.m_view = device.createImageView(view_ci);
        }

        m_textures[name] = tex;
    }

    void Resource_manager::create_linear_wrap_sampler()
    {
        vk::SamplerCreateInfo sampler_ci{
            .magFilter               = vk::Filter::eLinear,
            .minFilter               = vk::Filter::eLinear,
            .mipmapMode              = vk::SamplerMipmapMode::eLinear,
            .addressModeU            = vk::SamplerAddressMode::eRepeat,
            .addressModeV            = vk::SamplerAddressMode::eRepeat,
            .addressModeW            = vk::SamplerAddressMode::eRepeat,
            .mipLodBias              = 0.0f,
            .anisotropyEnable        = VK_FALSE,
            .maxAnisotropy           = 1.0f,
            .compareEnable           = VK_FALSE,
            .compareOp               = vk::CompareOp::eAlways,
            .minLod                  = 0.0f,
            .maxLod                  = 0.0f,
            .borderColor             = vk::BorderColor::eIntOpaqueBlack,
            .unnormalizedCoordinates = VK_FALSE,
        };

        auto&& sampler = m_gfx_device.m_device.createSampler(sampler_ci);

        // add to map
        m_samplers["s_linear_wrap"] = sampler;
    }

    Texture Resource_manager::get_texture(const std::string& name) const
    {
        auto it = m_textures.find(name);
        assert(it != m_textures.end());
        return it->second;
    }

    vk::Sampler Resource_manager::get_sampler(const std::string& name) const
    {
        auto it = m_samplers.find(name);
        assert(it != m_samplers.end());
        return it->second;
    }

} // namespace VKN
