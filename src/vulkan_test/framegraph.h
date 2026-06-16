#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

enum class PassType { Compute, Raster };

struct ResourceUse {
    uint32_t resource_id          = 0;
    bool is_write                 = false;
    vk::ImageLayout layout        = vk::ImageLayout::eUndefined;
    vk::AccessFlags2 access       = vk::AccessFlagBits2::eNone;
    vk::PipelineStageFlags2 stage = vk::PipelineStageFlagBits2::eTopOfPipe;

    // Optional image metadata for layout/barrier ownership.
    bool is_image   = false;
    vk::Image image = {};
    vk::ImageSubresourceRange image_range{
        .aspectMask     = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
    };
};

struct PassNode {
    std::string name;
    PassType type = PassType::Raster;
    std::vector<ResourceUse> reads;
    std::vector<ResourceUse> writes;
    std::function<void(vk::CommandBuffer&)> execute;
};

class Frame_graph {
  public:
    void clear();
    uint32_t add_pass(PassNode pass);
    void compile();
    void execute(vk::CommandBuffer& cmd);

  private:
    struct Edge {
        uint32_t from = 0;
        uint32_t to   = 0;
        ResourceUse from_use{};
        ResourceUse to_use{};
    };

    static bool is_hazard(const ResourceUse& a, const ResourceUse& b);

    std::vector<PassNode> m_passes;
    std::vector<uint32_t> m_execution_order;

    // Resource state cache: survives clear() for cross-frame tracking.
    // Default-inserted entry has layout=eUndefined which is always valid as oldLayout.
    std::unordered_map<uint32_t, ResourceUse> m_resource_state_cache;
};