#pragma once

namespace VKN {
    class Technique;
    struct Buffer;

    class Technique_instance {
      public:
        Technique_instance(Technique& tech)
            : m_tech(tech)
        {
        }

        bool bind_constant_by_name(const std::string& reflected_name, const void* data, size_t size);
        bool apply();

        Technique& m_tech;

        std::unordered_map<std::string, std::weak_ptr<Buffer>> m_constant_buffer_map;
    };
} // namespace VKN
