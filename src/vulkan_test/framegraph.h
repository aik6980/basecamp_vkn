#pragma once 

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