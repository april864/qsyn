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
                     a, b, _2q_gate_info.at(query)[0].time.count(), _2q_gate_info.at(query)[0].error);
    } else {
        fmt::println("No connection between {:>3} and {:>3}.", a, b);
    }
}

std::string Device::info_string() const {
    return fmt::format("Device: {} ({} qubits)\n- Gate set: [{}]", _name, _adjacency_map.size(), fmt::join(_gate_set, ", "));
}

std::optional<std::string> Device::gate_info_string(std::size_t qubit_id) const {
    if (!_1q_gate_info.contains(qubit_id)) {
        return std::nullopt;
    }
    std::string result = fmt::format("Qubit {}:\n", qubit_id);
    for (auto const& [gate_idx, time, error] : _1q_gate_info.at(qubit_id)) {
        // NOTE (Mu-Te): I think the longest gate name is "measure" with 7
        // characters. So we use 8 characters for padding.
        result += fmt::format("- {:>8}: Delay: {:>4.3} (ns)    Error: {:<.3e}\n", _gate_set[gate_idx], time.count(), error);
    }
    return result;
}

tl::expected<std::string, TwoQubitGateInfoAccessError> Device::gate_info_string(QubitPair const& qubit_pair) const {
    if (!_2q_gate_info.contains(qubit_pair)) {
        return tl::unexpected(TwoQubitGateInfoAccessError::invalid_qubit_pair);
    }
    if (!_1q_gate_info.contains(qubit_pair.first)) {
        return tl::unexpected(TwoQubitGateInfoAccessError::invalid_first_qubit_id);
    }
    if (!_1q_gate_info.contains(qubit_pair.second)) {
        return tl::unexpected(TwoQubitGateInfoAccessError::invalid_second_qubit_id);
    }
    std::string result = fmt::format("Adjacency ({}, {}):\n", qubit_pair.first, qubit_pair.second);
    for (auto const& [gate_idx, time, error] : _2q_gate_info.at(qubit_pair)) {
        // NOTE (Mu-Te): I think the longest gate name is "measure" with 7
        // characters. So we use 8 characters for padding.
        result += fmt::format("- {:>8}: Delay: {:>4.3} (ns)    Error: {:<.3e}\n", _gate_set[gate_idx], time.count(), error);
    }
    return result;
}

}  // namespace qsyn::device
