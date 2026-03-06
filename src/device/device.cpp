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
#include <cassert>
#include <gsl/narrow>
#include <ranges>
#include <string>
#include <tl/enumerate.hpp>
#include <tl/to.hpp>
#include <utility>

#include "qcir/basic_gate_type.hpp"
#include "qcir/qcir_gate.hpp"
#include "qsyn/qsyn_type.hpp"
#include "util/dvlab_string.hpp"
#include "util/util.hpp"

using namespace qsyn::qcir;

template <>
struct fmt::formatter<qsyn::device::PhysicalQubitState> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(qsyn::device::PhysicalQubitState const& q, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "Q{:>2}, logical: {:>2}, lock until {}", q.get_id(), q.get_logical_qubit(), q.get_occupied_time());
    }
};

namespace qsyn::device {

/**
 * @brief TEMPORARY SOLUTION! get gate delay as used by the Duostra algorithm
 *        This function should be rewritten to use the actual device information
 *
 * @param inst
 * @return size_t
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
 * @brief Floyd-Warshall Algorithm. Solve All Pairs Shortest Path (APSP)
 *
 * @param qubit_list Physical qubit adjacency information
 */
APSPResult floyd_warshall(const Device& device) {
    auto const n = device.get_num_qubits();

    APSPResult result;
    result.distance.assign(n, std::vector<std::optional<size_t>>(n, std::nullopt));
    result.predecessor.assign(n, std::vector<std::optional<QubitIdType>>(n, std::nullopt));

    // Start with no paths except zero distance to self
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            if (i == j) {
                result.distance[i][j]    = 0;
                result.predecessor[i][j] = std::nullopt;
            }
        }
    }

    // Set weights of direct edges from adjacency information
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

// SECTION - Class PhysicalQubit Member Functions

/**
 * @brief Operator overloading
 *
 * @param os
 * @param q
 * @return ostream&
 */
std::ostream& operator<<(std::ostream& os, PhysicalQubitState const& q) {
    return os << fmt::format("{}", q);
}

/**
 * @brief Mark qubit
 *
 * @param source false: from 0, true: from 1
 * @param pred predecessor
 */
void PhysicalQubitState::mark(bool source, QubitIdType pred) {
    _marked = true;
    _source = source;
    _pred   = pred;
}

/**
 * @brief Take the route
 *
 * @param cost
 * @param swapTime
 */
void PhysicalQubitState::take_route(size_t cost, size_t swap_time) {
    _cost      = cost;
    _swap_time = swap_time;
    _taken     = true;
}

/**
 * @brief Reset qubit
 *
 */
void PhysicalQubitState::reset() {
    _marked = false;
    _taken  = false;
    _cost   = _occupied_time;
}

// SECTION - Class Device Member Functions

/**
 * @brief Get next swap cost
 *
 * @param source
 * @param target
 * @return tuple<size_t, size_t> (index of next qubit, cost)
 */
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

/**
 * @brief Get physical qubit id by logical id
 *
 * @param id logical
 * @return size_t
 */
QubitIdType DeviceState::get_physical_by_logical(QubitIdType id) {
    for (auto& phy : _qubit_list) {
        if (phy.get_logical_qubit() == id) {
            return phy.get_id();
        }
    }
    return max_qubit_id;
}

/**
 * @brief Apply gate to device
 *
 * @param op
 */
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

/**
 * @brief get mapping of physical qubit
 *
 * @return vector<size_t> (index of physical qubit)
 */
std::vector<std::optional<size_t>> DeviceState::mapping() const {
    std::vector<std::optional<size_t>> ret;
    ret.resize(_qubit_list.size());
    for (auto const& [id, qubit] : tl::views::enumerate(_qubit_list)) {
        ret[id] = qubit.get_logical_qubit();
    }
    return ret;
}

/**
 * @brief Place logical qubit
 *
 * @param assign
 */
void DeviceState::place(std::vector<QubitIdType> const& assignment) {
    for (size_t i = 0; i < assignment.size(); ++i) {
        assert(_qubit_list[assignment[i]].get_logical_qubit() == std::nullopt);
        _qubit_list[assignment[i]].set_logical_qubit(i);
    }
}

/**
 * @brief Calculate Shortest Path
 *
 */
void DeviceState::calculate_path() {
    if (!_apsp) {
        _apsp = std::make_shared<APSPResult>(floyd_warshall(*_device));
    }
}

/**
 * @brief Get shortest path from `s` to `t`
 *
 * @param s start
 * @param t terminate
 * @return vector<PhyQubit>&
 */
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

/**
 * @brief Print physical qubits and their adjacencies
 *
 * @param cand a vector of qubits to be printed
 */
void DeviceState::print_qubits(std::vector<size_t> candidates) const {
    for (auto& c : candidates) {
        if (c >= _num_qubit) {
            spdlog::error("Error: the maximum qubit id is {}!!", _num_qubit - 1);
            return;
        }
    }
    fmt::println("");
    std::vector<PhysicalQubitState> qubits;
    qubits.resize(_num_qubit);
    for (auto const& [idx, info] : tl::views::enumerate(_qubit_list)) {
        qubits[idx] = info;
    }
    if (candidates.empty()) {
        for (size_t i = 0; i < qubits.size(); i++) {
            fmt::println("ID: {:>3}    {}Adjs: {:>3}", i, _device->get_qubit_info(i)[0], fmt::join(_device->get_adjacencies(i), " "));
        }
        fmt::println("Total #Qubits: {}", _num_qubit);
    } else {
        std::ranges::sort(candidates);
        for (auto& p : candidates) {
            fmt::println("ID: {:>3}    {}Adjs: {:>3}", p, _device->get_qubit_info(p)[0], fmt::join(_device->get_adjacencies(p), " "));
        }
    }
}

/**
 * @brief Print device edge
 *
 * @param cand Empty: print all. Single element [a]: print edges connecting to a. Two elements [a,b]: print edge (a,b).
 */
void DeviceState::print_edges(std::vector<size_t> candidates) const {
    for (auto& c : candidates) {
        if (c >= _num_qubit) {
            spdlog::error("the maximum qubit id is {}!!", _num_qubit - 1);
            return;
        }
    }
    fmt::println("");
    std::vector<PhysicalQubitState> qubits;
    qubits.resize(_num_qubit);
    for (auto const& [idx, info] : tl::views::enumerate(_qubit_list)) {
        qubits[idx] = info;
    }
    if (candidates.empty()) {
        size_t cnt = 0;
        for (size_t i = 0; i < _num_qubit; i++) {
            for (auto& q : _device->get_adjacencies(i)) {
                if (std::cmp_less(i, q)) {
                    cnt++;
                    _device->print_single_edge(i, q);
                }
            }
        }
        assert(cnt == _device->get_num_adjacencies());
        fmt::println("Total #Edges: {}", cnt);
    } else if (candidates.size() == 1) {
        for (auto& q : _device->get_adjacencies(candidates[0])) {
            _device->print_single_edge(candidates[0], q);
        }
        fmt::println("Total #Edges: {}", _device->get_adjacencies(candidates[0]).size());
    } else if (candidates.size() == 2) {
        _device->print_single_edge(candidates[0], candidates[1]);
    }
}

/**
 * @brief Print information of Topology
 *
 */
void DeviceState::print_topology() const {
    fmt::println("Topology: {} ({} qubits, {} edges)", get_name(), _qubit_list.size(), _device->get_num_adjacencies());
    auto const tmp = _device->get_gate_set();  // circumvents g++ 11.4 compiler bug
    fmt::println("Gate Set: {}", fmt::join(tmp | std::views::transform([](std::string const& gtype) { return dvlab::str::toupper_string(gtype); }), ", "));
}

/**
 * @brief Print shortest path from `s` to `t`
 *
 * @param s start
 * @param t terminate
 */
void DeviceState::print_path(QubitIdType src, QubitIdType dest) const {
    fmt::println("");
    for (auto& c : {src, dest}) {
        if (std::cmp_greater_equal(c, _num_qubit)) {
            spdlog::error("the maximum qubit id is {}!!", _num_qubit - 1);
            return;
        }
    }
    std::vector<PhysicalQubitState> const& path = get_path(src, dest);
    if (path.front().get_id() != src && path.back().get_id() != dest)
        fmt::println("No path between {} and {}", src, dest);
    else {
        fmt::println("Path from {} to {}:", src, dest);
        size_t cnt = 0;
        for (auto& v : path) {
            constexpr size_t num_cols = 10;
            fmt::print("{:4} ", v.get_id());
            if (++cnt % num_cols == 0) fmt::println("");
        }
    }
}

/**
 * @brief Print Mapping (Physical : Logical)
 *
 */
void DeviceState::print_mapping() {
    fmt::println("----------Mapping---------");
    for (size_t i = 0; i < _num_qubit; i++) {
        fmt::println("{:<5} : {}", i, _qubit_list[i].get_logical_qubit());
    }
}

/**
 * @brief Print device status
 *
 */
void DeviceState::print_status() const {
    fmt::println("Device Status:");
    std::vector<PhysicalQubitState> qubits;
    qubits.resize(_num_qubit);
    for (auto const& [idx, info] : tl::views::enumerate(_qubit_list)) {
        qubits[idx] = info;
    }
    for (size_t i = 0; i < qubits.size(); ++i) {
        fmt::println("{}", qubits[i]);
    }
    fmt::println("");
}

}  // namespace qsyn::device
