/****************************************************************************
  PackageName  [ util/graph ]
  Synopsis     [ Generic Floyd-Warshall all-pairs shortest path ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <unordered_map>
#include <vector>

namespace dvlab {

/**
 * @brief Result of all-pairs shortest path (Floyd-Warshall).
 *        Indices i, j correspond to vertices[i] and vertices[j].
 */
template <typename Vertex>
struct APSPResult {
    std::vector<Vertex> vertices;                                 // vertices[k] is the k-th vertex (sorted)
    std::vector<std::vector<float>> distance;                     // distance[i][j] = dist(vertices[i], vertices[j])
    std::vector<std::vector<std::optional<Vertex>>> predecessor;  // predecessor of vertices[j] on path from vertices[i]
};

/**
 * @brief Generic Floyd-Warshall for any digraph with vertices() and edges().
 * @param graph  Digraph (e.g. dvlab::Digraph<VA, EA>); must have vertices(), edges().
 * @param cost_fn  Callable (Edge e) -> float; use inf for absent or blocked edges.
 * @return APSPResult with sorted vertices; distance[i][j] uses infinity for no path.
 */
template <typename Graph, typename CostFn>
auto floyd_warshall(Graph const& graph, CostFn const& cost_fn) -> APSPResult<typename Graph::Vertex> {
    using Vertex        = typename Graph::Vertex;
    constexpr float inf = std::numeric_limits<float>::infinity();

    std::vector<Vertex> vertices(graph.vertices().begin(), graph.vertices().end());
    std::ranges::sort(vertices);

    size_t const n = vertices.size();
    std::unordered_map<Vertex, size_t> v2i;
    for (size_t i = 0; i < n; ++i) {
        v2i[vertices[i]] = i;
    }

    APSPResult<Vertex> result;
    result.vertices = vertices;
    result.distance.assign(n, std::vector<float>(n, inf));
    result.predecessor.assign(n, std::vector<std::optional<Vertex>>(n, std::nullopt));

    for (size_t i = 0; i < n; ++i) {
        result.distance[i][i] = 0.f;
    }

    for (auto const& e : graph.edges()) {
        auto it_src = v2i.find(e.src);
        auto it_dst = v2i.find(e.dst);
        if (it_src == v2i.end() || it_dst == v2i.end()) continue;
        size_t const i           = it_src->second;
        size_t const j           = it_dst->second;
        float const c            = cost_fn(e);
        result.distance[i][j]    = c;
        result.predecessor[i][j] = e.src;
    }

    for (size_t k = 0; k < n; ++k) {
        for (size_t i = 0; i < n; ++i) {
            if (std::isinf(result.distance[i][k])) continue;
            for (size_t j = 0; j < n; ++j) {
                if (std::isinf(result.distance[k][j])) continue;
                float const through = result.distance[i][k] + result.distance[k][j];
                if (std::isinf(result.distance[i][j]) || result.distance[i][j] > through) {
                    result.distance[i][j]    = through;
                    result.predecessor[i][j] = result.predecessor[k][j];
                }
            }
        }
    }

    return result;
}

}  // namespace dvlab
