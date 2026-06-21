#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace VKN {

    static constexpr uint32_t k_invalid_render_id = std::numeric_limits<uint32_t>::max();

    struct Render_texture_ref {
        std::string m_resource_name;
    };

    struct Render_material {
        std::string m_technique_name;
        uint32_t m_base_colour_texture = k_invalid_render_id;
    };

    struct Render_transform {
        Matrix m_world = Matrix::Identity;
    };

    struct Render_instance {
        uint32_t m_mesh_id      = 0;
        uint32_t m_material_id  = k_invalid_render_id;
        uint32_t m_transform_id = k_invalid_render_id;
    };

    struct Render_scene {
        std::vector<Render_texture_ref> m_textures;
        std::vector<Render_material> m_materials;
        std::vector<Render_transform> m_transforms;
        std::vector<Render_instance> m_instances;

        void clear()
        {
            m_textures.clear();
            m_materials.clear();
            m_transforms.clear();
            m_instances.clear();
        }
    };

    struct Render_scene_counters {
        uint32_t m_textures   = 0;
        uint32_t m_materials  = 0;
        uint32_t m_transforms = 0;
        uint32_t m_instances  = 0;
    };

    struct Render_scene_validation {
        bool m_ok                 = true;
        uint32_t m_instance_index = k_invalid_render_id;
        std::string m_reason;
    };

    class Render_scene_state {
      public:
        void bootstrap_demo_scene();

        // New detailed API for diagnostics
        bool need_validation() const { return m_need_validation; }
        const Render_scene_validation& last_validation_result() const { return m_last_validation_result; }
        Render_scene_validation validate_indices_verbose() const;

                Render_scene_counters counters() const;
                const Render_scene& scene() const { return m_scene; }

      private:
                Render_scene m_scene;
        mutable bool m_need_validation = false;
                mutable Render_scene_validation m_last_validation_result{};
    };

} // namespace VKN