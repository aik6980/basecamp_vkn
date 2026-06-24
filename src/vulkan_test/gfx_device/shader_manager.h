#pragma once

#include "shader.h"
#include "technique.h"

namespace VKN {
    class Shader_manager {
      public:
        Shader_manager(Device& gfx_device)
            : m_gfx_device(gfx_device)
        {
        }

        void destroy_resources();

        std::weak_ptr<Technique> register_raster_technique(
            const std::string& filename, Raster_stage_mask stages, vk::Format color_format, vk::Format depth_format);
        std::weak_ptr<Technique> register_compute_technique(const std::string& filename);
        std::weak_ptr<Technique> register_raytracing_technique(const std::string& filename);

        std::weak_ptr<Technique> get_technique(std::string name);

      private:
        std::weak_ptr<Shader> register_shader(std::string name);
        std::weak_ptr<Shader> get_shader(std::string name);

        Device& m_gfx_device;

        std::unordered_map<std::string, std::shared_ptr<Shader>> m_shader_map;
        std::unordered_map<std::string, std::shared_ptr<Technique>> m_technique_map;
    };
} // namespace VKN
