#include "framegraph.h"

#include <algorithm>
#include <queue>

void Frame_graph::clear()
{
    m_passes.clear();
    m_execution_order.clear();
    // m_resource_state_cache intentionally kept for cross-frame state tracking
}

uint32_t Frame_graph::add_pass(PassNode pass)
{
    m_passes.push_back(std::move(pass));
    return static_cast<uint32_t>(m_passes.size() - 1);
}

bool Frame_graph::is_hazard(const ResourceUse& a, const ResourceUse& b)
{
    if (a.resource_id != b.resource_id) {
        return false;
    }
    return a.is_write || b.is_write;
}

void Frame_graph::compile()
{
    // Rebuild derived graph data from current pass declarations.
    m_execution_order.clear();

    const uint32_t n = static_cast<uint32_t>(m_passes.size());
    std::vector<uint32_t> indegree(n, 0);
    std::vector<std::vector<uint32_t>> adjacency(n);

    // Build dependency edges from resource hazards:
    // - different resources: no dependency
    // - same resource + at least one write: ordering required
    for (uint32_t i = 0; i < n; ++i) {
        auto collect_a = m_passes[i].reads;
        collect_a.insert(collect_a.end(), m_passes[i].writes.begin(), m_passes[i].writes.end());

        for (uint32_t j = i + 1; j < n; ++j) {
            auto collect_b = m_passes[j].reads;
            collect_b.insert(collect_b.end(), m_passes[j].writes.begin(), m_passes[j].writes.end());

            // Check if any resource usage conflicts between pass i and j.
            bool has_hazard = false;
            for (const auto& ua : collect_a) {
                for (const auto& ub : collect_b) {
                    if (is_hazard(ua, ub)) {
                        has_hazard = true;
                        break;
                    }
                }
                if (has_hazard)
                    break; // Early exit both loops.
            }

            if (has_hazard) {
                adjacency[i].push_back(j);
                indegree[j] += 1;
            }
        }
    }

    // Kahn topological sort to generate execution order.
    std::queue<uint32_t> q;
    for (uint32_t i = 0; i < n; ++i) {
        if (indegree[i] == 0) {
            q.push(i);
        }
    }

    while (!q.empty()) {
        const uint32_t u = q.front();
        q.pop();
        m_execution_order.push_back(u);

        for (uint32_t v : adjacency[u]) {
            indegree[v] -= 1;
            if (indegree[v] == 0) {
                q.push(v);
            }
        }
    }

    // Fallback for cycles: preserve declared order so execution remains deterministic.
    if (m_execution_order.size() != n) {
        assert(false && "Frame_graph::compile produced incomplete topological order");
        m_execution_order.clear();
        for (uint32_t i = 0; i < n; ++i) {
            m_execution_order.push_back(i);
        }
    }
}

void Frame_graph::execute(vk::CommandBuffer& cmd)
{
    // Lazily compile if graph changed or has not been compiled yet.
    if (m_execution_order.empty()) {
        compile();
    }

    for (uint32_t order_i = 0; order_i < static_cast<uint32_t>(m_execution_order.size()); ++order_i) {
        const uint32_t pass_id = m_execution_order[order_i];
        auto& pass             = m_passes[pass_id];

        // Collect all resource uses for this pass.
        auto all_uses = pass.reads;
        all_uses.insert(all_uses.end(), pass.writes.begin(), pass.writes.end());

        for (const auto& use : all_uses) {
            // Default-insert gives eUndefined/eNone/eTopOfPipe — valid as barrier src.
            auto& cached = m_resource_state_cache[use.resource_id];

            const bool layout_changed = cached.layout != use.layout;
            const bool access_changed = cached.access != use.access;

            if ((layout_changed || access_changed) && use.is_image && use.image) {
                vk::ImageMemoryBarrier2 image_barrier{
                    .srcStageMask        = cached.stage,
                    .srcAccessMask       = cached.access,
                    .dstStageMask        = use.stage,
                    .dstAccessMask       = use.access,
                    .oldLayout           = cached.layout,
                    .newLayout           = use.layout,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image               = use.image,
                    .subresourceRange    = use.image_range,
                };
                vk::DependencyInfo dep{
                    .imageMemoryBarrierCount = 1,
                    .pImageMemoryBarriers    = &image_barrier,
                };
                cmd.pipelineBarrier2(dep);
            }

            // Update cache to reflect what this pass leaves the resource in.
            cached = use;
        }

        // Execute pass body after all incoming hazards for this pass are synchronized.
        if (pass.execute) {
            pass.execute(cmd);
        }
    }
}