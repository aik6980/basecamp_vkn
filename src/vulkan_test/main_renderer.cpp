#include "main_renderer.h"

// app
#include "gfx_device/gfx_main.h"

// systems
#include "framegraph.h"
#include "gfx_device/technique_instance.h"

void Main_renderer::load_resource() { m_frame_graph = std::make_unique<Frame_graph>(); }

void Main_renderer::draw()
{
    auto&& gfx_device     = Gfx_main::gfx_device();
    auto&& shader_manager = Gfx_main::shader_manager();

    auto&& command_buffer = gfx_device.curr_command_buffer();
    if (!command_buffer) {
        return;
    }

    m_frame_graph->clear();

    // local resource id:
    constexpr uint32_t kResComputeOutput = 1;
    constexpr uint32_t kResRenderTarget  = 2;

    auto&& compute_output_texture = Gfx_main::resource_manager().get_texture("t_compute_output");
    // Add compute pass:
    PassNode compute_pass;
    compute_pass.name = "compute_write_uav";
    compute_pass.writes.push_back(ResourceUse{
        .resource_id = kResComputeOutput,
        .is_write    = true,
        .layout      = vk::ImageLayout::eGeneral,
        .access      = vk::AccessFlagBits2::eShaderStorageWrite,
        .stage       = vk::PipelineStageFlagBits2::eComputeShader,
        .is_image    = true,
        .image       = compute_output_texture.m_image,
        .image_range =
            vk::ImageSubresourceRange{
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
    });

    compute_pass.execute = [&](vk::CommandBuffer& cmd) {
        auto&& technique = shader_manager.get_technique("test/uav_resource").lock();
        if (technique) {
            auto&& technique_instance = VKN::Technique_instance(*technique);

            const bool bind_ok = technique_instance.bind_storage_image_by_name("ColourTex_uav", "t_compute_output");

            // create a helper functio for following steps as they are common for both compute and raster techniques:
            // 1. bind pipeline
            cmd.bindPipeline(technique->m_bind_point, technique->m_pipeline);

            const bool apply_ok = technique_instance.apply();
            assert(bind_ok && apply_ok);

            // 2. dispatch compute shader with enough thread groups to cover the entire output texture
            const auto group_count_x = (compute_output_texture.m_width + 7) / 8;
            const auto group_count_y = (compute_output_texture.m_height + 7) / 8;
            cmd.dispatch(group_count_x, group_count_y, 1);
        }
    };

    m_frame_graph->add_pass(compute_pass);

    // Add raster pass:
    PassNode raster_pass;
    raster_pass.name = "raster_sample_uav_result";
    raster_pass.reads.push_back(ResourceUse{
        .resource_id = kResComputeOutput,
        .is_write    = false,
        .layout      = vk::ImageLayout::eShaderReadOnlyOptimal,
        .access      = vk::AccessFlagBits2::eShaderSampledRead,
        .stage       = vk::PipelineStageFlagBits2::eFragmentShader,
        .is_image    = true,
        .image       = compute_output_texture.m_image,
        .image_range =
            vk::ImageSubresourceRange{
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
    });

    auto&& render_target_image = gfx_device.offscreen_colour_image();
    raster_pass.writes.push_back(ResourceUse{
        .resource_id = kResRenderTarget,
        .is_write    = true,
        .layout      = vk::ImageLayout::eColorAttachmentOptimal,
        .access      = vk::AccessFlagBits2::eColorAttachmentWrite,
        .stage       = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .is_image    = true,
        .image       = render_target_image,
        .image_range =
            vk::ImageSubresourceRange{
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
    });

    auto&& render_target_image_view = gfx_device.offscreen_colour_image_view();
    auto&& depth_target_image       = gfx_device.backbuffer_depth_image();

    raster_pass.execute = [&](vk::CommandBuffer& cmd) {
        // setup render pass

        vk::ClearColorValue clear_colour{std::array<float, 4>{0.2f, 0.2f, 0.2f, 0.2f}};
        vk::ClearDepthStencilValue clear_depth = {
            .depth   = 1.0f,
            .stencil = 0u,
        };

        vk::RenderingAttachmentInfo colour_attachment{
            .imageView   = render_target_image_view,
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp      = vk::AttachmentLoadOp::eClear,
            .storeOp     = vk::AttachmentStoreOp::eStore,
            .clearValue  = clear_colour,
        };

        vk::RenderingInfo rendering_info{
            .renderArea =
                {
                    .offset = {0, 0},
                    .extent = gfx_device.backbuffer_colour_size(),
                },
            .layerCount           = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &colour_attachment,

        };

        command_buffer->beginRendering(&rendering_info);

        command_buffer->setViewport(0,
            vk::Viewport(0.0f,
                0.0f,
                static_cast<float>(rendering_info.renderArea.extent.width),
                static_cast<float>(rendering_info.renderArea.extent.height),
                0.0f,
                1.0f));
        command_buffer->setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), rendering_info.renderArea.extent));

        // 1st draw
        {
            auto&& technique = shader_manager.get_technique("test/single_triangle").lock();
            command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, technique->m_pipeline);
            command_buffer->draw(3, 1, 0, 0);
        }

        // 2nd draw
        {
            auto&& technique = shader_manager.get_technique("test/constant_buffer").lock();

            auto&& technique_instance = VKN::Technique_instance(*technique);
            float data[]              = {0.25f, -0.25f};
            const bool bound_ok       = technique_instance.bind_constant_by_name("Data_cbv", data, sizeof(data));

            float psData[]             = {0.8f, 0.1f, 0.6f};
            const bool psData_bound_ok = technique_instance.bind_constant_by_name("PsData_cbv", psData, sizeof(psData));

            command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, technique->m_pipeline);
            const bool apply_ok = bound_ok && psData_bound_ok && technique_instance.apply();
            assert(apply_ok);

            command_buffer->draw(3, 1, 0, 0);
        }

        // 3rd draw
        {
            auto&& technique = shader_manager.get_technique("test/bindless_textures").lock();

            auto&& technique_instance = VKN::Technique_instance(*technique);

            // const bool single_ok = technique_instance.bind_sampled_image_by_name("ColourTex_srv", "t_solid_magenta");
            const bool single_ok = technique_instance.bind_sampled_image_by_name("ColourTex_srv", "t_compute_output");

            std::vector<std::string> bindless_textures = {
                "t_checkerboard",
                "t_checkerboard_redblue",
                "t_checkerboard_redgreen",
                "t_checkerboard_greenblue",
            };

            const bool textures_ok =
                technique_instance.bind_sampled_image_by_name("ColourTexBindless_srv", bindless_textures);
            const bool sampler_ok = technique_instance.bind_sampler_by_name("Linear_sam", "s_linear_wrap");

            command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, technique->m_pipeline);
            const bool apply_ok = single_ok && textures_ok && sampler_ok && technique_instance.apply();
            assert(apply_ok);

            auto total_instances = static_cast<uint32_t>(bindless_textures.size()) + 1; // +1 for single texture draw
            command_buffer->draw(6, total_instances, 0, 0);
        }

        // 4th draw
        {
            auto&& technique = shader_manager.get_technique("test/mesh_shader_triangle").lock();
            auto&& technique_instance = VKN::Technique_instance(*technique);

            command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, technique->m_pipeline);
            const bool apply_ok = technique_instance.apply();
            assert(apply_ok);

            command_buffer->drawMeshTasksEXT(1, 1, 1);
        }

        command_buffer->endRendering();
    };

    m_frame_graph->add_pass(raster_pass);

    // copy offscreen render target to backbuffer
    // Add blit pass for copying render target to swapchain
    constexpr uint32_t kResSwapchain = 3;
    auto&& swapchain_image           = gfx_device.backbuffer_colour_image();

    PassNode blit_pass;
    blit_pass.name = "blit_render_to_swapchain";

    // Read from render target in transfer-src layout
    blit_pass.reads.push_back(ResourceUse{
        .resource_id = kResRenderTarget,
        .is_write    = false,
        .layout      = vk::ImageLayout::eTransferSrcOptimal,
        .access      = vk::AccessFlagBits2::eTransferRead,
        .stage       = vk::PipelineStageFlagBits2::eTransfer,
        .is_image    = true,
        .image       = render_target_image,
        .image_range =
            vk::ImageSubresourceRange{
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
    });

    // Write to swapchain in transfer-dst layout
    blit_pass.writes.push_back(ResourceUse{
        .resource_id = kResSwapchain,
        .is_write    = true,
        .layout      = vk::ImageLayout::eTransferDstOptimal,
        .access      = vk::AccessFlagBits2::eTransferWrite,
        .stage       = vk::PipelineStageFlagBits2::eTransfer,
        .is_image    = true,
        .image       = swapchain_image,
        .image_range =
            vk::ImageSubresourceRange{
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
    });

    blit_pass.execute = [&](vk::CommandBuffer& cmd) {
        vk::ImageCopy copy_region{
            .srcSubresource =
                vk::ImageSubresourceLayers{
                    .aspectMask     = vk::ImageAspectFlagBits::eColor,
                    .mipLevel       = 0,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
                },
            .srcOffset = vk::Offset3D{0, 0, 0},
            .dstSubresource =
                vk::ImageSubresourceLayers{
                    .aspectMask     = vk::ImageAspectFlagBits::eColor,
                    .mipLevel       = 0,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
                },
            .dstOffset = vk::Offset3D{0, 0, 0},
            .extent = vk::Extent3D{gfx_device.backbuffer_colour_size().width, gfx_device.backbuffer_colour_size().height, 1},
        };

        cmd.copyImage(render_target_image,
            vk::ImageLayout::eTransferSrcOptimal,
            swapchain_image,
            vk::ImageLayout::eTransferDstOptimal,
            1,
            &copy_region);
    };

    m_frame_graph->add_pass(blit_pass);

    // Present pass: transition swapchain image to present layout inside frame graph.
    PassNode present_pass;
    present_pass.name = "present_swapchain";
    present_pass.writes.push_back(ResourceUse{
        .resource_id = kResSwapchain,
        .is_write    = true,
        .layout      = vk::ImageLayout::ePresentSrcKHR,
        .access      = vk::AccessFlagBits2::eNone,
        .stage       = vk::PipelineStageFlagBits2::eBottomOfPipe,
        .is_image    = true,
        .image       = swapchain_image,
        .image_range =
            vk::ImageSubresourceRange{
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
    });
    present_pass.execute = [&](vk::CommandBuffer& cmd) { (void)cmd; };

    m_frame_graph->add_pass(present_pass);

    // compile and execute frame graph
    m_frame_graph->compile();
    m_frame_graph->execute(*command_buffer);
    if (m_dump_framegraph_requested) {
        m_dump_framegraph_requested = false;

        const std::string dot     = m_frame_graph->build_debug_dot();
        const std::string mermaid = m_frame_graph->build_debug_mermaid();

        namespace fs = std::filesystem;
        fs::create_directories("framegraph");

        const auto t = static_cast<long long>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count());

        const fs::path dot_path = fs::path("framegraph") / ("framegraph_" + std::to_string(t) + ".dot");
        const fs::path mmd_path = fs::path("framegraph") / ("framegraph_" + std::to_string(t) + ".mmd");

        {
            std::ofstream f(dot_path, std::ios::binary);
            f << dot;
        }
        {
            std::ofstream f(mmd_path, std::ios::binary);
            f << mermaid;
        }

        OutputDebugStringA(("FrameGraph exported: " + dot_path.string() + " , " + mmd_path.string() + "\n").c_str());
    }
}
