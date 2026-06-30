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
        uint32_t m_normal_texture      = k_invalid_render_id;
        uint32_t m_surface_texture     = k_invalid_render_id; // roughness/metallic/ao packed later

        glm::vec4 m_base_colour_factor = glm::vec4(1.f, 1.f, 1.f, 1.f);
        float m_metallic             = 0.0f;
        float m_roughness            = 1.0f;
        uint32_t m_flags             = 0;
    };

    struct Render_transform {
        glm::mat4 m_obj_to_world = Mat4::Identity;
        glm::mat4 m_world_to_obj = Mat4::Identity;
    };

    struct Render_mesh {
        uint32_t m_vertex_count  = 0;
        uint32_t m_index_count   = 0;
        uint32_t m_vertex_offset = 0;
        uint32_t m_index_offset  = 0;

        glm::vec3 m_bounds_center = Vec3::Zero;
        float m_bounds_radius   = 0.0f;
    };

    struct Render_instance {
        uint32_t m_mesh_id      = k_invalid_render_id;
        uint32_t m_material_id  = k_invalid_render_id;
        uint32_t m_transform_id = k_invalid_render_id;

        uint32_t m_flags           = 0;
        uint32_t m_visibility_mask = 0xFF;

        // Reserved for future RT/TLAS mapping without changing the contract.
        uint32_t m_blas_id = k_invalid_render_id;
    };

    struct Render_scene {
        std::vector<Render_texture_ref> m_textures;
        std::vector<Render_material> m_materials;
        std::vector<Render_transform> m_transforms;
        std::vector<Render_mesh> m_meshes;
        std::vector<Render_instance> m_instances;

        void clear()
        {
            m_textures.clear();
            m_materials.clear();
            m_transforms.clear();
            m_meshes.clear();
            m_instances.clear();
        }
    };

    struct Render_scene_counters {
        uint32_t m_textures   = 0;
        uint32_t m_materials  = 0;
        uint32_t m_transforms = 0;
        uint32_t m_meshes     = 0;
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
        void upload_to_gpu();
        
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