#include "shader_manager.h"

namespace VKN {

    void Shader_manager::destroy_resources()
    {
        for (auto&& itr : m_technique_map) {
            itr.second->destroy();
        }
        m_technique_map.clear();

        for (auto&& itr : m_shader_map) {
            itr.second->destroy();
        }
        m_shader_map.clear();
    }

    std::weak_ptr<Technique> Shader_manager::register_raster_technique(
        const std::string& filename, Raster_stage_mask stages, vk::Format color_format, vk::Format depth_format)
    {
        if (auto&& t = get_technique(filename); t.lock() != nullptr) {
            return t;
        }

        const bool has_vs = has_stage(stages, Raster_stage_mask::VS);
        const bool has_ms = has_stage(stages, Raster_stage_mask::MS);
        const bool has_ps = has_stage(stages, Raster_stage_mask::PS);

        if (!has_ps) {
            throw std::runtime_error("register_graphics_technique requires PS stage");
        }
        if (has_vs == has_ms) {
            throw std::runtime_error("register_graphics_technique requires exactly one of VS or MS");
        }

        auto&& technique           = std::make_shared<Technique>(m_gfx_device);
        technique->m_raster_stages = stages;

        if (has_vs) {
            technique->m_vs_handle = register_shader(filename + ".vs");
        }
        if (has_ms) {
            technique->m_ms_handle = register_shader(filename + ".ms");
        }
        if (has_ps) {
            technique->m_ps_handle = register_shader(filename + ".ps");
        }

        technique->create_pipeline(color_format, depth_format);

        m_technique_map.insert({filename, technique});
        return technique;
    }

    std::weak_ptr<Technique> Shader_manager::register_compute_technique(const std::string& filename)
    {
        if (auto&& t = get_technique(filename); t.lock() != nullptr) {
            return t;
        }

        auto&& cs_shader_handle = register_shader(filename + ".cs");

        auto&& technique       = std::make_shared<Technique>(m_gfx_device);
        technique->m_cs_handle = cs_shader_handle;
        technique->create_compute_pipeline();

        m_technique_map.insert({filename, technique});
        return technique;
    }

    std::weak_ptr<Technique> Shader_manager::get_technique(std::string name)
    {
        auto&& itr = m_technique_map.find(name);
        if (itr != std::end(m_technique_map)) {
            return itr->second;
        }

        return std::weak_ptr<Technique>();
    }

    std::weak_ptr<Shader> Shader_manager::register_shader(std::string name)
    {
        if (auto&& sh = get_shader(name); sh.lock() != nullptr) {
            return sh;
        }

        auto&& shader = std::make_shared<Shader>(m_gfx_device);
        shader->create_shader(name);

        m_shader_map.insert({name, shader});
        return shader;
    }

    std::weak_ptr<Shader> Shader_manager::get_shader(std::string name)
    {
        auto&& result = m_shader_map.find(name);
        if (result != m_shader_map.end()) {
            return result->second;
        }

        return std::weak_ptr<Shader>();
    }

} // namespace VKN
