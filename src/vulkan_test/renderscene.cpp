#include "renderscene.h"

namespace VKN {

    void Render_scene_state::bootstrap_demo_scene()
    {
        m_scene.clear();

        m_scene.m_textures.push_back(Render_texture_ref{.m_resource_name = "t_checkerboard"});
        m_scene.m_textures.push_back(Render_texture_ref{.m_resource_name = "t_checkerboard_redblue"});
        m_scene.m_textures.push_back(Render_texture_ref{.m_resource_name = "t_checkerboard_redgreen"});

        m_scene.m_materials.push_back(Render_material{
            .m_technique_name      = "scene/mesh_scene_unlit",
            .m_base_colour_texture = 0,
        });
        m_scene.m_materials.push_back(Render_material{
            .m_technique_name      = "scene/mesh_scene_unlit",
            .m_base_colour_texture = 1,
        });
        m_scene.m_materials.push_back(Render_material{
            .m_technique_name      = "scene/mesh_scene_unlit",
            .m_base_colour_texture = 2,
        });

        m_scene.m_transforms.push_back(
            Render_transform{.m_world = Matrix::CreateScale(0.35f) * Matrix::CreateTranslation(-0.55f, 0.35f, 0.0f)});
        m_scene.m_transforms.push_back(
            Render_transform{.m_world = Matrix::CreateScale(0.35f) * Matrix::CreateTranslation(0.00f, 0.35f, 0.0f)});
        m_scene.m_transforms.push_back(
            Render_transform{.m_world = Matrix::CreateScale(0.35f) * Matrix::CreateTranslation(0.55f, 0.35f, 0.0f)});
        m_scene.m_transforms.push_back(
            Render_transform{.m_world = Matrix::CreateScale(0.35f) * Matrix::CreateTranslation(-0.30f, -0.25f, 0.0f)});
        m_scene.m_transforms.push_back(
            Render_transform{.m_world = Matrix::CreateScale(0.35f) * Matrix::CreateTranslation(0.30f, -0.25f, 0.0f)});

        m_scene.m_instances.push_back(Render_instance{.m_mesh_id = 0, .m_material_id = 0, .m_transform_id = 0});
        m_scene.m_instances.push_back(Render_instance{.m_mesh_id = 0, .m_material_id = 1, .m_transform_id = 1});
        m_scene.m_instances.push_back(Render_instance{.m_mesh_id = 0, .m_material_id = 2, .m_transform_id = 2});
        m_scene.m_instances.push_back(Render_instance{.m_mesh_id = 0, .m_material_id = 1, .m_transform_id = 3});
        m_scene.m_instances.push_back(Render_instance{.m_mesh_id = 0, .m_material_id = 0, .m_transform_id = 4});

        m_need_validation = true;
    }

    Render_scene_validation Render_scene_state::validate_indices_verbose() const
    {
        Render_scene_validation result{};

        for (uint32_t i = 0; i < static_cast<uint32_t>(m_scene.m_instances.size()); ++i) {
            const auto& inst = m_scene.m_instances[i];

            if (inst.m_material_id == k_invalid_render_id) {
                result.m_ok             = false;
                result.m_instance_index = i;
                result.m_reason         = "invalid material id sentinel";
                return result;
            }
            if (inst.m_material_id >= m_scene.m_materials.size()) {
                result.m_ok             = false;
                result.m_instance_index = i;
                result.m_reason         = "material id out of range";
                return result;
            }

            if (inst.m_transform_id == k_invalid_render_id) {
                result.m_ok             = false;
                result.m_instance_index = i;
                result.m_reason         = "invalid transform id sentinel";
                return result;
            }
            if (inst.m_transform_id >= m_scene.m_transforms.size()) {
                result.m_ok             = false;
                result.m_instance_index = i;
                result.m_reason         = "transform id out of range";
                return result;
            }

            const auto& material = m_scene.m_materials[inst.m_material_id];
            if (material.m_base_colour_texture != k_invalid_render_id &&
                material.m_base_colour_texture >= m_scene.m_textures.size()) {
                result.m_ok             = false;
                result.m_instance_index = i;
                result.m_reason         = "base colour texture id out of range";
                return result;
            }
        }

        m_need_validation = false;
        m_last_validation_result = result;
        return m_last_validation_result;
    }

    Render_scene_counters Render_scene_state::counters() const
    {
        return Render_scene_counters{
            .m_textures   = static_cast<uint32_t>(m_scene.m_textures.size()),
            .m_materials  = static_cast<uint32_t>(m_scene.m_materials.size()),
            .m_transforms = static_cast<uint32_t>(m_scene.m_transforms.size()),
            .m_instances  = static_cast<uint32_t>(m_scene.m_instances.size()),
        };
    }

} // namespace VKN