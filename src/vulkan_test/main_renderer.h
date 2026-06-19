#pragma once

#include "framegraph.h"

class Main_renderer {
  public:
    void load_resource();
    void draw();

    void request_framegraph_dump() { m_dump_framegraph_requested = true; }

  private:
    std::unique_ptr<Frame_graph> m_frame_graph;

    bool m_dump_framegraph_requested = false;
};
