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
        bool bind_sampled_images_by_name(const std::string& reflected_name, const std::vector<std::string>& texture_names);
        bool bind_sampler_by_name(const std::string& reflected_name, const std::string& sampler_name);

        bool apply();

        Technique& m_tech;

        std::unordered_map<std::string, std::weak_ptr<Buffer>> m_constant_buffer_map;
        std::unordered_map<std::string, std::vector<std::string>> m_sampled_image_array_map;
        std::unordered_map<std::string, std::string> m_sampler_map;
    };
} // namespace VKN
