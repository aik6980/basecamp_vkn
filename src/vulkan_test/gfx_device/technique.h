#pragma once

namespace VKN {

    class Device;
    class Shader;

    struct Descriptorset_layoutdata;

    struct Reflected_descriptor_binding {
        uint32_t m_set_number       = 0;
        uint32_t m_binding_number   = 0;
        uint32_t m_set_layout_index = 0; // index used when binding in pipeline layout
        vk::DescriptorType m_descriptor_type{};
        uint32_t m_descriptor_count = 0;
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

        const Reflected_descriptor_binding* find_binding(const std::string& reflected_name) const;

      public:
        Device& m_gfx_device;

        std::weak_ptr<Shader> m_vs_handle;
        std::weak_ptr<Shader> m_ps_handle;
        std::weak_ptr<Shader> m_cs_handle;

        std::vector<vk::DescriptorSetLayout> m_descriptorset_layouts;
        std::vector<Descriptorset_layoutdata*> m_descriptorset_infos;

        std::unordered_map<std::string, Reflected_descriptor_binding> m_reflected_binding_map;

        vk::PipelineLayout m_pipeline_layout;
        vk::Pipeline m_pipeline;

      private:
        void create_descriptor_pipeline_layout();
    };

} // namespace VKN
