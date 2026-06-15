#pragma once

#include "framegraph.h"

class Main_renderer {
  public:
    void load_resource();
    void draw();

  private:
    std::unique_ptr<Frame_graph> m_frame_graph;
};
