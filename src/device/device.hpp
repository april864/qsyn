/****************************************************************************
  PackageName  [ device ]
  Synopsis     [ Define class Device, Topology, and Operation structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <fmt/core.h>

#include <cstddef>
#include <string>
#include <unordered_map>

#include "qsyn/qsyn_type.hpp"
#include "util/util.hpp"

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
    size_t get_num_qubits() const { return _adjacency_map.size(); }
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

APSPResult floyd_warshall(Device const& device);

}  // namespace qsyn::device

template <>
struct fmt::formatter<qsyn::device::GateInfo> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(qsyn::device::GateInfo const& info, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "Delay: {:>7.3}    Error: {:7.3}    ", info.time, info.error);
    }
};
