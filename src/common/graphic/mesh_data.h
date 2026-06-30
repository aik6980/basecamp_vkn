#pragma once

#include "common/math/aabb.h"

struct Mesh_vertex_array {
    // vertices
    vector<glm::vec3> m_position;
    vector<glm::vec3> m_normal;
    vector<glm::vec4> m_colour;

    void reset_vertices(UINT n);
};

struct Mesh_index_array {
    // indices
    vector<uint32_t> m_indices32;

    void reset_indices(UINT n);
};

struct Mesh_data {
    Mesh_vertex_array m_vertices;
    Mesh_index_array m_indices;

    Aabb3 m_aabb;
};

struct TextureData {
    uint32_t m_width  = 0;
    uint32_t m_height = 0;
    vector<glm::u8vec4> m_data;

    void reset(uint32_t w, uint32_t h);
    void set_data(uint32_t x, uint32_t y, const glm::u8vec4& val);

    int pixel_size_in_byte() const { return sizeof(decltype(*m_data.begin())); }
};

struct RT_vertex {
    glm::vec3 m_position;
};

struct P1_vertex {
    glm::vec4 m_position;
};
