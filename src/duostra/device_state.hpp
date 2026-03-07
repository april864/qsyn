/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define PhysicalQubitState and DeviceState for Duostra routing ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <fmt/core.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <ostream>
#include <vector>

#include "device/device.hpp"
#include "qsyn/qsyn_type.hpp"

namespace qsyn::qcir {
class QCirGate;
}

namespace qsyn::duostra {

class PhysicalQubitState {
public:
    using Adjacencies = std::vector<QubitIdType>;
    PhysicalQubitState() {}
    PhysicalQubitState(QubitIdType id) : _id(id) {}

    void set_id(QubitIdType id) { _id = id; }
    void set_occupied_time(size_t t) { _occupied_time = t; }
    void set_logical_qubit(std::optional<size_t> id) { _logical_qubit = id; }

    auto get_id() const { return _id; }
    auto get_occupied_time() const { return _occupied_time; }
    auto get_logical_qubit() const { return _logical_qubit; }

    // traversal
    auto get_cost() const { return _cost; }
    auto is_marked() const { return _marked; }
    auto is_taken() const { return _taken; }
    auto get_source() const { return _source; }
    auto get_predecessor() const { return _pred; }
    auto get_swap_time() const { return _swap_time; }

    void mark(bool source, QubitIdType pred);
    void take_route(size_t cost, size_t swap_time);
    void reset();

private:
    QubitIdType _id = max_qubit_id;

    std::optional<QubitIdType> _logical_qubit = std::nullopt;
    size_t _occupied_time                     = 0;

    bool _marked      = false;
    QubitIdType _pred = 0;
    size_t _cost      = 0;
    size_t _swap_time = 0;
    bool _source      = false;  // false:0, true:1
    bool _taken       = false;
};

std::ostream& operator<<(std::ostream& os, PhysicalQubitState const& q);

class DeviceState {
public:
    using PhysicalQubitList = std::vector<PhysicalQubitState>;
    DeviceState(device::Device device);
    std::string get_name() const { return _device->get_name(); }
    size_t get_num_qubits() const { return _device->get_num_qubits(); }
    PhysicalQubitList const& get_physical_qubit_list() const { return _qubit_list; }
    PhysicalQubitState& get_physical_qubit(QubitIdType id) { return _qubit_list[id]; }
    QubitIdType get_physical_by_logical(QubitIdType id);
    std::tuple<QubitIdType, QubitIdType> get_next_swap_cost(QubitIdType source, QubitIdType target);

    void apply_gate(qcir::QCirGate const& op, size_t time_begin);
    std::vector<std::optional<size_t>> mapping() const;
    void place(std::vector<QubitIdType> const& assignment);

    void calculate_path();
    std::vector<PhysicalQubitState> get_path(QubitIdType src, QubitIdType dest) const;

    size_t get_delay(qcir::QCirGate const& inst) const;

    device::Device const& get_device() const { return *_device; }

private:
    std::shared_ptr<device::Device> _device;
    PhysicalQubitList _qubit_list;
    std::shared_ptr<device::APSPResult<std::size_t>> _apsp;
};

}  // namespace qsyn::duostra

template <>
struct fmt::formatter<qsyn::duostra::PhysicalQubitState> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(qsyn::duostra::PhysicalQubitState const& q, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "Q{:>2}, logical: {:>2}, lock until {}", q.get_id(), q.get_logical_qubit(), q.get_occupied_time());
    }
};
