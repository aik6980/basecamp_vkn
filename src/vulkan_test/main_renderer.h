#pragma once

#include "framegraph.h"
#include "renderscene.h"

class Main_renderer {
  public:
    void load_resource();
    void draw();

    void set_scene_state(VKN::Render_scene_state* scene_state) { m_scene_state = scene_state; }
    void request_framegraph_dump() { m_dump_framegraph_requested = true; }

  private:
    std::unique_ptr<Frame_graph> m_frame_graph;
    VKN::Render_scene_state* m_scene_state = nullptr;

    bool m_dump_framegraph_requested = false;
};
