#pragma once

struct Aabb3 {
    glm::vec3 m_min, m_max;

    Aabb3() { reset(); }

    void reset()
    {
        m_min = glm::vec3(numeric_limits<float>::max(), numeric_limits<float>::max(), numeric_limits<float>::max());
        m_max = glm::vec3(numeric_limits<float>::lowest(), numeric_limits<float>::lowest(), numeric_limits<float>::lowest());
    }

    void expand(const glm::vec3& p)
    {
        m_min.x = std::min(m_min.x, p.x);
        m_min.y = std::min(m_min.y, p.y);
        m_min.z = std::min(m_min.z, p.z);

        m_max.x = std::max(m_max.x, p.x);
        m_max.y = std::max(m_max.y, p.y);
        m_max.z = std::max(m_max.z, p.z);
    }
};
