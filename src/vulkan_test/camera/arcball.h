#pragma once

class Arcball {
  public:
    void init(const glm::vec3& pos, const glm::vec3& target);
    void update();

    glm::vec3 npos_to_vector(const glm::vec2& npos);

    glm::vec3 pos();
    glm::mat4 view();

    enum State
    {
        State_idle,
        State_rotate_arcball,
    };

    State m_state = State_idle;

    glm::vec2    m_begin_mouse_pos;
    glm::quat m_begin_orient;

    glm::vec3    m_target;
    glm::quat m_orient;
    float      m_radius_screen_space = 0.9f; // arcball's radius in screen space;
    float      m_zoom;

    glm::vec3 m_view, m_right;
};
