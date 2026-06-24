#pragma once

#include "vma/vma.h"

namespace VKN {

    class Device;
    class Shader;

    struct Descriptorset_layoutdata;

    enum class Raster_stage_mask : uint32_t {
        None = 0,
        VS   = 1u << 0,
        PS   = 1u << 1,
        MS   = 1u << 2,
        // optional future:
        // AS = 1u << 3,
        // GS = 1u << 4,
    };

    inline Raster_stage_mask operator|(Raster_stage_mask a, Raster_stage_mask b)
    {
        return static_cast<Raster_stage_mask>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }
    inline bool has_stage(Raster_stage_mask mask, Raster_stage_mask bit)
    {
        return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(bit)) != 0;
    }

    struct Reflected_descriptor_binding {
        uint32_t m_set_number       = 0;
        uint32_t m_binding_number   = 0;
        uint32_t m_set_layout_index = 0; // index used when binding in pipeline layout
        vk::DescriptorType m_descriptor_type{};
        uint32_t m_descriptor_count         = 0;
        bool m_is_variable_descriptor_count = false;
    };

    class Technique {

      public:
        Technique(Device& gfx_device)
            : m_gfx_device(gfx_device)
        {
        }

        void destroy();

        void create_pipeline(vk::Format color_format, vk::Format depth_format);
        void create_compute_pipeline();
        void create_raytracing_pipeline();

        const Reflected_descriptor_binding* find_binding(const std::string& reflected_name) const;

      public:
        Device& m_gfx_device;

        std::weak_ptr<Shader> m_ms_handle;
        std::weak_ptr<Shader> m_vs_handle;
        std::weak_ptr<Shader> m_ps_handle;
        std::weak_ptr<Shader> m_cs_handle;
        std::weak_ptr<Shader> m_ray_lib_handle;

        Raster_stage_mask m_raster_stages = Raster_stage_mask::None;

        std::vector<vk::DescriptorSetLayout> m_descriptorset_layouts;
        std::vector<Descriptorset_layoutdata*> m_descriptorset_infos;

        std::unordered_map<std::string, Reflected_descriptor_binding> m_reflected_binding_map;

        vk::PipelineLayout m_pipeline_layout;
        vk::Pipeline m_pipeline;

        vk::PipelineBindPoint m_bind_point = vk::PipelineBindPoint::eGraphics;

        // Raytracing pipeline resources 
        // Shader binding table (SBT) buffer and allocation
        vk::Buffer m_sbt_buffer{};
        vma::Allocation m_sbt_allocation{};

        vk::StridedDeviceAddressRegionKHR m_sbt_raygen_region{};
        vk::StridedDeviceAddressRegionKHR m_sbt_miss_region{};
        vk::StridedDeviceAddressRegionKHR m_sbt_hit_region{};
        vk::StridedDeviceAddressRegionKHR m_sbt_callable_region{};

      private:
        void create_descriptor_pipeline_layout();
    };

} // namespace VKN
