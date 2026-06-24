#include "resource_manager.h"

#include "common/common_cpp.h"

#include "device.h"
#include "vma/vma.h"

namespace VKN {
    void Resource_manager::destroy()
    {
        destroy_buffer(m_vertex_buffer);
        destroy_buffer(m_index_buffer);

        // upload scratch allocator lifetime is tied to submit batch; cleaned up by allocator
        m_upload_scratch_allocator.destroy();

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

        for (auto&& [name, buffer] : m_storage_buffers) {
            destroy_buffer(buffer);
        }
        m_storage_buffers.clear();

        for (auto& [name, tlas] : m_tlas_map) {
            if (tlas.m_accel_struct.m_accel_struct) {
                m_gfx_device.m_device.destroyAccelerationStructureKHR(tlas.m_accel_struct.m_accel_struct);
            }
            if (tlas.m_accel_struct.m_allocation) {
                m_gfx_device.m_vma_allocator.freeMemory(tlas.m_accel_struct.m_allocation);
            }
        }
        m_tlas_map.clear();

        for (auto& [name, blas] : m_blas_map) {
            if (blas.m_accel_struct.m_accel_struct) {
                m_gfx_device.m_device.destroyAccelerationStructureKHR(blas.m_accel_struct.m_accel_struct);
            }
            if (blas.m_accel_struct.m_allocation) {
                m_gfx_device.m_vma_allocator.freeMemory(blas.m_accel_struct.m_allocation);
            }
        }
        m_blas_map.clear();
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

        // 1. create staging buffer from upload scratch allocator and copy src data to it
        auto alloc                 = m_upload_scratch_allocator.allocate_and_copy(data, size, 16);
        vk::Buffer& staging_buffer = alloc.m_buffer;

        // 2. create destination buffer
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
        auto&& copy_region = vk::BufferCopy(alloc.m_offset, 0, size);
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

        return Buffer{
            .m_buffer = buffer, .m_allocation = buffer_alloc, .m_size = static_cast<size_t>(buffer_alloc_info.size)};
    }

    void Resource_manager::create_storage_buffer(
        const std::string& name, const void* data, size_t size, vk::BufferUsageFlags additional_usage_flags)
    {
        if (!data || size == 0) {
            return;
        }

        const auto usage = vk::BufferUsageFlagBits::eStorageBuffer | additional_usage_flags;

        Buffer buffer = create_buffer(Buffer_create_info{
            .m_usage_flags = usage,
            .m_data        = data,
            .m_size        = size,
        });

        m_storage_buffers[name] = buffer;
    }

    Buffer Resource_manager::get_storage_buffer(const std::string& name) const
    {
        auto it = m_storage_buffers.find(name);
        assert(it != m_storage_buffers.end());
        return it->second;
    }

    void Resource_manager::create_texture(
        const std::string& name, const TextureData& texture_data, vk::ImageUsageFlags additional_usage_flags)
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
        auto alloc = m_upload_scratch_allocator.allocate_and_copy(texture_data.m_data.data(), size_in_bytes, 16);
        vk::Buffer staging_buffer{alloc.m_buffer};

        // 2) GPU image
        Texture tex{};
        tex.m_format = vk::Format::eR8G8B8A8Unorm;
        tex.m_width  = texture_data.m_width;
        tex.m_height = texture_data.m_height;
        {
            const vk::ImageUsageFlags image_usage =
                vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled | additional_usage_flags;

            vk::ImageCreateInfo image_ci{
                .imageType     = vk::ImageType::e2D,
                .format        = tex.m_format,
                .extent        = vk::Extent3D{tex.m_width, tex.m_height, 1},
                .mipLevels     = 1,
                .arrayLayers   = 1,
                .samples       = vk::SampleCountFlagBits::e1,
                .tiling        = vk::ImageTiling::eOptimal,
                .usage         = image_usage,
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
                .bufferOffset      = alloc.m_offset,
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
                .imageExtent = vk::Extent3D{tex.m_width, tex.m_height, 1},
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

    VKN::BLAS Resource_manager::get_blas(const std::string& name) const
    {
        auto it = m_blas_map.find(name);
        assert(it != m_blas_map.end());
        return it->second;
    }

    VKN::TLAS Resource_manager::get_tlas(const std::string& name) const
    {
        auto it = m_tlas_map.find(name);
        assert(it != m_tlas_map.end());
        return it->second;
    }

    VKN::BLAS Resource_manager::build_blas_from_buffers(const std::string& name,
        const void* triangle_indices,
        uint32_t triangle_count,
        const void* vertex_positions,
        uint32_t vertex_count)
    {
        auto& vk_device       = m_gfx_device.m_device;
        auto& allocator       = m_gfx_device.m_vma_allocator;
        auto&& command_buffer = m_gfx_device.m_single_use_command_buffer;

        // Step 1: Create GPU buffers for geometry
        // Todo: replace a fixed geometry with scene geometry, and replace a fixed vertex/index buffer with a dynamic one
        // ========================================
        // Triangle indices: 3 indices per triangle
        size_t index_buffer_size = triangle_count * 3 * sizeof(uint32_t);
        Buffer_create_info index_buffer_info{
            .m_usage_flags = vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
                             vk::BufferUsageFlagBits::eShaderDeviceAddress,
            .m_data        = triangle_indices,
            .m_size        = index_buffer_size,
        };
        auto index_buffer      = create_buffer(index_buffer_info);
        auto index_device_addr = vk_device.getBufferAddress(vk::BufferDeviceAddressInfo{.buffer = index_buffer.m_buffer});

        // Vertex positions: float3 (12 bytes) per vertex
        size_t vertex_buffer_size = vertex_count * sizeof(float) * 3;
        Buffer_create_info vertex_buffer_info{
            .m_usage_flags = vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
                             vk::BufferUsageFlagBits::eShaderDeviceAddress,
            .m_data        = vertex_positions,
            .m_size        = vertex_buffer_size,
        };
        auto vertex_buffer      = create_buffer(vertex_buffer_info);
        auto vertex_device_addr = vk_device.getBufferAddress(vk::BufferDeviceAddressInfo{.buffer = vertex_buffer.m_buffer});

        // Step 2: Describe triangle geometry for BLAS
        // ===========================================
        // VkAccelerationStructureGeometryTrianglesDataKHR describes triangle layout
        vk::AccelerationStructureGeometryTrianglesDataKHR triangles_data{
            .sType         = vk::StructureType::eAccelerationStructureGeometryTrianglesDataKHR,
            .vertexFormat  = vk::Format::eR32G32B32Sfloat, // float3 vertices
            .vertexData    = {.deviceAddress = vertex_device_addr},
            .vertexStride  = sizeof(float) * 3, // Stride between vertices
            .maxVertex     = vertex_count - 1,
            .indexType     = vk::IndexType::eUint32, // uint32 indices
            .indexData     = {.deviceAddress = index_device_addr},
            .transformData = {.deviceAddress = 0}, // No per-triangle transforms
        };

        // Wrap triangle data in geometry structure
        vk::AccelerationStructureGeometryKHR geometry{
            .sType        = vk::StructureType::eAccelerationStructureGeometryKHR,
            .geometryType = vk::GeometryTypeKHR::eTriangles,
            .geometry     = {.triangles = triangles_data},
            .flags        = vk::GeometryFlagBitsKHR::eOpaque,
        };

        // Step 3: Query build sizes
        // =========================
        // Ask GPU: "How much memory do I need to build BLAS with this geometry?"
        vk::AccelerationStructureBuildGeometryInfoKHR build_info{
            .sType         = vk::StructureType::eAccelerationStructureBuildGeometryInfoKHR,
            .type          = vk::AccelerationStructureTypeKHR::eBottomLevel,              // BLAS (not TLAS)
            .flags         = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace, // Optimize for ray tracing speed
            .geometryCount = 1,
            .pGeometries   = &geometry,
        };

        vk::AccelerationStructureBuildSizesInfoKHR build_sizes =
            vk_device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice,
                build_info,
                triangle_count); // Pass triangle count, not vertex count

        // Step 4: Create BLAS buffer and structure object
        // ===============================================
        vma::AllocationCreateInfo blas_alloc_info{
            .usage = vma::MemoryUsage::eGpuOnly,
        };

        vk::BufferCreateInfo blas_buffer_info{
            .size = build_sizes.accelerationStructureSize,
            .usage =
                vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        };

        auto [blas_allocation, blas_buffer] = allocator.createBuffer(blas_buffer_info, blas_alloc_info);

        // Create the acceleration structure handle
        vk::AccelerationStructureCreateInfoKHR as_create_info{
            .buffer = blas_buffer,
            .offset = 0,
            .size   = build_sizes.accelerationStructureSize,
            .type   = vk::AccelerationStructureTypeKHR::eBottomLevel,
        };

        auto blas_handle = vk_device.createAccelerationStructureKHR(as_create_info);

        // Step 5: Allocate scratch buffer for build
        // =========================================
        // GPU acceleration structure builds need temporary scratch space
        vma::AllocationCreateInfo scratch_alloc_info{
            .usage = vma::MemoryUsage::eGpuOnly,
        };

        vk::BufferCreateInfo scratch_buffer_info{
            .size  = build_sizes.buildScratchSize,
            .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        };

        auto [scratch_allocation, scratch_buffer] = allocator.createBuffer(scratch_buffer_info, scratch_alloc_info);

        auto scratch_device_addr = vk_device.getBufferAddress(vk::BufferDeviceAddressInfo{.buffer = scratch_buffer});

        // Step 6: Record and submit build command
        // =======================================

        // Update build_info to point to allocated structures
        build_info.dstAccelerationStructure  = blas_handle;
        build_info.scratchData.deviceAddress = scratch_device_addr;

        vk::AccelerationStructureBuildRangeInfoKHR build_range_info{
            .primitiveCount  = triangle_count, // for BLAS; use instance_count for TLAS
            .primitiveOffset = 0,
            .firstVertex     = 0,
            .transformOffset = 0,
        };

        // Record build command (executes on GPU)
        command_buffer.buildAccelerationStructuresKHR(build_info, &build_range_info);

        // Ensure BLAS is built before use
        vk::MemoryBarrier2 barrier{
            .srcStageMask  = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
            .srcAccessMask = vk::AccessFlagBits2::eAccelerationStructureWriteKHR,
            .dstStageMask  = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
            .dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureReadKHR,
        };
        vk::DependencyInfo dep{
            .memoryBarrierCount = 1,
            .pMemoryBarriers    = &barrier,
        };

        command_buffer.pipelineBarrier2(dep);

        // Step 7: Get device address of BLAS (needed for TLAS)
        // ===================================================
        vk::AccelerationStructureDeviceAddressInfoKHR blas_addr_info{
            .accelerationStructure = blas_handle,
        };
        auto blas_device_address = vk_device.getAccelerationStructureAddressKHR(blas_addr_info);

        // Step 8: Store and return BLAS
        // =============================
        VKN::BLAS blas{
            .m_accel_struct =
                {
                    .m_accel_struct   = blas_handle,
                    .m_allocation     = blas_allocation,
                    .m_device_address = blas_device_address,
                },
            .m_name = name,
        };

        // Clean up temporary buffers (owned by create_buffer, will be freed)
        // In production, you'd track index_buffer and vertex_buffer for lifetime

        // Store BLAS in map
        m_blas_map[name] = blas;

        return blas;
    }

    VKN::TLAS Resource_manager::build_tlas_from_blas_instances(
        const std::string& name, const std::vector<std::pair<const VKN::BLAS*, VkTransformMatrixKHR>>& blas_instances)
    {
        auto& vk_device       = m_gfx_device.m_device;
        auto& allocator       = m_gfx_device.m_vma_allocator;
        auto&& command_buffer = m_gfx_device.m_single_use_command_buffer;

        // Step 1: Create instance buffer
        // ==============================
        // TLAS references BLASes through instances, each with a transform matrix
        std::vector<VkAccelerationStructureInstanceKHR> instances;
        instances.reserve(blas_instances.size());

        for (uint32_t i = 0; i < blas_instances.size(); ++i) {
            const auto& [blas, transform] = blas_instances[i];

            VkAccelerationStructureInstanceKHR instance{
                .transform                              = transform, // 3x4 transform matrix
                .instanceCustomIndex                    = i,         // User-defined instance ID (usable in shaders)
                .mask                                   = 0xFF,      // Visibility mask (0xFF = visible to all rays)
                .instanceShaderBindingTableRecordOffset = 0,         // Shader binding table offset (for complex shaders)
                .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,      // Don't cull back faces
                .accelerationStructureReference = blas->m_accel_struct.m_device_address, // Reference to BLAS
            };
            instances.push_back(instance);
        }

        // Upload instances to GPU buffer
        size_t instance_buffer_size = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);
        Buffer_create_info instance_buffer_info{
            .m_usage_flags = vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
                             vk::BufferUsageFlagBits::eShaderDeviceAddress,
            .m_data        = instances.data(),
            .m_size        = instance_buffer_size,
        };
        auto instance_buffer = create_buffer(instance_buffer_info);
        auto instance_device_addr =
            vk_device.getBufferAddress(vk::BufferDeviceAddressInfo{.buffer = instance_buffer.m_buffer});

        // Step 2: Describe instance geometry for TLAS
        // ===========================================
        vk::AccelerationStructureGeometryInstancesDataKHR instances_data{
            .arrayOfPointers = VK_FALSE, // Instances are in contiguous array (not array of pointers)
            .data            = {.deviceAddress = instance_device_addr},
        };

        vk::AccelerationStructureGeometryKHR geometry{
            .geometryType = vk::GeometryTypeKHR::eInstances,
            .geometry     = {.instances = instances_data},
            .flags        = vk::GeometryFlagBitsKHR::eOpaque,
        };

        // Step 3: Query TLAS build sizes
        // ==============================
        uint32_t instance_count = static_cast<uint32_t>(blas_instances.size());

        vk::AccelerationStructureBuildGeometryInfoKHR build_info{
            .type          = vk::AccelerationStructureTypeKHR::eTopLevel, // TLAS (not BLAS)
            .flags         = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace,
            .geometryCount = 1,
            .pGeometries   = &geometry,
        };

        vk::AccelerationStructureBuildSizesInfoKHR build_sizes = vk_device.getAccelerationStructureBuildSizesKHR(
            vk::AccelerationStructureBuildTypeKHR::eDevice, build_info, instance_count);

        // Step 4: Create TLAS buffer and structure
        // ========================================
        vma::AllocationCreateInfo tlas_alloc_info{
            .usage = vma::MemoryUsage::eGpuOnly,
        };

        vk::BufferCreateInfo tlas_buffer_info{
            .size = build_sizes.accelerationStructureSize,
            .usage =
                vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        };

        auto [tlas_allocation, tlas_buffer] = allocator.createBuffer(tlas_buffer_info, tlas_alloc_info);

        vk::AccelerationStructureCreateInfoKHR as_create_info{
            .buffer = tlas_buffer,
            .offset = 0,
            .size   = build_sizes.accelerationStructureSize,
            .type   = vk::AccelerationStructureTypeKHR::eTopLevel,
        };

        auto tlas_handle = vk_device.createAccelerationStructureKHR(as_create_info);

        // Step 5: Allocate scratch buffer
        // ===============================
        vma::AllocationCreateInfo scratch_alloc_info{
            .usage = vma::MemoryUsage::eGpuOnly,
        };

        vk::BufferCreateInfo scratch_buffer_info{
            .size  = build_sizes.buildScratchSize,
            .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        };

        auto [scratch_allocation, scratch_buffer] = allocator.createBuffer(scratch_buffer_info, scratch_alloc_info);

        auto scratch_device_addr = vk_device.getBufferAddress(vk::BufferDeviceAddressInfo{.buffer = scratch_buffer});

        // Step 6: Record and submit TLAS build
        // ===================================

        build_info.dstAccelerationStructure  = tlas_handle;
        build_info.scratchData.deviceAddress = scratch_device_addr;

        vk::AccelerationStructureBuildRangeInfoKHR build_range_info{
            .primitiveCount  = instance_count, // use instance_count for TLAS
            .primitiveOffset = 0,
            .firstVertex     = 0,
            .transformOffset = 0,
        };

        // Record build command (executes on GPU)
        command_buffer.buildAccelerationStructuresKHR(build_info, &build_range_info);

        // Memory barrier for TLAS build completion
        vk::MemoryBarrier2 barrier{
            .srcStageMask  = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
            .srcAccessMask = vk::AccessFlagBits2::eAccelerationStructureWriteKHR,
            .dstStageMask  = vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
            .dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureReadKHR,
        };
        vk::DependencyInfo dep{
            .memoryBarrierCount = 1,
            .pMemoryBarriers    = &barrier,
        };
        command_buffer.pipelineBarrier2(dep);

        // Step 7: Get TLAS device address
        // ==============================
        vk::AccelerationStructureDeviceAddressInfoKHR tlas_addr_info{
            .accelerationStructure = tlas_handle,
        };
        auto tlas_device_address = vk_device.getAccelerationStructureAddressKHR(tlas_addr_info);

        // Step 8: Return TLAS
        // ==================
        VKN::TLAS tlas{
            .m_accel_struct =
                {
                    .m_accel_struct   = tlas_handle,
                    .m_allocation     = tlas_allocation,
                    .m_device_address = tlas_device_address,
                },
            .m_name = name,
        };

        // Store TLAS in map
        m_tlas_map[name] = tlas;

        return tlas;
    }

} // namespace VKN
