#include "technique.h"

#include "device.h"
#include "shader.h"
#include "shader_manager.h"
#include <sstream>
#include <stdexcept>

namespace VKN {

    void Technique::destroy()
    {
        auto&& device = m_gfx_device.m_device;

        if (m_sbt_buffer) {
            device.destroyBuffer(m_sbt_buffer);
            m_sbt_buffer = nullptr;
        }
        if (m_sbt_allocation) {
            m_gfx_device.m_vma_allocator.freeMemory(m_sbt_allocation);
            m_sbt_allocation = nullptr;
        }

        device.destroyPipeline(m_pipeline);
        device.destroyPipelineLayout(m_pipeline_layout);

        for (auto&& d : m_descriptorset_layouts) {
            device.destroyDescriptorSetLayout(d);
        }
        m_descriptorset_layouts.clear();

        m_reflected_binding_map.clear();
    }

    void Technique::create_raytracing_pipeline()
    {
        // Key learning points in comments:
        // Steps 1-4 are pipeline creation (similar to compute/graphics)
        // Steps 5-6 handle GPU alignment quirks (different hardware has different requirements)
        // Steps 7-9 are the SBT-specific part: get handles → allocate buffer → write with spacing
        // Steps 10-11 convert the buffer into GPU-accessible regions that traceRaysKHR will use

        auto&& device = m_gfx_device.m_device;
        auto&& ray    = m_ray_lib_handle.lock();
        if (!ray) {
            throw std::runtime_error("Raytracing pipeline requires ray library shader handle");
        }

        // Step 1: Create descriptor set layout and pipeline layout from shader reflection
        create_descriptor_pipeline_layout();

        // Step 2: Define shader stages
        // Each stage represents one shader function from the ray library (raygen, miss, closesthit)
        std::vector<vk::PipelineShaderStageCreateInfo> stages;
        stages.push_back(vk::PipelineShaderStageCreateInfo{
            .stage  = vk::ShaderStageFlagBits::eRaygenKHR, // Ray generation shader (entry point)
            .module = ray->m_shader_module,
            .pName  = "raygen_main",
        });
        stages.push_back(vk::PipelineShaderStageCreateInfo{
            .stage  = vk::ShaderStageFlagBits::eMissKHR, // Miss shader (executed when ray doesn't hit)
            .module = ray->m_shader_module,
            .pName  = "miss_main",
        });
        stages.push_back(vk::PipelineShaderStageCreateInfo{
            .stage  = vk::ShaderStageFlagBits::eClosestHitKHR, // Closest-hit shader (executed at intersection)
            .module = ray->m_shader_module,
            .pName  = "closethit_main",
        });

        // Step 3: Define shader groups
        // Groups tell the GPU how to route rays to different shader functions.
        // Each group contains one or more shader stages and defines the entry point for that group type.
        std::vector<vk::RayTracingShaderGroupCreateInfoKHR> groups;

        // Group 0: Raygen - general type, just calls raygen_main
        groups.push_back(vk::RayTracingShaderGroupCreateInfoKHR{
            .type               = vk::RayTracingShaderGroupTypeKHR::eGeneral,
            .generalShader      = 0, // Index into stages array
            .closestHitShader   = VK_SHADER_UNUSED_KHR,
            .anyHitShader       = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
        });

        // Group 1: Miss - general type, just calls miss_main
        groups.push_back(vk::RayTracingShaderGroupCreateInfoKHR{
            .type               = vk::RayTracingShaderGroupTypeKHR::eGeneral,
            .generalShader      = 1, // Index into stages array
            .closestHitShader   = VK_SHADER_UNUSED_KHR,
            .anyHitShader       = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
        });

        // Group 2: Triangle hit group - contains closest-hit shader for triangle intersections
        groups.push_back(vk::RayTracingShaderGroupCreateInfoKHR{
            .type               = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
            .generalShader      = VK_SHADER_UNUSED_KHR,
            .closestHitShader   = 2, // Index into stages array (closethit_main)
            .anyHitShader       = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
        });

        // Step 4: Create raytracing pipeline
        // The pipeline links stages, groups, and layout together
        vk::RayTracingPipelineCreateInfoKHR rt_ci{
            .stageCount                   = static_cast<uint32_t>(stages.size()),
            .pStages                      = stages.data(),
            .groupCount                   = static_cast<uint32_t>(groups.size()),
            .pGroups                      = groups.data(),
            .maxPipelineRayRecursionDepth = 1, // No recursive ray tracing (ray bounces)
            .layout                       = m_pipeline_layout,
        };

        m_pipeline   = device.createRayTracingPipelineKHR({}, {}, rt_ci).value;
        m_bind_point = vk::PipelineBindPoint::eRayTracingKHR;

        // Step 5: Query GPU capabilities for shader binding table (SBT) alignment and sizing
        // Different GPUs have different alignment requirements for SBT entries
        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rt_props{};
        vk::PhysicalDeviceProperties2 props2{};
        props2.pNext = &rt_props;
        m_gfx_device.m_physical_device.getProperties2(&props2);

        // Helper lambda for alignment calculation: rounds up v to nearest multiple of a
        auto align_up = [](uint32_t v, uint32_t a) -> uint32_t { return (v + a - 1) & ~(a - 1); };

        // Step 6: Calculate SBT buffer layout and sizing
        // Each group gets an entry in the SBT with proper GPU alignment
        const uint32_t group_count = static_cast<uint32_t>(groups.size());
        const uint32_t handle_size = rt_props.shaderGroupHandleSize;
        // Calculate alignment within the group
        const uint32_t handle_size_aligned = align_up(handle_size, rt_props.shaderGroupHandleAlignment);
        // Calculate alignment between group
        const uint32_t sbt_stride = align_up(handle_size_aligned, rt_props.shaderGroupBaseAlignment);
        const uint32_t sbt_size   = sbt_stride * group_count;

        // Step 7: Get shader group handles from the pipeline
        // These are opaque binary blobs that represent the compiled shader groups
        std::vector<uint8_t> handles(group_count * handle_size);
        vk::Result result = m_gfx_device.m_device.getRayTracingShaderGroupHandlesKHR(m_pipeline, 0, group_count, handles.size(), handles.data());
        if (result != vk::Result::eSuccess) {
            assert("Failed to get ray tracing shader group handles");
        }

        // Step 8: Create GPU buffer for the SBT
        // This buffer will contain the shader handles with proper alignment
        vk::BufferCreateInfo sbt_ci{
            .size        = sbt_size,
            .usage       = vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            .sharingMode = vk::SharingMode::eExclusive,
        };

        // Allocate host-visible memory so we can write handles to it
        vma::AllocationCreateInfo sbt_alloc_ci{};
        sbt_alloc_ci.setUsage(vma::MemoryUsage::eAutoPreferHost);
        sbt_alloc_ci.setFlags(
            vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped);

        vma::AllocationInfo sbt_alloc_info{};
        std::tie(m_sbt_allocation, m_sbt_buffer) =
            m_gfx_device.m_vma_allocator.createBuffer(sbt_ci, sbt_alloc_ci, sbt_alloc_info);

        // Step 9: Write shader handles into the SBT buffer with proper alignment
        // Each group's handle is placed at offset (sbt_stride * group_index)
        uint8_t* mapped = reinterpret_cast<uint8_t*>(sbt_alloc_info.pMappedData);
        std::memset(mapped, 0, sbt_size); // Clear buffer first

        // Copy each group's handle to its aligned offset
        std::memcpy(mapped + sbt_stride * 0, handles.data() + handle_size * 0, handle_size); // Group 0 (raygen)
        std::memcpy(mapped + sbt_stride * 1, handles.data() + handle_size * 1, handle_size); // Group 1 (miss)
        std::memcpy(mapped + sbt_stride * 2, handles.data() + handle_size * 2, handle_size); // Group 2 (hit)

        // Step 10: Get GPU device address of the SBT buffer
        // This is used to reference the buffer during ray tracing dispatch
        vk::DeviceAddress sbt_addr = device.getBufferAddress(vk::BufferDeviceAddressInfo{.buffer = m_sbt_buffer});

        // Step 11: Create strided device address regions for each group type
        // These regions tell traceRaysKHR where to find the shader entry points
        m_sbt_raygen_region = vk::StridedDeviceAddressRegionKHR{
            .deviceAddress = sbt_addr + sbt_stride * 0, // Points to group 0
            .stride        = sbt_stride,                // Space between entries (if multiple)
            .size          = sbt_stride,                // Size of this region
        };
        m_sbt_miss_region = vk::StridedDeviceAddressRegionKHR{
            .deviceAddress = sbt_addr + sbt_stride * 1, // Points to group 1
            .stride        = sbt_stride,
            .size          = sbt_stride,
        };
        m_sbt_hit_region = vk::StridedDeviceAddressRegionKHR{
            .deviceAddress = sbt_addr + sbt_stride * 2, // Points to group 2
            .stride        = sbt_stride,
            .size          = sbt_stride,
        };

        // Callable region is empty (we don't use callable shaders)
        m_sbt_callable_region = vk::StridedDeviceAddressRegionKHR{};
    }

    void Technique::create_compute_pipeline()
    {
        auto&& device = m_gfx_device.m_device;

        auto&& cs = m_cs_handle.lock();

        // Programable state -----------
        vk::PipelineShaderStageCreateInfo pipeline_shader_stage_createinfo{
            .stage = vk::ShaderStageFlagBits::eCompute, .module = cs->m_shader_module, .pName = "csmain"};

        // Create a pipeline layout from Shader stages
        // Descriptor set layout + reflected binding map + pipeline layout
        create_descriptor_pipeline_layout();
        // ----------

        vk::ComputePipelineCreateInfo compute_pipeline_create_info{
            .stage  = pipeline_shader_stage_createinfo,
            .layout = m_pipeline_layout,
        };

        m_pipeline   = device.createComputePipeline({}, compute_pipeline_create_info).value;
        m_bind_point = vk::PipelineBindPoint::eCompute;
    }

    void Technique::create_pipeline(vk::Format color_format, vk::Format depth_format)
    {
        auto&& device = m_gfx_device.m_device;

        auto&& ms = m_ms_handle.lock();
        auto&& vs = m_vs_handle.lock();
        auto&& ps = m_ps_handle.lock();

        // Programable state -----------
        const bool wants_vs = has_stage(m_raster_stages, Raster_stage_mask::VS);
        const bool wants_ms = has_stage(m_raster_stages, Raster_stage_mask::MS);
        const bool wants_ps = has_stage(m_raster_stages, Raster_stage_mask::PS);

        if (!wants_ps) {
            throw std::runtime_error("Graphics pipeline requires PS stage in stage mask");
        }

        if (wants_vs == wants_ms) {
            throw std::runtime_error("Graphics pipeline requires exactly one of VS or MS in stage mask");
        }

        if (wants_vs && !vs) {
            throw std::runtime_error("Graphics pipeline declared VS stage but VS shader handle is missing");
        }

        if (wants_ms && !ms) {
            throw std::runtime_error("Graphics pipeline declared MS stage but MS shader handle is missing");
        }

        if (wants_ps && !ps) {
            throw std::runtime_error("Graphics pipeline declared PS stage but PS shader handle is missing");
        }

        std::vector<vk::PipelineShaderStageCreateInfo> pipeline_shader_stages;
        vk::PipelineVertexInputStateCreateInfo pipeline_vertex_input_state_createinfo;

        if (wants_ms) {
            // Mesh shader path: MS + PS
            pipeline_shader_stages.push_back(vk::PipelineShaderStageCreateInfo{
                .stage = vk::ShaderStageFlagBits::eMeshEXT, .module = ms->m_shader_module, .pName = "msmain"});
            // No vertex input state for mesh shaders
        }
        else {
            // Vertex shader path: VS + PS
            pipeline_shader_stages.push_back(vk::PipelineShaderStageCreateInfo{
                .stage = vk::ShaderStageFlagBits::eVertex, .module = vs->m_shader_module, .pName = "vsmain"});

            // Setup vertex input only for vertex shaders
            if (vs->m_vertex_input_attribute_descriptions.size() > 0) {
                pipeline_vertex_input_state_createinfo = {
                    .vertexBindingDescriptionCount   = 1,
                    .pVertexBindingDescriptions      = &vs->m_vertex_input_binding_description,
                    .vertexAttributeDescriptionCount = (uint32_t)vs->m_vertex_input_attribute_descriptions.size(),
                    .pVertexAttributeDescriptions    = vs->m_vertex_input_attribute_descriptions.data(),
                };
            }
        }

        pipeline_shader_stages.push_back(vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eFragment, .module = ps->m_shader_module, .pName = "psmain"});

        // Create a pipeline layout from Shader stages
        // Descriptor set layout + reflected binding map + pipeline layout
        create_descriptor_pipeline_layout();
        // ----------

        // Fixed pipeline state -----------
        vk::PipelineInputAssemblyStateCreateInfo pipeline_input_assembly_state_createinfo{
            .topology = vk::PrimitiveTopology::eTriangleList};

        vk::PipelineRasterizationStateCreateInfo pipeline_rasterization_state_createinfo{
            .flags                   = vk::PipelineRasterizationStateCreateFlags(),
            .depthClampEnable        = false,                       // depthClampEnable
            .rasterizerDiscardEnable = false,                       // rasterizerDiscardEnable
            .polygonMode             = vk::PolygonMode::eFill,      // polygonMode
            .cullMode                = vk::CullModeFlagBits::eNone, // cullMode
            .frontFace               = vk::FrontFace::eClockwise,   // frontFace
            .depthBiasEnable         = false,                       // depthBiasEnable
            .depthBiasConstantFactor = 0.0f,                        // depthBiasConstantFactor
            .depthBiasClamp          = 0.0f,                        // depthBiasClamp
            .depthBiasSlopeFactor    = 0.0f,                        // depthBiasSlopeFactor
            .lineWidth               = 1.0f,                        // lineWidth
        };

        vk::PipelineMultisampleStateCreateInfo pipeline_multisample_state_createinfo{
            .flags                = vk::PipelineMultisampleStateCreateFlags(), // flags
            .rasterizationSamples = vk::SampleCountFlagBits::e1,               // rasterizationSamples
                                                                               // other values can be default
        };

        vk::StencilOpState stencil_op_state(
            vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::CompareOp::eAlways);
        vk::PipelineDepthStencilStateCreateInfo pipeline_depth_stencil_state_createinfo{
            .flags                 = vk::PipelineDepthStencilStateCreateFlags(), // flags
            .depthTestEnable       = true,                                       // depthTestEnable
            .depthWriteEnable      = true,                                       // depthWriteEnable
            .depthCompareOp        = vk::CompareOp::eLessOrEqual,                // depthCompareOp
            .depthBoundsTestEnable = false,                                      // depthBoundTestEnable
            .stencilTestEnable     = false,                                      // stencilTestEnable
            .front                 = stencil_op_state,                           // front
            .back                  = stencil_op_state,                           // back
        };

        vk::ColorComponentFlags color_component_flags(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                                      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
        vk::PipelineColorBlendAttachmentState pipeline_color_blend_attachment_state(false, // blendEnable
            vk::BlendFactor::eZero,                                                        // srcColorBlendFactor
            vk::BlendFactor::eZero,                                                        // dstColorBlendFactor
            vk::BlendOp::eAdd,                                                             // colorBlendOp
            vk::BlendFactor::eZero,                                                        // srcAlphaBlendFactor
            vk::BlendFactor::eZero,                                                        // dstAlphaBlendFactor
            vk::BlendOp::eAdd,                                                             // alphaBlendOp
            color_component_flags                                                          // colorWriteMask
        );

        vk::PipelineColorBlendStateCreateInfo pipeline_color_blend_state_createinfo{
            .flags           = vk::PipelineColorBlendStateCreateFlags(), // flags
            .logicOpEnable   = false,                                    // logicOpEnable
            .logicOp         = vk::LogicOp::eNoOp,                       // logicOp
            .attachmentCount = 1,
            .pAttachments    = &pipeline_color_blend_attachment_state, // attachments
            .blendConstants  = {{1.0f, 1.0f, 1.0f, 1.0f}}              // blendConstants
        };

        const int num_render_targets = 1;

        vk::PipelineViewportStateCreateInfo pipeline_viewport_state_createinfo{
            .flags         = vk::PipelineViewportStateCreateFlags(),
            .viewportCount = num_render_targets,
            .scissorCount  = num_render_targets,
        };

        vk::PipelineRenderingCreateInfo render_info{
            .colorAttachmentCount    = num_render_targets,
            .pColorAttachmentFormats = &color_format,
            .depthAttachmentFormat   = depth_format,
        };

        // Dynamic states
        std::array<vk::DynamicState, 2> dynamic_states = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor,
            //vk::DynamicState::eCullMode,
            // vk::DynamicState::eFrontFace,
            // vk::DynamicState::ePrimitiveTopology,
        };

        vk::PipelineDynamicStateCreateInfo pipeline_dynamic_state_createinfo{
            .dynamicStateCount = (uint32_t)dynamic_states.size(),
            .pDynamicStates    = dynamic_states.data(),
        };
        // -----------

        vk::GraphicsPipelineCreateInfo graphics_pipeline_createinfo{
            .pNext               = &render_info,
            .flags               = vk::PipelineCreateFlags(),               // flags
            .stageCount          = (uint32_t)pipeline_shader_stages.size(), // stages
            .pStages             = pipeline_shader_stages.data(),
            .pVertexInputState   = wants_ms ? nullptr : &pipeline_vertex_input_state_createinfo,
            .pInputAssemblyState = wants_ms ? nullptr : &pipeline_input_assembly_state_createinfo,
            .pTessellationState  = nullptr,                                  // pTessellationState
            .pViewportState      = &pipeline_viewport_state_createinfo,      // pViewportState
            .pRasterizationState = &pipeline_rasterization_state_createinfo, // pRasterizationState
            .pMultisampleState   = &pipeline_multisample_state_createinfo,   // pMultisampleState
            .pDepthStencilState  = &pipeline_depth_stencil_state_createinfo, // pDepthStencilState
            .pColorBlendState    = &pipeline_color_blend_state_createinfo,   // pColorBlendState
            .pDynamicState       = &pipeline_dynamic_state_createinfo,       // pDynamicState
            .layout              = m_pipeline_layout,                        // layout
            .renderPass          = nullptr, // renderPass, and since we are using dynamic rendering this will set as null
            .subpass             = 0,
        };

        vk::Result result;
        vk::Pipeline pipeline;
        std::tie(result, pipeline) = device.createGraphicsPipeline(nullptr, graphics_pipeline_createinfo);
        switch (result) {
        case vk::Result::eSuccess:
            break;
        case vk::Result::ePipelineCompileRequiredEXT:
            // something meaningful here
            break;
        default:
            assert(false); // should never happen
        }

        m_pipeline   = pipeline;
        m_bind_point = vk::PipelineBindPoint::eGraphics;
    }

    const Reflected_descriptor_binding* Technique::find_binding(const std::string& reflected_name) const
    {
        auto itr = m_reflected_binding_map.find(reflected_name);
        if (itr == m_reflected_binding_map.end()) {
            return nullptr;
        }
        return &itr->second;
    }

    void Technique::create_descriptor_pipeline_layout()
    {
        auto&& device = m_gfx_device.m_device;

        auto&& ms = m_ms_handle.lock();
        auto&& vs = m_vs_handle.lock();
        auto&& ps = m_ps_handle.lock();
        auto&& cs = m_cs_handle.lock();
        auto&& ray = m_ray_lib_handle.lock();

        // Collect and merge descriptor set layouts from VS and PS by set_number.
        // Bindings at the same (set, binding) slot have their stageFlags OR-ed together.
        struct Merged_set_binding {
            vk::DescriptorSetLayoutBinding binding{};
            vk::DescriptorBindingFlags binding_flag{};
            std::string reflected_name;
        };

        struct Merged_set {
            uint32_t set_number = 0;
            std::vector<Merged_set_binding> entries;
        };
        std::map<uint32_t, Merged_set> merged_sets; // ordered by set_number

        auto collect_stage = [&](const Shader& shader) {
            for (const auto& layout_data : shader.m_descriptorset_layoutdata) {
                auto& merged      = merged_sets[layout_data.set_number];
                merged.set_number = layout_data.set_number;

                for (size_t i = 0; i < layout_data.bindings.size(); ++i) {
                    const auto& src_binding = layout_data.bindings[i];
                    const auto& src_name    = layout_data.binding_names[i];
                    const auto src_flag     = layout_data.binding_flags[i];

                    bool found = false;
                    for (auto& entry : merged.entries) {
                        if (entry.binding.binding == src_binding.binding) {

                            const bool same_name  = (entry.reflected_name == src_name);
                            const bool same_type  = (entry.binding.descriptorType == src_binding.descriptorType);
                            const bool same_count = (entry.binding.descriptorCount == src_binding.descriptorCount);
                            const bool same_flags = (entry.binding_flag == src_flag);

                            // Hard error on incompatible declarations in the same (set,binding) slot.
                            if (!same_type || !same_count || !same_flags) {
                                std::ostringstream oss;
                                oss << "Descriptor collision (incompatible) at set " << layout_data.set_number
                                    << ", binding " << src_binding.binding << ". "
                                    << "Existing{type=" << vk::to_string(entry.binding.descriptorType)
                                    << ", count=" << entry.binding.descriptorCount
                                    << ", flags=" << static_cast<uint32_t>(entry.binding_flag) << "} "
                                    << "Incoming{type=" << vk::to_string(src_binding.descriptorType)
                                    << ", count=" << src_binding.descriptorCount
                                    << ", flags=" << static_cast<uint32_t>(src_flag) << "} "
                                    << "IncomingName=" << src_name;
                                throw std::runtime_error(oss.str());
                            }

                            // Hard error on aliasing different names to the same slot unless intentionally shared.
                            // Current policy: only the exact same reflected name may share a slot across stages.
                            if (!same_name) {
                                std::ostringstream oss;
                                oss << "Descriptor collision (ambiguous names) at set " << layout_data.set_number
                                    << ", binding " << src_binding.binding << ". ExistingName=" << entry.reflected_name;
                                throw std::runtime_error(oss.str());
                            }

                            found = true;
                            entry.binding.stageFlags |= src_binding.stageFlags;
                            break;
                        }
                    }

                    if (!found) {
                        Merged_set_binding new_entry{};
                        new_entry.binding        = src_binding;
                        new_entry.binding_flag   = src_flag;
                        new_entry.reflected_name = src_name;
                        merged.entries.emplace_back(std::move(new_entry));
                    }
                }
            }
        };

        // todo : refactor and make it easier to read
        if (ms) {
            collect_stage(*ms);
        }
        if (vs) {
            collect_stage(*vs);
        }
        if (ps) {
            collect_stage(*ps);
        }
        if (cs) {
            collect_stage(*cs);
        }
        if (ray) {
            collect_stage(*ray);
        }

        m_reflected_binding_map.clear();
        m_descriptorset_layouts.clear();
        m_descriptorset_infos.clear();

        if (!merged_sets.empty()) {

            uint32_t max_set_number = 0;
            for (const auto& kv : merged_sets) {
                max_set_number = std::max(max_set_number, kv.first);
            }

            m_descriptorset_layouts.resize(max_set_number + 1);

            auto create_empty_layout = [&device]() {
                vk::DescriptorSetLayoutCreateInfo ci{};
                return device.createDescriptorSetLayout(ci);
            };

            for (uint32_t set = 0; set <= max_set_number; ++set) {
                m_descriptorset_layouts[set] = create_empty_layout();
            }

            for (auto& [set_number, merged] : merged_sets) {
                std::vector<vk::DescriptorSetLayoutBinding> merged_bindings;
                std::vector<vk::DescriptorBindingFlags> merged_flags;
                merged_bindings.reserve(merged.entries.size());
                merged_flags.reserve(merged.entries.size());

                for (const auto& entry : merged.entries) {
                    merged_bindings.push_back(entry.binding);
                    merged_flags.push_back(entry.binding_flag);
                }

                vk::DescriptorSetLayoutBindingFlagsCreateInfo flags_info{
                    .bindingCount  = (uint32_t)merged_flags.size(),
                    .pBindingFlags = merged_flags.data(),
                };

                vk::DescriptorSetLayoutCreateInfo set_ci{
                    .pNext        = &flags_info,
                    .bindingCount = (uint32_t)merged_bindings.size(),
                    .pBindings    = merged_bindings.data(),
                };

                device.destroyDescriptorSetLayout(m_descriptorset_layouts[set_number]);
                m_descriptorset_layouts[set_number] = device.createDescriptorSetLayout(set_ci);

                for (const auto& entry : merged.entries) {
                    Reflected_descriptor_binding new_binding{.m_set_number = set_number,
                        .m_binding_number                                  = entry.binding.binding,
                        .m_set_layout_index = set_number, // can be 0 I believed, only use for error checking
                        .m_descriptor_type  = entry.binding.descriptorType,
                        .m_descriptor_count = entry.binding.descriptorCount,
                        .m_is_variable_descriptor_count =
                            (entry.binding_flag & vk::DescriptorBindingFlagBits::eVariableDescriptorCount) !=
                            vk::DescriptorBindingFlags{}};

                    auto [it, inserted] = m_reflected_binding_map.emplace(entry.reflected_name, new_binding);
                    if (!inserted) {
                        const auto& old_binding = it->second;

                        const bool same_slot = old_binding.m_set_number == new_binding.m_set_number &&
                                               old_binding.m_binding_number == new_binding.m_binding_number &&
                                               old_binding.m_set_layout_index == new_binding.m_set_layout_index;

                        const bool same_desc = old_binding.m_descriptor_type == new_binding.m_descriptor_type &&
                                               old_binding.m_descriptor_count == new_binding.m_descriptor_count;

                        if (!same_slot || !same_desc) {
                            std::ostringstream oss;
                            oss << "Reflected name collision for '" << entry.reflected_name << "'. "
                                << "Existing{set=" << old_binding.m_set_number
                                << ", binding=" << old_binding.m_binding_number
                                << ", layoutIndex=" << old_binding.m_set_layout_index
                                << ", type=" << vk::to_string(old_binding.m_descriptor_type)
                                << ", count=" << old_binding.m_descriptor_count
                                << "} Incoming{set=" << new_binding.m_set_number
                                << ", binding=" << new_binding.m_binding_number
                                << ", layoutIndex=" << new_binding.m_set_layout_index
                                << ", type=" << vk::to_string(new_binding.m_descriptor_type)
                                << ", count=" << new_binding.m_descriptor_count << "}";
                            throw std::runtime_error(oss.str());
                        }
                    }
                }
            }

            vk::PipelineLayoutCreateInfo pipeline_layout_createinfo{
                .setLayoutCount = (uint32_t)m_descriptorset_layouts.size(),
                .pSetLayouts    = m_descriptorset_layouts.data(),
            };
            m_pipeline_layout = device.createPipelineLayout(pipeline_layout_createinfo);
        }
        else {
            m_pipeline_layout = device.createPipelineLayout(vk::PipelineLayoutCreateInfo{});
        }
    }

} // namespace VKN
