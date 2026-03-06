/****************************************************************************
  PackageName  [ device ]
  Synopsis     [ Define class Device, Topology, and Operation structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <fmt/core.h>

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>

#include "qcir/qcir_gate.hpp"
#include "qsyn/qsyn_type.hpp"
#include "util/util.hpp"

namespace qsyn::qcir {
class QCirGate;
}

namespace qsyn::device {

struct GateInfo {
    size_t gate_idx;  // index of this gate in the _gate_set of the device
    float time;
    float error;
};

std::ostream& operator<<(std::ostream& os, GateInfo const& info);

class Device {
    struct AdjacencyPairHash {
        size_t operator()(std::pair<size_t, size_t> const& k) const {
            return (
                (std::hash<size_t>()(k.first) ^
                 (std::hash<size_t>()(k.second) << 1)) >>
                1);
        }
    };

public:
    using QubitPair           = std::pair<size_t, size_t>;
    using OneQubitGateInfoMap = std::unordered_map<size_t, std::vector<GateInfo>>;
    using TwoQubitGateInfoMap = std::unordered_map<QubitPair, std::vector<GateInfo>, AdjacencyPairHash>;
    using AdjacencyMap        = std::unordered_map<size_t, std::vector<size_t>>;

    std::string get_name() const { return _name; }
    auto get_gate_set() const { return _gate_set; }
    std::vector<GateInfo> const& get_adjacency_pair_info(size_t a, size_t b);
    std::vector<GateInfo> const& get_qubit_info(size_t a);
    size_t get_num_adjacencies() const { return _2q_gate_info.size(); }
    size_t get_num_qubits() const { return _num_qubit; }
    void set_num_qubits(size_t n) { _num_qubit = n; }
    void set_name(std::string n) { _name = std::move(n); }
    void add_gate_type(std::string const& gt) { _gate_set.emplace_back(gt); }
    void add_gate_info(std::pair<size_t, size_t> const& qubit_id_pair, GateInfo info);
    void add_gate_info(size_t qubit_id, GateInfo info);

    void print_single_edge(size_t a, size_t b) const;
    TwoQubitGateInfoMap const& get_2q_gate_info_map() const { return _2q_gate_info; }

    AdjacencyMap const& get_adjacency_map() const { return _adjacency_map; }
    std::vector<size_t> const& get_adjacencies(size_t qubit_id) const { return _adjacency_map.at(qubit_id); }
    size_t get_num_adjacencies(size_t qubit_id) const { return _adjacency_map.at(qubit_id).size(); }
    bool is_adjacency(size_t a, size_t b) const { return dvlab::contains(_adjacency_map.at(a), b); }

private:
    std::string _name;
    size_t _num_qubit{0};
    std::vector<std::string> _gate_set;
    OneQubitGateInfoMap _1q_gate_info;
    TwoQubitGateInfoMap _2q_gate_info;
    AdjacencyMap _adjacency_map;
};

struct APSPResult {
    std::vector<std::vector<std::optional<QubitIdType>>> predecessor;
    std::vector<std::vector<std::optional<size_t>>> distance;
};

std::optional<Device> read_qsyn_device_file(std::string const& filename);

APSPResult floyd_warshall(const Device& device);

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

    // NOTE - Duostra functions
    void mark(bool source, QubitIdType pred);
    void take_route(size_t cost, size_t swap_time);
    void reset();

private:
    // NOTE - Device information
    QubitIdType _id = max_qubit_id;

    // NOTE - Duostra parameter
    std::optional<QubitIdType> _logical_qubit = std::nullopt;
    size_t _occupied_time                     = 0;

    bool _marked      = false;
    QubitIdType _pred = 0;
    size_t _cost      = 0;
    size_t _swap_time = 0;
    bool _source      = false;  // false:0, true:1
    bool _taken       = false;
};

class DeviceState {
public:
    using PhysicalQubitList = std::vector<PhysicalQubitState>;
    DeviceState() : _device{std::make_shared<Device>()} {}

    std::string get_name() const { return _device->get_name(); }
    size_t get_num_qubits() const { return _num_qubit; }
    PhysicalQubitList const& get_physical_qubit_list() const { return _qubit_list; }
    PhysicalQubitState& get_physical_qubit(QubitIdType id) { return _qubit_list[id]; }
    QubitIdType get_physical_by_logical(QubitIdType id);
    std::tuple<QubitIdType, QubitIdType> get_next_swap_cost(QubitIdType source, QubitIdType target);

    // NOTE - Duostra
    void apply_gate(qcir::QCirGate const& op, size_t time_begin);
    std::vector<std::optional<size_t>> mapping() const;
    void place(std::vector<QubitIdType> const& assignment);

    // NOTE - All Pairs Shortest Path
    void calculate_path();
    std::vector<PhysicalQubitState> get_path(QubitIdType src, QubitIdType dest) const;

    bool read_device(std::string const& filename);

    void print_qubits(std::vector<size_t> candidates = {}) const;
    void print_edges(std::vector<size_t> candidates = {}) const;
    void print_topology() const;
    void print_predecessor() const;
    void print_distance() const;
    void print_path(QubitIdType src, QubitIdType dest) const;
    void print_mapping();
    void print_status() const;

    size_t get_delay(qcir::QCirGate const& inst) const;

    Device const& get_device() const { return *_device; }

private:
    size_t _num_qubit = 0;
    std::shared_ptr<Device> _device;
    PhysicalQubitList _qubit_list;
    std::shared_ptr<APSPResult> _apsp;
};

}  // namespace qsyn::device

template <>
struct fmt::formatter<qsyn::device::GateInfo> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(qsyn::device::GateInfo const& info, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "Delay: {:>7.3}    Error: {:7.3}    ", info.time, info.error);
    }
};
