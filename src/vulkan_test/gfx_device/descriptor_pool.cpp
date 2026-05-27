#include "descriptor_pool.h"

#include "device.h"

namespace VKN {

    void Descriptor_pool::create_pool()
    {
        auto&& device = m_gfx_device.m_device;

        std::array<vk::DescriptorPoolSize, 3> pool_sizes = {
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, Descriptor_pool::m_max_descriptor),
            vk::DescriptorPoolSize(vk::DescriptorType::eSampledImage, Descriptor_pool::m_max_descriptor),
            vk::DescriptorPoolSize(vk::DescriptorType::eSampler, Descriptor_pool::m_max_descriptor),
        };

        vk::DescriptorPoolCreateInfo create_info{
            .maxSets       = Descriptor_pool::m_max_descriptor,
            .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
            .pPoolSizes    = pool_sizes.data(),
        };

        m_descriptor_pool = device.createDescriptorPool(create_info);
        m_descriptor_sets.reserve(Descriptor_pool::m_max_descriptor);
    }

    void Descriptor_pool::destroy_resources()
    {
        m_gfx_device.m_device.destroyDescriptorPool(m_descriptor_pool);
        m_descriptor_sets.clear();
    }

    void Descriptor_pool::reset()
    {
        m_gfx_device.m_device.resetDescriptorPool(m_descriptor_pool);
        m_descriptor_sets.clear();
    }

    vk::DescriptorSet& Descriptor_pool::create_descriptor_set(
        const vk::DescriptorSetLayout& layout, uint32_t variable_descriptor_count)
    {
        auto&& device = m_gfx_device.m_device;

        vk::DescriptorSetAllocateInfo alloc_info{};
        alloc_info.descriptorPool     = m_descriptor_pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts        = &layout;

        vk::DescriptorSetVariableDescriptorCountAllocateInfo variable_count_info{};
        if (variable_descriptor_count > 0) {
            variable_count_info.descriptorSetCount = 1;
            variable_count_info.pDescriptorCounts  = &variable_descriptor_count;
            alloc_info.pNext                       = &variable_count_info;
        }

        auto&& descriptor_sets = device.allocateDescriptorSets(alloc_info);
        return m_descriptor_sets.emplace_back(descriptor_sets[0]);
    }
} // namespace VKN
