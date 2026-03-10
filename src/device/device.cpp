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
    QubitPair const& qubit_id_pair, GateInfo info) {
    _graph.add_vertex_with_id(qubit_id_pair.src, std::vector<GateInfo>{});
    _graph.add_vertex_with_id(qubit_id_pair.dst, std::vector<GateInfo>{});
    if (!_graph.has_edge(qubit_id_pair)) {
        _graph.add_edge(qubit_id_pair.src, qubit_id_pair.dst, std::vector<GateInfo>{});
    }
    _graph.edge_attr(qubit_id_pair).emplace_back(info);
}

/**
 * @brief Add qubit information
 *
 * @param a
 * @param info
 */
void Device::add_gate_info(size_t qubit_id, GateInfo info) {
    _graph.add_vertex_with_id(qubit_id, std::vector<GateInfo>{});
    _graph.vertex_attr(qubit_id).emplace_back(info);
}

std::string Device::info_string() const {
    return fmt::format("Device: {} ({} qubits)\n- Gate set: [{}]", _name, _graph.num_vertices(), fmt::join(_gate_set, ", "));
}

std::optional<std::string> Device::gate_info_string(std::size_t qubit_id) const {
    if (!_graph.has_vertex(qubit_id)) {
        return std::nullopt;
    }
    std::string result = fmt::format("Qubit {} (adjacencies: [{}]):\n", qubit_id, fmt::join(get_adjacencies(qubit_id), ", "));
    for (auto const& [gate_idx, time, error] : _graph.vertex_attr(qubit_id)) {
        // NOTE (Mu-Te): I think the longest gate name is "measure" with 7
        // characters. So we use 8 characters for padding.
        result += fmt::format("- {:>8}: Delay: {:<.3e} (ns)    Error: {:<.3e}\n", _gate_set[gate_idx], time.count(), error);
    }
    return result;
}

tl::expected<std::string, TwoQubitGateInfoAccessError> Device::gate_info_string(QubitPair const& qubit_pair) const {
    if (!_graph.has_vertex(qubit_pair.src)) {
        return tl::unexpected(TwoQubitGateInfoAccessError::invalid_first_qubit_id);
    }
    if (!_graph.has_vertex(qubit_pair.dst)) {
        return tl::unexpected(TwoQubitGateInfoAccessError::invalid_second_qubit_id);
    }
    // this check must come after the above two checks to avoid undefined behavior
    if (!_graph.has_edge(qubit_pair)) {
        return tl::unexpected(TwoQubitGateInfoAccessError::invalid_qubit_pair);
    }
    std::string result = fmt::format("Adjacency ({}, {}):\n", qubit_pair.src, qubit_pair.dst);
    for (auto const& [gate_idx, time, error] : _graph.edge_attr(qubit_pair)) {
        // NOTE (Mu-Te): I think the longest gate name is "measure" with 7
        // characters. So we use 8 characters for padding.
        result += fmt::format("- {:>8}: Delay: {:<.3e} (ns)    Error: {:<.3e}\n", _gate_set[gate_idx], time.count(), error);
    }
    return result;
}

}  // namespace qsyn::device
