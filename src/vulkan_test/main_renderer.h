#pragma once

#include "framegraph.h"
#include "renderscene.h"

class Main_renderer {
  public:
    enum class Render_mode { MainScene3D, VerificationCompute, VerificationRaytrace, VerificationBindless, CombinedDebug };

    void load_resource();
    void draw();

    void set_scene_state(VKN::Render_scene_state* scene_state) { m_scene_state = scene_state; }
    void request_framegraph_dump() { m_dump_framegraph_requested = true; }

    void set_render_mode(Render_mode mode) { m_render_mode = mode; }
    Render_mode render_mode() const { return m_render_mode; }

  private:
    void build_verification_compute_passes(Frame_graph& frame_graph);
    void build_verification_bindless_passes(Frame_graph& frame_graph);
    void build_verification_raytrace_passes(Frame_graph& frame_graph);
    void build_combined_debug_passes(Frame_graph& frame_graph);

    void build_main_scene_passes(Frame_graph& frame_graph);
    void append_blit_and_present_passes(Frame_graph& frame_graph);

    std::unique_ptr<Frame_graph> m_frame_graph;
    VKN::Render_scene_state* m_scene_state = nullptr;

    bool m_dump_framegraph_requested = false;

    Render_mode m_render_mode = Render_mode::CombinedDebug;
};
