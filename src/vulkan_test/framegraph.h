#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>

enum class PassType { Compute, Raster };

struct ResourceUse {
    uint32_t resource_id = 0;
    bool is_write = false;
    vk::ImageLayout layout = vk::ImageLayout::eUndefined;
    vk::AccessFlags2 access = vk::AccessFlagBits2::eNone;
    vk::PipelineStageFlags2 stage = vk::PipelineStageFlagBits2::eTopOfPipe;
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
        uint32_t to = 0;
        ResourceUse from_use{};
        ResourceUse to_use{};
    };

    static bool is_hazard(const ResourceUse& a, const ResourceUse& b);

    std::vector<PassNode> m_passes;
    std::vector<uint32_t> m_execution_order;
    std::vector<Edge> m_edges;
};