/****************************************************************************
  PackageName  [ device ]
  Synopsis     [ Define class Device, Topology, and Operation functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "device/device.hpp"

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <ranges>
#include <string>
#include <utility>

#include "qsyn/qsyn_type.hpp"
#include "util/dvlab_string.hpp"
#include "util/util.hpp"

namespace qsyn::device {

/**
 * @brief Floyd-Warshall Algorithm. Solve All Pairs Shortest Path (APSP)
 *
 * @param device Physical qubit adjacency information
 */
APSPResult floyd_warshall(Device const& device) {
    auto const n = device.get_num_qubits();

    APSPResult result;
    result.distance.assign(n, std::vector<std::optional<size_t>>(n, std::nullopt));
    result.predecessor.assign(n, std::vector<std::optional<QubitIdType>>(n, std::nullopt));

    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            if (i == j) {
                result.distance[i][j]    = 0;
                result.predecessor[i][j] = std::nullopt;
            }
        }
    }

    for (auto const& [adj, _] : device.get_2q_gate_info_map()) {
        auto const& [i, j]       = adj;
        result.distance[i][j]    = 1;
        result.distance[j][i]    = 1;
        result.predecessor[i][j] = i;
        result.predecessor[j][i] = j;
    }

    for (size_t k = 0; k < n; k++) {
        spdlog::debug("Including vertex({}):", k);
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

        spdlog::debug("Predecessor Matrix:");
        for (auto& row : result.predecessor) {
            spdlog::debug("{:5}", fmt::join(
                                      row | std::views::transform([](std::optional<QubitIdType> const& j) {
                                          return j.has_value() ? std::to_string(j.value()) : std::string{"/"};
                                      }),
                                      ""));
        }
        spdlog::debug("Distance Matrix:");
        for (auto& row : result.distance) {
            spdlog::debug("{:5}", fmt::join(
                                      row | std::views::transform([](std::optional<size_t> const& d) {
                                          return d.has_value() ? std::to_string(d.value()) : std::string{"X"};
                                      }),
                                      ""));
        }
    }

    return result;
}

// SECTION - Class Topology Member Functions

/**
 * @brief Get the information of a single adjacency pair
 *
 * @param a Id of first qubit
 * @param b Id of second qubit
 * @return Info&
 */
std::vector<GateInfo> const& Device::get_adjacency_pair_info(size_t a, size_t b) {
    if (a > b) std::swap(a, b);
    return _2q_gate_info[std::make_pair(a, b)];
}

/**
 * @brief Get the information of a qubit
 *
 * @param a
 * @return const Info&
 */
std::vector<GateInfo> const& Device::get_qubit_info(size_t a) {
    return _1q_gate_info[a];
}

/**
 * @brief Add adjacency information of (a,b)
 *
 * @param a Id of first qubit
 * @param b Id of second qubit
 * @param info Information of this pair
 */
void Device::add_gate_info(
    std::pair<size_t, size_t> const& qubit_id_pair, GateInfo info) {
    _adjacency_map[qubit_id_pair.first].emplace_back(qubit_id_pair.second);
    _2q_gate_info[qubit_id_pair].emplace_back(info);
}

/**
 * @brief Add qubit information
 *
 * @param a
 * @param info
 */
void Device::add_gate_info(size_t qubit_id, GateInfo info) {
    _1q_gate_info[qubit_id].emplace_back(info);
}

/**
 * @brief Print information of the edge (a,b)
 *
 * @param a Index of first qubit
 * @param b Index of second qubit
 */
void Device::print_single_edge(size_t a, size_t b) const {
    auto query = (a < b) ? std::make_pair(a, b) : std::make_pair(b, a);
    if (_2q_gate_info.contains(query)) {
        fmt::println("({:>3}, {:>3})    Delay: {:>8.3f}    Error: {:>8.5f}",
                     a, b, _2q_gate_info.at(query)[0].time, _2q_gate_info.at(query)[0].error);
    } else {
        fmt::println("No connection between {:>3} and {:>3}.", a, b);
    }
}

}  // namespace qsyn::device
