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

    // clamp (1 - error) to (epsilon, 1] to gates prevent log(0) or log(negative)
    // this happens when error is 1 or close to 1 (bad couplings)
    constexpr float epsilon     = 1e-12f;
    float const one_minus_error = std::max(epsilon, 1.f - gate_info.error);

    // -log(1 - error) is small for low error; use minimum cost so all-zero costs don't make every node a center
    return std::max(epsilon, -std::log2(one_minus_error));
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

    APSPResult result;
    result.distance.assign(n, std::vector<std::optional<float>>(n, std::nullopt));
    result.predecessor.assign(n, std::vector<std::optional<QubitIdType>>(n, std::nullopt));

    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            if (i == j) {
                result.distance[i][j]    = 0.f;
                result.predecessor[i][j] = std::nullopt;
            }
        }
    }

    for (auto const& [adj, _] : device.get_2q_gate_info_map()) {
        auto const& [i, j]       = adj;
        result.distance[i][j]    = cost_fn(adj, device);
        result.predecessor[i][j] = i;
    }

    for (size_t k = 0; k < n; k++) {
        for (size_t i = 0; i < n; i++) {
            for (size_t j = 0; j < n; j++) {
                if (!result.distance[i][k].has_value() || !result.distance[k][j].has_value()) {
                    continue;
                }
                auto const through_k = result.distance[i][k].value() + result.distance[k][j].value();
                if (!result.distance[i][j].has_value() || result.distance[i][j].value() > through_k) {
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
            if (!apsp.distance[i][j].has_value()) continue;
            eccentricities[i] = std::max(eccentricities[i], apsp.distance[i][j].value());
        }
    }
    return eccentricities;
}

std::vector<QubitIdType> get_centers(std::vector<float> const& eccentricities) {
    auto const radius = std::ranges::min(eccentricities);
    std::vector<QubitIdType> centers;
    for (size_t i = 0; i < eccentricities.size(); i++) {
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
 * @return Centers
 */
std::vector<QubitIdType> get_centers(APSPResult const& apsp, Device const& device) {
    auto const eccentricities = get_eccentricities(apsp, device);
    return get_centers(eccentricities);
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

}  // namespace qsyn::device
