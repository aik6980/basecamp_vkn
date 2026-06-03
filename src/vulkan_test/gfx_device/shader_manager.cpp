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
        const std::string& filename, vk::Format color_format, vk::Format depth_format)
    {
        if (auto&& t = get_technique(filename); t.lock() != nullptr) {
            return t;
        }

        auto&& vs_shader_handle = register_shader(filename + ".vs");
        auto&& ps_shader_handle = register_shader(filename + ".ps");

        auto&& technique = std::make_shared<Technique>(m_gfx_device);
        technique->m_vs_handle = vs_shader_handle;
        technique->m_ps_handle = ps_shader_handle;
        technique->create_pipeline(color_format, depth_format);

        m_technique_map.insert({filename, technique});
        return technique;
    }

    std::weak_ptr<Technique> Shader_manager::register_compute_technique(
        const std::string& filename)
    {
        if (auto&& t = get_technique(filename); t.lock() != nullptr) {
            return t;
        }

        auto&& cs_shader_handle = register_shader(filename + ".cs");

        auto&& technique = std::make_shared<Technique>(m_gfx_device);
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
