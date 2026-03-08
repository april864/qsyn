/****************************************************************************
  PackageName  [ device ]
  Synopsis     [ Floyd-Warshall and device analysis implementations ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "device/device_analysis.hpp"

#include <algorithm>
#include <cmath>
#include <ranges>

namespace qsyn::device {

float default_floyd_warshall_cost(Device::QubitPair const& /*adj*/, Device const& /*device*/) {
    return 1.f;
}

/**
 * @brief Additive cost function for Floyd-Warshall algorithm.
 *        Cost is -log(1 - error) for the first gate info of the adjacency.
 *        Uses a minimum cost so that perfect gates (error 0) still distinguish
 *        path length; clamps to avoid log(0) or log(negative).
 * @param adj Adjacency pair
 * @param device Device
 * @return Cost
 */
float log_success_rate_floyd_warshall_cost(Device::QubitPair const& adj, Device const& device) {
    // assumes the first gate info is the only one for this adjacency
    auto const& gate_info = device.get_2q_gate_info_map().at(adj)[0];

    if (1.f - gate_info.error <= 0.f) {
        return std::numeric_limits<float>::infinity();
    }

    return -std::log2(1.f - gate_info.error);
}

/**
 * @brief Floyd-Warshall Algorithm. Solve All Pairs Shortest Path (APSP)
 *
 * @param device Physical qubit adjacency information
 * @param cost_fn Function to compute the cost of an adjacency. If not provided, the cost is assumed to be 1 for all adjacencies.
 */
APSPResult floyd_warshall(
    Device const& device,
    std::function<float(Device::QubitPair const&, Device const&)> const& cost_fn) {
    auto const n = device.get_num_qubits();

    constexpr float inf = std::numeric_limits<float>::infinity();
    APSPResult result;
    result.distance.assign(n, std::vector<float>(n, inf));
    result.predecessor.assign(n, std::vector<std::optional<QubitIdType>>(n, std::nullopt));

    for (size_t i = 0; i < n; i++) {
        result.distance[i][i] = 0.f;
    }

    for (auto const& [adj, _] : device.get_2q_gate_info_map()) {
        auto const& [i, j] = adj;
        // if the error rate is 1, the cost should be infinity
        // regardless of the cost function. Even for noise-agnostic qubit
        // mappings, allowing using these couplings are just too ridiculous.
        auto const cost = (device.get_2q_gate_info_map().at(adj)[0].error == 1.f)
                              ? std::numeric_limits<float>::infinity()
                              : cost_fn(adj, device);

        result.distance[i][j]    = cost;
        result.predecessor[i][j] = i;
    }

    for (size_t k = 0; k < n; k++) {
        for (size_t i = 0; i < n; i++) {
            for (size_t j = 0; j < n; j++) {
                if (std::isinf(result.distance[i][k]) || std::isinf(result.distance[k][j])) {
                    continue;
                }
                auto const through_k = result.distance[i][k] + result.distance[k][j];
                if (std::isinf(result.distance[i][j]) || result.distance[i][j] > through_k) {
                    result.distance[i][j]    = through_k;
                    result.predecessor[i][j] = result.predecessor[k][j];
                }
            }
        }
    }

    return result;
}

std::vector<float> get_eccentricities(APSPResult const& apsp, Device const& device) {
    std::vector<float> eccentricities(device.get_num_qubits(), std::numeric_limits<float>::lowest());
    for (size_t i = 0; i < device.get_num_qubits(); i++) {
        for (size_t j = 0; j < device.get_num_qubits(); j++) {
            if (std::isinf(apsp.distance[i][j])) continue;
            eccentricities[i] = std::max(eccentricities[i], apsp.distance[i][j]);
        }
    }
    return eccentricities;
}

/**
 * @brief Get the centers of the device, i.e., arg min(max_i d(i, j)) where j is all other qubits
 * @param eccentricities Eccentricities of the qubits
 * @param filter_fn Function to filter the qubits. A qubit is only considered if filter_fn(qubit_id) returns true.
 * @return Centers
 */
std::vector<QubitIdType> get_centers(
    std::vector<float> const& eccentricities,
    std::function<bool(QubitIdType const&)> const& filter_fn) {
    auto radius = std::numeric_limits<float>::infinity();
    for (size_t i = 0; i < eccentricities.size(); i++) {
        if (!filter_fn(i)) continue;
        radius = std::min(radius, eccentricities[i]);
    }
    std::vector<QubitIdType> centers;
    for (size_t i = 0; i < eccentricities.size(); i++) {
        if (!filter_fn(i)) continue;
        if (eccentricities[i] == radius) {
            centers.push_back(i);
        }
    }
    return centers;
}

/**
 * @brief Get the centers of the device, i.e., arg min(max_i d(i, j)) where j is all other qubits
 * @param apsp APSPResult
 * @param device Device
 * @param filter_fn Function to filter the qubits. A qubit is only considered if filter_fn(qubit_id) returns true.
 * @return Centers
 */
std::vector<QubitIdType>
get_centers(
    APSPResult const& apsp, Device const& device,
    std::function<bool(QubitIdType const&)> const& filter_fn) {
    auto const eccentricities = get_eccentricities(apsp, device);
    return get_centers(eccentricities, filter_fn);
}

std::optional<std::vector<QubitIdType>> get_shortest_path(APSPResult const& apsp, QubitIdType src, QubitIdType dest) {
    std::vector<QubitIdType> path;
    path.push_back(src);
    while (src != dest) {
        if (!apsp.predecessor[dest][src].has_value()) return std::nullopt;
        src = apsp.predecessor[dest][src].value();
        path.push_back(src);
    }
    return path;
}

/**
 * @brief Get the connected components of the device.
 * @param apsp APSPResult. This is used to check if two qubits are effectively
 *        connected. For example, if two qubits are connected but the APSP
 *        distance is infinite (e.g., due to broken couplings),
 *        this function will treat them as disconnected.
 * @param device Device
 * @return A vector of vector of qubit ID, where each vector contains qubits
           that can reach each other.
 */
std::vector<std::vector<QubitIdType>>
get_connected_components(APSPResult const& apsp, Device const& device) {
    std::vector<std::vector<QubitIdType>> connected_components;

    std::vector<bool> visited(device.get_num_qubits(), false);

    for (size_t i = 0; i < device.get_num_qubits(); i++) {
        if (visited[i]) continue;
        // do DFS to find all qubits that can reach i
        connected_components.push_back(std::vector<QubitIdType>());
        std::vector<QubitIdType> stack;
        stack.push_back(i);
        while (!stack.empty()) {
            auto const current = stack.back();
            stack.pop_back();
            if (visited[current]) continue;
            visited[current] = true;
            connected_components.back().push_back(current);
            for (auto const& neighbor : device.get_adjacencies(current)) {
                if (std::isinf(apsp.distance[current][neighbor])) continue;
                if (!visited[neighbor]) stack.push_back(neighbor);
            }
        }
        std::ranges::sort(connected_components.back());
    }
    return connected_components;
}

}  // namespace qsyn::device
