#include "renderscene.h"

namespace VKN {

    void Render_scene_state::bootstrap_demo_scene()
    {
        m_scene.clear();

        m_scene.m_textures.push_back(RenderTextureRef{
            .m_resource_name = "t_checkerboard",
        });

        m_scene.m_materials.push_back(RenderMaterial{
            .m_technique_name      = "scene/mesh_scene_unlit",
            .m_base_colour_texture = 0,
        });

        m_scene.m_transforms.push_back(RenderTransform{
            .m_world = Matrix::Identity,
        });

        m_scene.m_instances.push_back(RenderInstance{
            .m_mesh_id      = 0,
            .m_material_id  = 0,
            .m_transform_id = 0,
        });
    }

    bool Render_scene_state::validate_indices() const
    {
        for (const auto& inst : m_scene.m_instances) {
            if (inst.m_material_id == k_invalid_render_id || inst.m_material_id >= m_scene.m_materials.size()) {
                return false;
            }
            if (inst.m_transform_id == k_invalid_render_id || inst.m_transform_id >= m_scene.m_transforms.size()) {
                return false;
            }

            const auto& material = m_scene.m_materials[inst.m_material_id];
            if (material.m_base_colour_texture != k_invalid_render_id &&
                material.m_base_colour_texture >= m_scene.m_textures.size()) {
                return false;
            }
        }
        return true;
    }

    RenderSceneCounters Render_scene_state::counters() const
    {
        return RenderSceneCounters{
            .m_textures   = static_cast<uint32_t>(m_scene.m_textures.size()),
            .m_materials  = static_cast<uint32_t>(m_scene.m_materials.size()),
            .m_transforms = static_cast<uint32_t>(m_scene.m_transforms.size()),
            .m_instances  = static_cast<uint32_t>(m_scene.m_instances.size()),
        };
    }

} // namespace VKN