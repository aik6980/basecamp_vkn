#include "framegraph.h"

#include "framegraph.h"

#include <algorithm>
#include <queue>

void Frame_graph::clear()
{
    m_passes.clear();
    m_execution_order.clear();
    m_edges.clear();
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
    m_execution_order.clear();
    m_edges.clear();

    const uint32_t n = static_cast<uint32_t>(m_passes.size());
    std::vector<uint32_t> indegree(n, 0);
    std::vector<std::vector<uint32_t>> adjacency(n);

    for (uint32_t i = 0; i < n; ++i) {
        auto collect_a = m_passes[i].reads;
        collect_a.insert(collect_a.end(), m_passes[i].writes.begin(), m_passes[i].writes.end());

        for (uint32_t j = i + 1; j < n; ++j) {
            auto collect_b = m_passes[j].reads;
            collect_b.insert(collect_b.end(), m_passes[j].writes.begin(), m_passes[j].writes.end());

            bool linked = false;
            for (const auto& ua : collect_a) {
                for (const auto& ub : collect_b) {
                    if (!is_hazard(ua, ub)) {
                        continue;
                    }
                    if (!linked) {
                        adjacency[i].push_back(j);
                        indegree[j] += 1;
                        linked = true;
                    }
                    m_edges.push_back(Edge{i, j, ua, ub});
                }
            }
        }
    }

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

    if (m_execution_order.size() != n) {
        m_execution_order.clear();
        for (uint32_t i = 0; i < n; ++i) {
            m_execution_order.push_back(i);
        }
    }
}

void Frame_graph::execute(vk::CommandBuffer& cmd)
{
    if (m_execution_order.empty()) {
        compile();
    }

    std::unordered_map<uint32_t, uint32_t> pass_rank;
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_execution_order.size()); ++i) {
        pass_rank[m_execution_order[i]] = i;
    }

    for (uint32_t order_i = 0; order_i < static_cast<uint32_t>(m_execution_order.size()); ++order_i) {
        const uint32_t pass_id = m_execution_order[order_i];

        for (const auto& e : m_edges) {
            if (e.to != pass_id) {
                continue;
            }
            if (pass_rank[e.from] >= pass_rank[e.to]) {
                continue;
            }

            vk::MemoryBarrier2 barrier{
                .srcStageMask  = e.from_use.stage,
                .srcAccessMask = e.from_use.access,
                .dstStageMask  = e.to_use.stage,
                .dstAccessMask = e.to_use.access,
            };

            vk::DependencyInfo dep{
                .memoryBarrierCount = 1,
                .pMemoryBarriers    = &barrier,
            };
            cmd.pipelineBarrier2(dep);
        }

        auto& pass = m_passes[pass_id];
        if (pass.execute) {
            pass.execute(cmd);
        }
    }
}