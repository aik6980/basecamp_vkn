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

    // Resource ID intern API (name -> stable numeric id).
    uint32_t get_or_create_resource_id(const std::string& name);
    const std::string& resource_name(uint32_t id) const;

    // debug utilities:
    std::string build_debug_dot() const;
    std::string build_debug_mermaid() const;

  private:
    struct Edge {
        uint32_t from = 0;
        uint32_t to   = 0;
        ResourceUse from_use{};
        ResourceUse to_use{};
    };

    static bool is_hazard(const ResourceUse& a, const ResourceUse& b);
    std::string resource_label(uint32_t id) const;

    std::vector<PassNode> m_passes;
    std::vector<uint32_t> m_execution_order;

    // Resource state cache: survives clear() for cross-frame tracking.
    // Default-inserted entry has layout=eUndefined which is always valid as oldLayout.
    std::unordered_map<uint32_t, ResourceUse> m_resource_state_cache;

    // Name/ID intern tables, kept across frames for stable IDs and better debug labels.
    std::unordered_map<std::string, uint32_t> m_resource_name_to_id;
    std::unordered_map<uint32_t, std::string> m_resource_id_to_name;
    uint32_t m_next_resource_id = 1; // 0 reserved for invalid/unset
};