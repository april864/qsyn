/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ PhysicalQubitState and DeviceState implementations ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "duostra/device_state.hpp"

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <spdlog/spdlog.h>

#include <cassert>
#include <ranges>
#include <string>
#include <tl/enumerate.hpp>
#include <utility>

#include "device/device_analysis.hpp"
#include "qcir/basic_gate_type.hpp"
#include "qcir/qcir_gate.hpp"
#include "qsyn/qsyn_type.hpp"
#include "util/util.hpp"

using namespace qsyn::qcir;

namespace qsyn::duostra {

/**
 * @brief TEMPORARY SOLUTION! get gate delay as used by the Duostra algorithm
 *        This function should be rewritten to use the actual device information
 */
size_t DeviceState::get_delay(qcir::QCirGate const& inst) const {
    if (inst.get_operation().get_type() == "swap") {
        return 6;
    } else if (inst.get_qubits().size() == 1) {
        return 1;
    } else if (inst.get_qubits().size() == 2) {
        return 2;
    } else {
        return 5;
    }
}

std::ostream& operator<<(std::ostream& os, PhysicalQubitState const& q) {
    return os << fmt::format("{}", q);
}

void PhysicalQubitState::mark(bool source, QubitIdType pred) {
    _marked = true;
    _source = source;
    _pred   = pred;
}

void PhysicalQubitState::take_route(size_t cost, size_t swap_time) {
    _cost      = cost;
    _swap_time = swap_time;
    _taken     = true;
}

void PhysicalQubitState::reset() {
    _marked = false;
    _taken  = false;
    _cost   = _occupied_time;
}

DeviceState::DeviceState(device::Device device) : _device(std::make_shared<device::Device>(std::move(device))) {
    _qubit_list.reserve(_device->get_num_qubits());
    for (size_t i = 0; i < _device->get_num_qubits(); ++i) {
        _qubit_list.emplace_back(PhysicalQubitState(i));
    }
    calculate_path();
}

std::tuple<QubitIdType, QubitIdType> DeviceState::get_next_swap_cost(QubitIdType source, QubitIdType target) {
    DVLAB_ASSERT(static_cast<bool>(_apsp), "APSPResult not initialized; call calculate_path() first.");
    auto const& predecessor = _apsp->predecessor;
    auto const next_idx_opt = predecessor[target][source];
    DVLAB_ASSERT(next_idx_opt.has_value(), fmt::format("No path between {} and {} in get_next_swap_cost()", source, target));
    auto const next_idx  = next_idx_opt.value();
    auto const& q_source = get_physical_qubit(source);
    auto const& q_next   = get_physical_qubit(next_idx);
    auto const cost      = std::max(q_source.get_occupied_time(), q_next.get_occupied_time());

    assert(_device->is_adjacency(source, next_idx));
    return {next_idx, cost};
}

QubitIdType DeviceState::get_physical_by_logical(QubitIdType id) {
    for (auto& phy : _qubit_list) {
        if (phy.get_logical_qubit() == id) {
            return phy.get_id();
        }
    }
    return max_qubit_id;
}

void DeviceState::apply_gate(qcir::QCirGate const& op, size_t time_begin) {
    auto qubits = op.get_qubits();
    auto& q0    = get_physical_qubit(qubits[0]);
    auto& q1    = get_physical_qubit(qubits[1]);

    if (op.get_operation() == SwapGate{}) {
        auto temp = q0.get_logical_qubit();
        q0.set_logical_qubit(q1.get_logical_qubit());
        q1.set_logical_qubit(temp);
        q0.set_occupied_time(time_begin + get_delay(op));
        q1.set_occupied_time(time_begin + get_delay(op));
    } else if (op.get_num_qubits() == 2) {
        q0.set_occupied_time(time_begin + get_delay(op));
        q1.set_occupied_time(time_begin + get_delay(op));
    } else {
        DVLAB_ASSERT(false, fmt::format("Unknown gate type ({}) at apply_gate()!!", op.get_operation().get_repr()));
    }
}

std::vector<std::optional<size_t>> DeviceState::mapping() const {
    std::vector<std::optional<size_t>> ret;
    ret.resize(_qubit_list.size());
    for (auto const& [id, qubit] : tl::views::enumerate(_qubit_list)) {
        ret[id] = qubit.get_logical_qubit();
    }
    return ret;
}

void DeviceState::place(std::vector<QubitIdType> const& assignment) {
    for (size_t i = 0; i < assignment.size(); ++i) {
        assert(_qubit_list[assignment[i]].get_logical_qubit() == std::nullopt);
        _qubit_list[assignment[i]].set_logical_qubit(i);
    }
}

void DeviceState::calculate_path() {
    if (!_apsp) {
        _apsp = std::make_shared<device::APSPResult<std::size_t>>(device::floyd_warshall<std::size_t>(*_device));
    }
}

std::vector<PhysicalQubitState> DeviceState::get_path(QubitIdType src, QubitIdType dest) const {
    std::vector<PhysicalQubitState> path;
    path.emplace_back(_qubit_list.at(src));
    if (src == dest) return path;
    if (!_apsp) {
        return path;
    }
    auto const& predecessor = _apsp->predecessor;
    auto pred_opt           = predecessor[dest][src];
    if (!pred_opt.has_value()) {
        return path;
    }
    auto new_pred = pred_opt.value();
    path.emplace_back(_qubit_list.at(new_pred));
    while (true) {
        pred_opt = predecessor[dest][new_pred];
        if (!pred_opt.has_value()) break;
        new_pred = pred_opt.value();
        path.emplace_back(_qubit_list.at(new_pred));
    }
    return path;
}

}  // namespace qsyn::duostra
