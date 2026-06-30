#include "app.h"

#include <atomic>
#include <thread>

#include "common/common_cpp.h"
#include "global.h"

#include "gfx_device/gfx_main.h"
#include "main_renderer.h"
#include "renderscene.h"

std::chrono::time_point<std::chrono::steady_clock> App::m_time_begin_app;
std::chrono::time_point<std::chrono::steady_clock> App::m_time_begin_frame;
std::chrono::microseconds App::m_duration_frame;

std::unique_ptr<std::thread> render_thread;
std::atomic<bool> game_running = true;

Main_renderer main_renderer;
VKN::Render_scene_state g_scene_state;

void render_thread_func()
{
    while (game_running) {

        Sleep(1000);
        OutputDebugString(L"render frame\n");
    }
}

void App::on_init(HINSTANCE hInstance, HWND hWnd)
{
    m_hInstance = hInstance;
    m_hWnd      = hWnd;

    OutputDebugString(L"app start\n");

    // time
    m_time_begin_frame = m_time_begin_app = std::chrono::high_resolution_clock::now();

    // m_engine->update();
    // render thread
    render_thread.reset(new std::thread(render_thread_func));

    Gfx_main::create(m_hInstance, m_hWnd);

    auto&& gfx_device = Gfx_main::gfx_device();

    // load resources
    gfx_device.begin_single_command_submission();
    create_scene();
    gfx_device.end_single_command_submission();
}

struct Auto_mode_cycle {
    bool m_enabled  = true;
    bool m_finished = false;
    float m_start_s = 0.0f;
};

Auto_mode_cycle g_auto_mode_cycle;

constexpr float k_seconds_per_test = 1.25f;

// Keep only modes that currently have concrete pass builders.
constexpr std::array<Main_renderer::Render_mode, 3> k_test_sequence = {
    Main_renderer::Render_mode::VerificationCompute,
    Main_renderer::Render_mode::VerificationRaytrace,
    Main_renderer::Render_mode::CombinedDebug,
};

void App::on_update()
{

    if ((GetAsyncKeyState(VK_F8) & 0x0001) != 0) {
        main_renderer.request_framegraph_dump();
    }

    // frame time
    m_duration_frame =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - m_time_begin_frame);
    m_time_begin_frame = std::chrono::steady_clock::now();

    auto&& debug_str = DBG::Format(L"Cureent frame time %.4f ms", m_duration_frame.count() / 1000.0f);
    SetWindowText(m_hWnd, debug_str.c_str());

    // Update automatic mode cycling
    if (g_auto_mode_cycle.m_enabled && !g_auto_mode_cycle.m_finished) {
        const float elapsed_s = App::get_duration_app() - g_auto_mode_cycle.m_start_s;
        const uint32_t phase  = static_cast<uint32_t>(elapsed_s / k_seconds_per_test);

        if (phase < k_test_sequence.size()) {
            main_renderer.set_render_mode(k_test_sequence[phase]);
        }
        else {
            // Hand off to final 3D scene mode after all verification passes.
            main_renderer.set_render_mode(Main_renderer::Render_mode::MainScene3D);
            g_auto_mode_cycle.m_finished = true;
            OutputDebugStringA("Auto-cycle complete. Switched to MainScene3D.\n");
        }
    }

    // render the scene
    auto&& gfx_device = Gfx_main::gfx_device();

    gfx_device.begin_frame();
    main_renderer.draw();
    gfx_device.end_frame();
}

void App::on_destroy()
{
    game_running = false;

    render_thread->join();

    Gfx_main::destroy();

    OutputDebugString(L"app destroy\n");
}

void App::create_scene()
{
    auto&& gfx_device = Gfx_main::gfx_device();

    auto&& shader_manager   = Gfx_main::shader_manager();
    auto&& resource_manager = Gfx_main::resource_manager();

    // for each render passes
    auto&& colour_format = gfx_device.backbuffer_colour_format();
    resource_manager.create_depth_buffer(gfx_device.backbuffer_colour_size().width, gfx_device.backbuffer_colour_size().height);
    auto&& depth_buffer = resource_manager.depth_buffer();
    auto&& depth_format = depth_buffer.m_format;

    // create raster techniques
    shader_manager.register_raster_technique(
        "test/single_triangle", VKN::Raster_stage_mask::VS | VKN::Raster_stage_mask::PS, colour_format, depth_format);

    shader_manager.register_raster_technique(
        "test/constant_buffer", VKN::Raster_stage_mask::VS | VKN::Raster_stage_mask::PS, colour_format, depth_format);

    shader_manager.register_raster_technique(
        "test/bindless_textures", VKN::Raster_stage_mask::VS | VKN::Raster_stage_mask::PS, colour_format, depth_format);
    shader_manager.register_raster_technique(
        "test/fullscreen_texture", VKN::Raster_stage_mask::VS | VKN::Raster_stage_mask::PS, colour_format, depth_format);

    shader_manager.register_raster_technique(
        "test/mesh_shader_triangle", VKN::Raster_stage_mask::MS | VKN::Raster_stage_mask::PS, colour_format, depth_format);

    // scene material technique
    shader_manager.register_raster_technique(
        "scene/mesh_scene_unlit", VKN::Raster_stage_mask::MS | VKN::Raster_stage_mask::PS, colour_format, depth_format);

    // create compute techniques
    shader_manager.register_compute_technique("test/uav_resource");

    // create raytracing techniques
    shader_manager.register_raytracing_technique("test/ray_tracing_triangle");

    // create Textures
    auto texture_size = 64u;
    auto&& texdata    = TextureDataGenerator::create_checkerboard_texture_default(texture_size);
    resource_manager.create_texture("t_checkerboard", texdata);

    resource_manager.create_texture("t_checkerboard_redblue",
        TextureDataGenerator::create_checkerboard_texture(
            texture_size, glm::u8vec4(255, 0, 0, 255), glm::u8vec4(0, 0, 255, 255)));
    resource_manager.create_texture("t_checkerboard_redgreen",
        TextureDataGenerator::create_checkerboard_texture(
            texture_size, glm::u8vec4(255, 0, 0, 255), glm::u8vec4(0, 255, 0, 255)));
    resource_manager.create_texture("t_checkerboard_greenblue",
        TextureDataGenerator::create_checkerboard_texture(
            texture_size, glm::u8vec4(0, 255, 0, 255), glm::u8vec4(0, 0, 255, 255)));

    auto&& texdata_solid = TextureDataGenerator::create_solid_texture(texture_size, glm::u8vec4(255, 0, 255, 255));
    resource_manager.create_texture("t_solid_magenta", texdata_solid);

    auto solid_black = TextureDataGenerator::create_solid_texture(texture_size, glm::u8vec4(0, 0, 0, 255));
    resource_manager.create_texture("t_compute_output", solid_black, vk::ImageUsageFlagBits::eStorage);

    resource_manager.create_texture("t_raytracing_output", solid_black, vk::ImageUsageFlagBits::eStorage);

    // create samplers
    resource_manager.create_linear_wrap_sampler();

    // create mesh
    // resource_manager->create_mesh();

    g_scene_state.bootstrap_demo_scene();
    g_scene_state.validate_indices_verbose();
    assert(g_scene_state.last_validation_result().m_ok);

    g_scene_state.upload_to_gpu();

    main_renderer.set_scene_state(&g_scene_state);
    main_renderer.load_resource();
    main_renderer.set_render_mode(Main_renderer::Render_mode::VerificationCompute);

    // ===== RAYTRACING SETUP: BLAS/TLAS =====

    // Step 1: Create hardcoded triangle geometry
    // ==========================================
    // A simple triangle facing the camera at Z=0
    // Camera is at Z=-3, looking forward (+Z)
    std::vector<float> triangle_vertices = {
        -0.5f,
        -0.5f,
        0.0f, // Vertex 0 (left-bottom)
        0.5f,
        -0.5f,
        0.0f, // Vertex 1 (right-bottom)
        0.0f,
        0.5f,
        0.0f, // Vertex 2 (top-center)
    };

    std::vector<uint32_t> triangle_indices = {
        0,
        1,
        2, // Single triangle (counterclockwise when viewed from camera)
    };

    // Step 2: Build BLAS from triangle
    // ================================
    auto rt_blas = resource_manager.build_blas_from_buffers("rt_triangle_blas",
        triangle_indices.data(),
        static_cast<uint32_t>(triangle_indices.size() / 3), // triangle count
        triangle_vertices.data(),
        static_cast<uint32_t>(triangle_vertices.size() / 3) // vertex count
    );

    // Step 3: Create identity transform matrix for TLAS instance
    // ===========================================================
    VkTransformMatrixKHR identity_transform{
        .matrix = {{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}}};

    // Step 4: Build TLAS with one BLAS instance
    // =========================================
    std::vector<std::pair<const VKN::BLAS*, VkTransformMatrixKHR>> tlas_instances{
        {&rt_blas, identity_transform}, // One instance, no transform
    };

    auto rt_tlas = resource_manager.build_tlas_from_blas_instances("rt_triangle_tlas", tlas_instances);

    // Store TLAS for later reference in raytracing dispatch
    // (You'll use this in main_renderer.cpp)
    OutputDebugStringA("BLAS/TLAS built successfully\n");

    // Start automatic mode cycling
    main_renderer.set_render_mode(k_test_sequence[0]);
    g_auto_mode_cycle.m_start_s  = App::get_duration_app();
    g_auto_mode_cycle.m_finished = false;
}
