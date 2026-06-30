#pragma once

struct Camera {
    enum Projection_mode
    {
        Perspective,
        Ortho,
    };

    Projection_mode m_projection_mode;
    float           m_fov;
    float           m_z_near;
    float           m_z_far;
    float           m_orthographic_height;

    glm::mat4 m_view;
    glm::mat4 m_projection;
    glm::vec3 m_position;

    glm::vec3 position()
    {
        return m_position;
    }

    glm::mat4 view()
    {
        return m_view;
    }

    glm::mat4 world()
    {
        auto&& world = glm::inverse(m_view);
        return world;
    }

    glm::mat4 projection()
    {
        return m_projection;
    }

    glm::mat4 projection_to_world()
    {
        auto&& view_proj     = m_projection * m_view;
        auto&& view_proj_inv = glm::inverse(view_proj);

        return view_proj_inv;
    }
};
