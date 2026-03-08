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
 * @brief Add adjacency information of (a,b)
 *
 * @param a Id of first qubit
 * @param b Id of second qubit
 * @param info Information of this pair
 */
void Device::add_gate_info(
    std::pair<size_t, size_t> const& qubit_id_pair, GateInfo info) {
    if (!is_adjacency(qubit_id_pair.first, qubit_id_pair.second)) {
        _adjacency_map[qubit_id_pair.first].emplace_back(qubit_id_pair.second);
    }
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
    std::string result = fmt::format("Qubit {} (adjacencies: [{}]):\n", qubit_id, fmt::join(get_adjacencies(qubit_id), ", "));
    for (auto const& [gate_idx, time, error] : _1q_gate_info.at(qubit_id)) {
        // NOTE (Mu-Te): I think the longest gate name is "measure" with 7
        // characters. So we use 8 characters for padding.
        result += fmt::format("- {:>8}: Delay: {:<.3e} (ns)    Error: {:<.3e}\n", _gate_set[gate_idx], time.count(), error);
    }
    return result;
}

tl::expected<std::string, TwoQubitGateInfoAccessError> Device::gate_info_string(QubitPair const& qubit_pair) const {
    if (!_1q_gate_info.contains(qubit_pair.first)) {
        return tl::unexpected(TwoQubitGateInfoAccessError::invalid_first_qubit_id);
    }
    if (!_1q_gate_info.contains(qubit_pair.second)) {
        return tl::unexpected(TwoQubitGateInfoAccessError::invalid_second_qubit_id);
    }
    // this check must come after the above two checks to avoid undefined behavior
    if (!_2q_gate_info.contains(qubit_pair)) {
        return tl::unexpected(TwoQubitGateInfoAccessError::invalid_qubit_pair);
    }
    std::string result = fmt::format("Adjacency ({}, {}):\n", qubit_pair.first, qubit_pair.second);
    for (auto const& [gate_idx, time, error] : _2q_gate_info.at(qubit_pair)) {
        // NOTE (Mu-Te): I think the longest gate name is "measure" with 7
        // characters. So we use 8 characters for padding.
        result += fmt::format("- {:>8}: Delay: {:<.3e} (ns)    Error: {:<.3e}\n", _gate_set[gate_idx], time.count(), error);
    }
    return result;
}

}  // namespace qsyn::device
