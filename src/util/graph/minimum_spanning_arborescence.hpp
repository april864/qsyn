#pragma once

#include <fmt/format.h>

#include <algorithm>
#include <functional>
#include <unordered_set>

#include "util/graph/digraph.hpp"
#include "util/util.hpp"

namespace dvlab {

namespace detail {

// Find a cycle in the graph if there is one.
// Assumes the graph has at most n - 1 edges.
template <typename VertexAttr, typename CostType>
std::optional<std::vector<typename Digraph<VertexAttr, CostType>::Vertex>>
find_cycle(Digraph<VertexAttr, CostType> const& g) {
    using VertexT = typename Digraph<VertexAttr, CostType>::Vertex;
    for (auto const& v : g.vertices()) {
        auto visited = std::unordered_set<VertexT>{};
        auto stack   = std::vector<VertexT>{v};
        while (!stack.empty()) {
            auto w = stack.back();
            stack.pop_back();
            if (visited.contains(w)) {
                auto cycle = std::vector<VertexT>{};
                while (cycle.empty() || cycle.front() != w) {
                    cycle.push_back(w);
                    w = *g.in_neighbors(w).begin();
                }
                return cycle;
            }
            visited.insert(w);
            for (auto const& u : g.out_neighbors(w)) {
                stack.push_back(u);
            }
        }
    }
    return std::nullopt;
}

template <typename VertexAttr, typename CostType>
Digraph<VertexAttr, CostType>
build_min_edge_subgraph(
    Digraph<VertexAttr, CostType> const& g,
    typename Digraph<VertexAttr, CostType>::Vertex const& root) {
    using DigraphT = Digraph<VertexAttr, CostType>;
    using EdgeT    = typename DigraphT::Edge;

    std::vector<EdgeT> edges;

    for (auto const& v : g.vertices()) {
        if (v == root) continue;
        auto const w = *std::ranges::min_element(
            g.in_neighbors(v),
            std::less{},
            [&](auto const& w) { return g[{w, v}]; });
        edges.emplace_back(w, v);
    }

    auto mst = DigraphT{};
    for (auto const& v : g.vertices()) {
        mst.add_vertex_with_id(v);
    }
    for (auto const& e : edges) {
        mst.add_edge(e, g[e]);
    }
    return mst;
}

}  // namespace detail

/**
 * Build a minimum spanning arborescence rooted at 'root'.
 *
 * Assumption: the input graph is (weakly) connected; otherwise Edmonds'
 * algorithm is not well-defined for a single global arborescence.
 *
 * @param g The graph to build the MST of.
 * @param root The root vertex of the MST.
 * @return The MST.
 */
template <typename VertexAttr, typename CostType>
requires std::signed_integral<CostType> || std::floating_point<CostType>
Digraph<VertexAttr, CostType>
minimum_spanning_arborescence(
    Digraph<VertexAttr, CostType> const& g,
    typename Digraph<VertexAttr, CostType>::Vertex const& root) {
    using DigraphT = Digraph<VertexAttr, CostType>;
    using VertexT  = typename DigraphT::Vertex;

    auto min_edges = detail::build_min_edge_subgraph(g, root);

    auto const cycle = detail::find_cycle(min_edges);
    if (!cycle.has_value()) {
        // already a mst
        return min_edges;
    }

    // build a graph with the cycle replaced by a single vertex
    auto g_prime = g;

    for (auto const& v : *cycle) {
        g_prime.remove_vertex(v);
    }

    auto const v_cycle = g_prime.add_vertex();

    auto const is_in_cycle = [&](auto const& v) {
        return !g_prime.has_vertex(v);
    };

    std::unordered_map<VertexT, VertexT>
        v_cycle_in_idx;  // records the original successors for edge
                         // sources that point into the cycle
    std::unordered_map<VertexT, VertexT>
        v_cycle_out_idx;  // records the original predecessors for edge
                          // destinations that point out of the cycle

    for (auto const& u : g.vertices()) {
        for (auto const& v : g.out_neighbors(u)) {
            // case 1: (u, v) points into the cycle
            if (!is_in_cycle(u) && is_in_cycle(v)) {
                auto const pred_in_cycle = *min_edges.in_neighbors(v).begin();
                auto const new_weight    = g[{u, v}] + min_edges[{pred_in_cycle, v}];
                if (!g_prime.has_edge(u, v_cycle)) {
                    g_prime.add_edge(u, v_cycle, new_weight);
                    v_cycle_in_idx[u] = v;
                } else {
                    if (new_weight < g_prime[{u, v_cycle}]) {
                        g_prime[{u, v_cycle}] = new_weight;
                        v_cycle_in_idx[u]     = v;
                    }
                }
            }
            // case 2: (u, v) points out of the cycle
            else if (is_in_cycle(u) && !is_in_cycle(v)) {
                auto const weight = g[{u, v}];
                if (!g_prime.has_edge(v_cycle, v)) {
                    g_prime.add_edge(v_cycle, v, weight);
                    v_cycle_out_idx[v] = u;
                } else {
                    if (weight < g_prime[{v_cycle, v}]) {
                        g_prime[{v_cycle, v}] = weight;
                        v_cycle_out_idx[v]    = u;
                    }
                }
            }
            // Case 3: If (u, v) is a part of the loop, it is removed in
            // g_prime, so we do nothing. If neither u nor v is in the cycle,
            // the edge is unaffected.
        }
    }

    // find mst of g_prime
    auto mst = minimum_spanning_arborescence(g_prime, root);

    for (auto const& v : *cycle) {
        mst.add_vertex_with_id(v);
    }

    // restore the cycle edges
    for (auto const& node_in : mst.out_neighbors(v_cycle)) {
        auto const orig_out = v_cycle_out_idx.at(node_in);
        assert(g.has_vertex(orig_out));
        mst.add_edge(orig_out, node_in, g[{orig_out, node_in}]);
    }

    DVLAB_ASSERT(mst.in_degree(v_cycle) == 1,
                 "In-degree to the cycle vertex must be 1");

    auto const src = *mst.in_neighbors(v_cycle).begin();

    auto const orig_in = v_cycle_in_idx.at(src);
    mst.add_edge(src, orig_in, g[{src, orig_in}]);
    auto n = mst.remove_vertex(v_cycle);
    DVLAB_ASSERT(n == 1, "Must remove exactly one vertex");

    for (auto const& v : *cycle) {
        if (v == orig_in) continue;
        auto const src = *min_edges.in_neighbors(v).begin();
        mst.add_edge(src, v, min_edges[{src, v}]);
    }

    DVLAB_ASSERT(mst.num_edges() == g.num_vertices() - 1,
                 "MST must have n - 1 edges");

    return mst;
}

/**
 * Build a minimum spanning arborescence of the graph. This function will try
 * all possible roots and return the one that minimizes the total weight of the
 * MST.
 *
 * Assumption: the input graph is (weakly) connected; otherwise there is no
 * single global arborescence spanning all vertices.
 *
 * @param g The graph to build the MST of.
 * @return The MST and the root vertex.
 */
template <typename VertexAttr, typename CostType>
requires std::signed_integral<CostType> || std::floating_point<CostType>
std::pair<Digraph<VertexAttr, CostType>,
          typename Digraph<VertexAttr, CostType>::Vertex>
minimum_spanning_arborescence(
    Digraph<VertexAttr, CostType> const& g) {
    using DigraphT = Digraph<VertexAttr, CostType>;
    using VertexT  = typename DigraphT::Vertex;
    using EdgeT    = typename DigraphT::Edge;

    auto const default_cost_fn = [&](EdgeT const& e) { return g[e]; };

    auto const total_weight = [&](DigraphT const& g_mst) {
        auto sum = CostType{0};
        for (auto const& v : g_mst.vertices()) {
            for (auto const& e : g_mst.out_edges(v)) {
                sum += default_cost_fn(e);
            }
        }
        return sum;
    };

    auto cost = std::numeric_limits<CostType>::max();
    auto mst  = DigraphT{};
    auto root = VertexT{};
    for (auto const& v : g.vertices()) {
        auto const mst_prime = minimum_spanning_arborescence(g, v);
        auto const new_cost  = total_weight(mst_prime);
        if (new_cost < cost) {
            cost = new_cost;
            mst  = mst_prime;
            root = v;
        }
    }

    return {mst, root};
}

/**
 * Build a minimum spanning arborescence of the graph using a custom cost function.
 *
 * @param g The graph to build the MST of.
 * @param cost_fn The cost function to use.
 * @return The MST and the root vertex.
 */
template <typename VertexAttr, typename EdgeAttr, typename CostFn>
std::pair<Digraph<VertexAttr, EdgeAttr>,
          typename Digraph<VertexAttr, EdgeAttr>::Vertex>
minimum_spanning_arborescence_with_cost(
    Digraph<VertexAttr, EdgeAttr> const& g,
    CostFn const& cost_fn) {
    using InputDigraphT = Digraph<VertexAttr, EdgeAttr>;
    using EdgeT         = typename InputDigraphT::Edge;
    using CostType      = typename std::decay_t<decltype(cost_fn(std::declval<EdgeT>()))>;
    using CostDigraphT  = Digraph<VertexAttr, CostType>;
    // using VertexT       = typename CostDigraphT::Vertex;

    // Build a cost-weighted graph using the custom cost function.
    // We create a new graph with the costs because Edmonds' algorithm adds
    // new vertices to the graph during the process, rendering the cost function
    // ill-defined.
    CostDigraphT cost_graph;
    for (auto const& v : g.vertices()) {
        cost_graph.add_vertex_with_id(v);
    }
    for (auto const& u : g.vertices()) {
        for (auto const& v : g.out_neighbors(u)) {
            EdgeT const e{u, v};
            cost_graph.add_edge(e, cost_fn(e));
        }
    }

    // Compute MST on the cost-weighted graph using the standard algorithm.
    auto const [mst_cost_graph, root] = minimum_spanning_arborescence(cost_graph);

    // Return the MST (with cost-based edge weights) and chosen root.
    return {mst_cost_graph, root};
}

}  // namespace dvlab
