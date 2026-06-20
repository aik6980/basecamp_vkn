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
    auto&& depth_format  = gfx_device.backbuffer_depth_format();

    // create raster techniques
    shader_manager.register_raster_technique(
        "test/single_triangle", VKN::Raster_stage_mask::VS | VKN::Raster_stage_mask::PS, colour_format, depth_format);

    shader_manager.register_raster_technique(
        "test/constant_buffer", VKN::Raster_stage_mask::VS | VKN::Raster_stage_mask::PS, colour_format, depth_format);

    shader_manager.register_raster_technique(
        "test/bindless_textures", VKN::Raster_stage_mask::VS | VKN::Raster_stage_mask::PS, colour_format, depth_format);

    shader_manager.register_raster_technique(
        "test/mesh_shader_triangle", VKN::Raster_stage_mask::MS | VKN::Raster_stage_mask::PS, colour_format, depth_format);

    // scene material technique
    shader_manager.register_raster_technique(
        "scene/mesh_scene_unlit", VKN::Raster_stage_mask::MS | VKN::Raster_stage_mask::PS, colour_format, depth_format);

    // create compute techniques
    shader_manager.register_compute_technique("test/uav_resource");

    // create Textures
    auto texture_size = 64u;
    auto&& texdata    = TextureDataGenerator::create_checkerboard_texture_default(texture_size);
    resource_manager.create_texture("t_checkerboard", texdata);

    resource_manager.create_texture("t_checkerboard_redblue",
        TextureDataGenerator::create_checkerboard_texture(
            texture_size, XMCOLOR(1.0f, 0.0f, 0.0f, 1.0f), XMCOLOR(0.0f, 0.0f, 1.0f, 1.0f)));
    resource_manager.create_texture("t_checkerboard_redgreen",
        TextureDataGenerator::create_checkerboard_texture(
            texture_size, XMCOLOR(1.0f, 0.0f, 0.0f, 1.0f), XMCOLOR(0.0f, 1.0f, 0.0f, 1.0f)));
    resource_manager.create_texture("t_checkerboard_greenblue",
        TextureDataGenerator::create_checkerboard_texture(
            texture_size, XMCOLOR(0.0f, 1.0f, 0.0f, 1.0f), XMCOLOR(0.0f, 0.0f, 1.0f, 1.0f)));

    auto&& texdata_solid = TextureDataGenerator::create_solid_texture(texture_size, XMCOLOR(1.0f, 0.0f, 1.0f, 1.0f));
    resource_manager.create_texture("t_solid_magenta", texdata_solid);

    auto compute_out = TextureDataGenerator::create_solid_texture(texture_size, XMCOLOR(0.0f, 0.0f, 0.0f, 1.0f));
    resource_manager.create_texture("t_compute_output", compute_out, vk::ImageUsageFlagBits::eStorage);

    // create samplers
    resource_manager.create_linear_wrap_sampler();

    // create mesh
    // resource_manager->create_mesh();

    g_scene_state.bootstrap_demo_scene();
    assert(g_scene_state.validate_indices());

    main_renderer.set_scene_state(&g_scene_state);
    main_renderer.load_resource();
}
