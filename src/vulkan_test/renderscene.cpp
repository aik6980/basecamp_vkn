#include "renderscene.h"

#include "gfx_device/gfx_main.h"
#include "shaders/hlsl_shared_struct.h"

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

        m_scene.m_meshes.push_back(Render_mesh{
            .m_vertex_count  = 24, // cube: 4 verts × 6 faces
            .m_index_count   = 36, // cube: 6 indices × 6 faces
            .m_vertex_offset = 0,
            .m_index_offset  = 0,
        });

        constexpr int k_grid = 4; // 4x4 = 16 cubes
        for (int row = 0; row < k_grid; ++row) {
            for (int col = 0; col < k_grid; ++col) {
                glm::vec3 pos = glm::vec3(col * 1.5f, 0.0f, row * 1.5f);
                m_scene.m_transforms.push_back(Render_transform{.m_obj_to_world = glm::translate(glm::mat4(1.0f), pos)});
                uint32_t mat_id = (row + col) % 3; // cycle materials
                m_scene.m_instances.push_back(Render_instance{
                    .m_mesh_id = 0, .m_material_id = mat_id, .m_transform_id = (uint32_t)m_scene.m_transforms.size() - 1});
            }
        }

        m_need_validation = true;
    }

    void Render_scene_state::upload_to_gpu()
    {
        auto&& resource_manager = Gfx_main::resource_manager();

        // --- instances ---
        {
            std::vector<Scene_instance_desc> gpu_instances;
            gpu_instances.reserve(m_scene.m_instances.size());
            for (const auto& inst : m_scene.m_instances) {
                gpu_instances.push_back(Scene_instance_desc{
                    .m_mesh_id         = inst.m_mesh_id,
                    .m_material_id     = inst.m_material_id,
                    .m_transform_id    = inst.m_transform_id,
                    .m_flags           = inst.m_flags,
                    .m_visibility_mask = inst.m_visibility_mask,
                    .m_blas_id         = inst.m_blas_id,
                    .m_pad0            = 0,
                    .m_pad1            = 0,
                });
            }
            resource_manager.create_storage_buffer(
                "scene_instances", gpu_instances.data(), gpu_instances.size() * sizeof(Scene_instance_desc));
        }

        // --- materials ---
        {
            std::vector<Scene_material_desc> gpu_materials;
            gpu_materials.reserve(m_scene.m_materials.size());
            for (const auto& mat : m_scene.m_materials) {
                Scene_material_desc d{};
                d.m_base_colour_texture = mat.m_base_colour_texture;
                d.m_normal_texture      = mat.m_normal_texture;
                d.m_surface_texture     = mat.m_surface_texture;
                d.m_flags               = mat.m_flags;
                d.m_base_colour_factor  = glm::vec4(mat.m_base_colour_factor.x,
                    mat.m_base_colour_factor.y,
                    mat.m_base_colour_factor.z,
                    mat.m_base_colour_factor.w);
                d.m_metallic            = mat.m_metallic;
                d.m_roughness           = mat.m_roughness;
                d.m_pad0                = 0;
                d.m_pad1                = 0;
                gpu_materials.push_back(d);
            }
            resource_manager.create_storage_buffer(
                "scene_materials", gpu_materials.data(), gpu_materials.size() * sizeof(Scene_material_desc));
        }

        // --- transforms ---
        {
            std::vector<Scene_transform_desc> gpu_transforms;
            gpu_transforms.reserve(m_scene.m_transforms.size());
            for (const auto& xform : m_scene.m_transforms) {
                Scene_transform_desc d{};
                d.m_obj_to_world = xform.m_obj_to_world;
                d.m_world_to_obj = xform.m_world_to_obj;
                gpu_transforms.push_back(d);
            }
            resource_manager.create_storage_buffer(
                "scene_transforms", gpu_transforms.data(), gpu_transforms.size() * sizeof(Scene_transform_desc));
        }

        // --- meshes ---
        {
            std::vector<Scene_mesh_desc> gpu_meshes;
            gpu_meshes.reserve(m_scene.m_meshes.size());
            for (const auto& mesh : m_scene.m_meshes) {
                gpu_meshes.push_back(Scene_mesh_desc{
                    .m_num_vertices    = mesh.m_vertex_count,
                    .m_num_indices     = mesh.m_index_count,
                    .m_offset_vertices = mesh.m_vertex_offset,
                    .m_offset_indices  = mesh.m_index_offset,
                    .m_bounds_center   = glm::vec3(mesh.m_bounds_center.x, mesh.m_bounds_center.y, mesh.m_bounds_center.z),
                    .m_bounds_radius   = mesh.m_bounds_radius,
                });
            }
            if (!gpu_meshes.empty()) {
                resource_manager.create_storage_buffer(
                    "scene_meshes", gpu_meshes.data(), gpu_meshes.size() * sizeof(Scene_mesh_desc));
            }
        }
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

        m_need_validation        = false;
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