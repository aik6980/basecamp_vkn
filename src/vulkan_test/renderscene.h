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

} // namespace VKN