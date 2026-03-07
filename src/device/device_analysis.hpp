/****************************************************************************
  PackageName  [ device ]
  Synopsis     [ Floyd-Warshall and device analysis utilities ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <functional>
#include <limits>
#include <optional>
#include <vector>

#include "device/device.hpp"
#include "qsyn/qsyn_type.hpp"

namespace qsyn::device {

template <typename CostType>
struct APSPResult {
    std::vector<std::vector<std::optional<QubitIdType>>> predecessor;
    std::vector<std::vector<std::optional<CostType>>> distance;
};

template <typename CostType>
CostType default_floyd_warshall_cost(Device::QubitPair const& /*adj*/, Device const& /*device*/) {
    return CostType{1};
}

float log_success_rate_floyd_warshall_cost(Device::QubitPair const& adj, Device const& device);

/**
 * @brief Floyd-Warshall Algorithm. Solve All Pairs Shortest Path (APSP)
 *
 * @tparam CostType Type of the cost.
 * @param device Physical qubit adjacency information
 * @param cost_fn Function to compute the cost of an adjacency. If not provided, the cost is assumed to be 1 for all adjacencies.
 */
template <typename CostType>
APSPResult<CostType>
floyd_warshall(
    Device const& device,
    std::function<CostType(Device::QubitPair const&, Device const&)> const& cost_fn =
        default_floyd_warshall_cost<CostType>) {
    auto const n = device.get_num_qubits();

    APSPResult<CostType> result;
    result.distance.assign(n, std::vector<std::optional<CostType>>(n, std::nullopt));
    result.predecessor.assign(n, std::vector<std::optional<QubitIdType>>(n, std::nullopt));

    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            if (i == j) {
                result.distance[i][j]    = CostType{0};
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

template <typename CostType>
std::vector<CostType> get_eccentricities(APSPResult<CostType> const& apsp, Device const& device) {
    // use lowest() so max(distance) correctly replaces initial value for float (min() is smallest positive)
    std::vector<CostType> eccentricities(device.get_num_qubits(), std::numeric_limits<CostType>::lowest());
    for (size_t i = 0; i < device.get_num_qubits(); i++) {
        for (size_t j = 0; j < device.get_num_qubits(); j++) {
            if (!apsp.distance[i][j].has_value()) continue;
            eccentricities[i] = std::max(eccentricities[i], apsp.distance[i][j].value());
        }
    }
    return eccentricities;
}

/**
 * @brief Get the centers of the device, i.e., arg min(max_i d(i, j)) where j is all other qubits
 * @param apsp APSPResult
 * @param device Device
 * @return Centers
 */
template <typename CostType>
std::vector<QubitIdType> get_centers(APSPResult<CostType> const& apsp, Device const& device) {
    auto const eccentricities = get_eccentricities(apsp, device);
    auto const radius         = std::ranges::min(eccentricities);

    std::vector<QubitIdType> centers;
    for (size_t i = 0; i < device.get_num_qubits(); i++) {
        if (eccentricities[i] == radius) {
            centers.push_back(i);
        }
    }
    return centers;
}

template <typename CostType>
std::optional<std::vector<QubitIdType>> get_shortest_path(APSPResult<CostType> const& apsp, QubitIdType src, QubitIdType dest) {
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
