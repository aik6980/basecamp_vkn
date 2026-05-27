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

        device.destroyPipeline(m_pipeline);
        device.destroyPipelineLayout(m_pipeline_layout);

        for (auto&& d : m_descriptorset_layouts) {
            device.destroyDescriptorSetLayout(d);
        }
        m_descriptorset_layouts.clear();

        m_reflected_binding_map.clear();
    }

    void Technique::create_pipeline(vk::Format color_format, vk::Format depth_format)
    {
        auto&& device = m_gfx_device.m_device;

        auto&& vs = mh_vs.lock();
        auto&& ps = mh_ps.lock();

        m_reflected_binding_map.clear();

        // Programable state -----------
        std::array<vk::PipelineShaderStageCreateInfo, 2> pipeline_shader_stage_createinfo = {
            vk::PipelineShaderStageCreateInfo{
                .stage = vk::ShaderStageFlagBits::eVertex, .module = vs->m_shader_module, .pName = "main"},
            vk::PipelineShaderStageCreateInfo{
                .stage = vk::ShaderStageFlagBits::eFragment, .module = ps->m_shader_module, .pName = "main"}};

        vk::PipelineVertexInputStateCreateInfo pipeline_vertex_input_state_createinfo;
        if (vs->m_vertex_input_attribute_descriptions.size() > 0) {
            pipeline_vertex_input_state_createinfo = {
                .vertexBindingDescriptionCount   = 1,
                .pVertexBindingDescriptions      = &vs->m_vertex_input_binding_description,
                .vertexAttributeDescriptionCount = (uint32_t)vs->m_vertex_input_attribute_descriptions.size(),
                .pVertexAttributeDescriptions    = vs->m_vertex_input_attribute_descriptions.data(),
            };
        }

        // Create a pipeline layout from Shader stages

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

        collect_stage(*vs);
        collect_stage(*ps);

        m_reflected_binding_map.clear();

        if (!merged_sets.empty()) {
            m_descriptorset_layouts.reserve(merged_sets.size());
            m_descriptorset_infos.clear(); // no longer populated; kept for compatibility

            uint32_t set_layout_index = 0;

            for (auto& [set_number, merged] : merged_sets) {
                std::vector<vk::DescriptorSetLayoutBinding> merged_bindings;
                std::vector<vk::DescriptorBindingFlags> merged_flags;

                merged_bindings.reserve(merged.entries.size());
                merged_flags.reserve(merged.entries.size());

                for (const auto& entry : merged.entries) {
                    merged_bindings.emplace_back(entry.binding);
                    merged_flags.emplace_back(entry.binding_flag);
                }

                vk::DescriptorSetLayoutBindingFlagsCreateInfo flags_info{
                    .bindingCount  = (uint32_t)merged_flags.size(),
                    .pBindingFlags = merged_flags.data(),
                };

                vk::DescriptorSetLayoutCreateInfo set_create_info{
                    .pNext        = &flags_info,
                    .bindingCount = (uint32_t)merged_bindings.size(),
                    .pBindings    = merged_bindings.data(),
                };

                m_descriptorset_layouts.emplace_back(device.createDescriptorSetLayout(set_create_info));

                for (const auto& entry : merged.entries) {
                    Reflected_descriptor_binding new_binding{
                        .m_set_number       = set_number,
                        .m_binding_number   = entry.binding.binding,
                        .m_set_layout_index = set_layout_index,
                        .m_descriptor_type  = entry.binding.descriptorType,
                        .m_descriptor_count = entry.binding.descriptorCount,
                    };

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

                ++set_layout_index;
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

        // ----------

        // Fixed pipeline state -----------
        vk::PipelineInputAssemblyStateCreateInfo pipeline_input_assembly_state_createinfo{
            .topology = vk::PrimitiveTopology::eTriangleList};

        vk::PipelineRasterizationStateCreateInfo pipeline_rasterization_state_createinfo{
            .flags                   = vk::PipelineRasterizationStateCreateFlags(),
            .depthClampEnable        = false,                       // depthClampEnable
            .rasterizerDiscardEnable = false,                       // rasterizerDiscardEnable
            .polygonMode             = vk::PolygonMode::eFill,      // polygonMode
            .cullMode                = vk::CullModeFlagBits::eBack, // cullMode
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
            // vk::DynamicState::eCullMode,
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
            .stageCount          = pipeline_shader_stage_createinfo.size(), // stages
            .pStages             = pipeline_shader_stage_createinfo.data(),
            .pVertexInputState   = &pipeline_vertex_input_state_createinfo,   // pVertexInputState
            .pInputAssemblyState = &pipeline_input_assembly_state_createinfo, // pInputAssemblyState
            .pTessellationState  = nullptr,                                   // pTessellationState
            .pViewportState      = &pipeline_viewport_state_createinfo,       // pViewportState
            .pRasterizationState = &pipeline_rasterization_state_createinfo,  // pRasterizationState
            .pMultisampleState   = &pipeline_multisample_state_createinfo,    // pMultisampleState
            .pDepthStencilState  = &pipeline_depth_stencil_state_createinfo,  // pDepthStencilState
            .pColorBlendState    = &pipeline_color_blend_state_createinfo,    // pColorBlendState
            .pDynamicState       = &pipeline_dynamic_state_createinfo,        // pDynamicState
            .layout              = m_pipeline_layout,                         // layout
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

        m_pipeline = pipeline;
    }

    const Reflected_descriptor_binding* Technique::find_binding(const std::string& reflected_name) const
    {
        auto itr = m_reflected_binding_map.find(reflected_name);
        if (itr == m_reflected_binding_map.end()) {
            return nullptr;
        }
        return &itr->second;
    }

} // namespace VKN
