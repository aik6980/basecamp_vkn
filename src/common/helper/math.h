#pragma once

struct Vec3 {
    static constexpr glm::vec3 Origin = glm::vec3(0.0f, 0.0f, 0.0f);
    static constexpr glm::vec3 Zero   = glm::vec3(0.0f, 0.0f, 0.0f);
    static constexpr glm::vec3 One    = glm::vec3(1.0f, 1.0f, 1.0f);
};

struct Mat4 {
    static constexpr glm::mat4 Identity = glm::mat4(1.0f);
};

struct Quat {
    static constexpr glm::quat Identity = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
};
