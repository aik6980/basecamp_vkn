#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace VKN {

    static constexpr uint32_t k_invalid_render_id = std::numeric_limits<uint32_t>::max();

    struct RenderTextureRef {
        std::string m_resource_name;
    };

    struct RenderMaterial {
        std::string m_technique_name;
        uint32_t m_base_colour_texture = k_invalid_render_id;
    };

    struct RenderTransform {
        Matrix m_world = Matrix::Identity;
    };

    struct RenderInstance {
        uint32_t m_mesh_id      = 0;
        uint32_t m_material_id  = k_invalid_render_id;
        uint32_t m_transform_id = k_invalid_render_id;
    };

    struct RenderScene {
        std::vector<RenderTextureRef> m_textures;
        std::vector<RenderMaterial> m_materials;
        std::vector<RenderTransform> m_transforms;
        std::vector<RenderInstance> m_instances;

        void clear()
        {
            m_textures.clear();
            m_materials.clear();
            m_transforms.clear();
            m_instances.clear();
        }
    };

    struct RenderSceneCounters {
        uint32_t m_textures   = 0;
        uint32_t m_materials  = 0;
        uint32_t m_transforms = 0;
        uint32_t m_instances  = 0;
    };

    struct RenderSceneValidation {
        bool m_ok                 = true;
        uint32_t m_instance_index = k_invalid_render_id;
        std::string m_reason;
    };

    class Render_scene_state {
      public:
        void bootstrap_demo_scene();

        // New detailed API for diagnostics
        bool need_validation() const { return m_need_validation; }
        const RenderSceneValidation& last_validation_result() const { return m_last_validation_result; }
        RenderSceneValidation validate_indices_verbose() const;

        RenderSceneCounters counters() const;
        const RenderScene& scene() const { return m_scene; }

      private:
        RenderScene m_scene;
        mutable bool m_need_validation = false;
        mutable RenderSceneValidation m_last_validation_result{};
    };

} // namespace VKN