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
#include <tl/enumerate.hpp>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "qsyn/qsyn_type.hpp"
#include "util/dvlab_string.hpp"
#include "util/util.hpp"

namespace qsyn::device {

void Device::_ensure_qubit(QubitIdType qubit_id) {
    if (!_graph.has_vertex(qubit_id)) {
        _graph.add_vertex_with_id(qubit_id, QubitProperties{});
    }
}

void Device::set_qubit_t1(QubitIdType qubit_id, float t1_microseconds) {
    _ensure_qubit(qubit_id);
    _graph.vertex_attr(qubit_id).t1 = t1_microseconds;
}

void Device::set_qubit_t2(QubitIdType qubit_id, float t2_microseconds) {
    _ensure_qubit(qubit_id);
    _graph.vertex_attr(qubit_id).t2 = t2_microseconds;
}

void Device::set_readout_error(QubitIdType qubit_id, float readout_error) {
    _ensure_qubit(qubit_id);
    _graph.vertex_attr(qubit_id).readout_error = readout_error;
}

/**
 * @brief Add adjacency information of (a,b)
 *
 * @param a Id of first qubit
 * @param b Id of second qubit
 * @param info Information of this pair
 */
void Device::add_gate_info(
    QubitPair const& qubit_id_pair, GateInfo info) {
    _ensure_qubit(qubit_id_pair.src);
    _ensure_qubit(qubit_id_pair.dst);
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
    _ensure_qubit(qubit_id);
    _graph.vertex_attr(qubit_id).gate_infos.emplace_back(info);
}

namespace {

void append_qubit_properties_string(std::string& result, QubitProperties const& props) {
    if (props.t1.has_value()) {
        result += fmt::format("- T1: {:<.3e} (us)\n", *props.t1);
    }
    if (props.t2.has_value()) {
        result += fmt::format("- T2: {:<.3e} (us)\n", *props.t2);
    }
    if (props.readout_error.has_value()) {
        result += fmt::format("- readout_error: {:<.3e}\n", *props.readout_error);
    }
}

}  // namespace

std::string Device::info_string() const {
    return fmt::format("Device: {} ({} qubits)\n- Gate set: [{}]", _name, _graph.num_vertices(), fmt::join(_gate_set, ", "));
}

std::optional<std::string> Device::gate_info_string(std::size_t qubit_id) const {
    if (!_graph.has_vertex(qubit_id)) {
        return std::nullopt;
    }
    auto const& props  = get_qubit_properties(qubit_id);
    std::string result = fmt::format("Qubit {} (adjacencies: [{}]):\n", qubit_id, fmt::join(get_adjacencies(qubit_id), ", "));
    append_qubit_properties_string(result, props);
    for (auto const& info : props.gate_infos) {
        result += fmt::format(
            "- {:>8}: Delay: {:<.3e} (ns)    Error: {:<.3e}\n",
            gate_name(info),
            info.time.count(),
            info.error);
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
    for (auto const& info : _graph.edge_attr(qubit_pair)) {
        result += fmt::format(
            "- {:>8}: Delay: {:<.3e} (ns)    Error: {:<.3e}\n",
            gate_name(info),
            info.time.count(),
            info.error);
    }
    return result;
}

tl::expected<InducedSubdevice, InducedSubdeviceError> Device::induced_subdevice(
    std::span<QubitIdType const> physical_qubits) const {
    std::vector<QubitIdType> ordered_physical;
    ordered_physical.reserve(physical_qubits.size());
    std::unordered_set<QubitIdType> active;

    for (auto const q : physical_qubits) {
        if (!active.insert(q).second) {
            return tl::unexpected(InducedSubdeviceError::duplicate_qubit_id);
        }
        if (!_graph.has_vertex(q)) {
            return tl::unexpected(InducedSubdeviceError::unknown_qubit_id);
        }
        ordered_physical.push_back(q);
    }

    InducedSubdevice result{
        .device          = Device{},
        .physical_qubits = std::move(ordered_physical),
    };

    auto& sub     = result.device;
    sub._gate_set = _gate_set;
    if (_name.empty()) {
        sub.set_name(fmt::format("subdevice({})", fmt::join(result.physical_qubits, ",")));
    } else {
        sub.set_name(fmt::format("{}_sub", _name));
    }

    for (auto const [logical, physical] : tl::views::enumerate(result.physical_qubits)) {
        auto const& src_props = get_qubit_properties(physical);
        sub._ensure_qubit(logical);
        auto& dst_props         = sub._graph.vertex_attr(logical);
        dst_props.t1            = src_props.t1;
        dst_props.t2            = src_props.t2;
        dst_props.readout_error = src_props.readout_error;
        for (auto const& info : src_props.gate_infos) {
            dst_props.gate_infos.emplace_back(info);
        }
    }

    std::unordered_map<QubitIdType, QubitIdType> physical_to_logical;
    physical_to_logical.reserve(result.physical_qubits.size());
    for (auto const [logical, physical] : tl::views::enumerate(result.physical_qubits)) {
        physical_to_logical.emplace(physical, logical);
    }

    for (auto const physical_src : result.physical_qubits) {
        for (auto const physical_dst : get_adjacencies(physical_src)) {
            if (!active.contains(physical_dst)) {
                continue;
            }
            auto const logical_src = physical_to_logical.at(physical_src);
            auto const logical_dst = physical_to_logical.at(physical_dst);
            for (auto const& info : get_gate_info(QubitPair{physical_src, physical_dst})) {
                sub.add_gate_info(QubitPair{logical_src, logical_dst}, info);
            }
        }
    }

    return result;
}

}  // namespace qsyn::device
