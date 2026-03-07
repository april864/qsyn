/****************************************************************************
  PackageName  [ device ]
  Synopsis     [ Floyd-Warshall and device analysis implementations ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "device/device_analysis.hpp"

#include <cmath>

namespace qsyn::device {

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

}  // namespace qsyn::device
