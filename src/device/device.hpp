/****************************************************************************
  PackageName  [ device ]
  Synopsis     [ Define class Device, Topology, and Operation structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <fmt/core.h>

#include <chrono>
#include <cstddef>
#include <string>
#include <tl/expected.hpp>
#include <unordered_map>

#include "qsyn/qsyn_type.hpp"
#include "util/graph/digraph.hpp"
#include "util/util.hpp"

namespace qsyn::device {

/** Gate delay in nanoseconds (float for fractional ns). */
using GateDelayNanoSec = std::chrono::duration<float, std::nano>;

struct GateInfo {
    size_t gate_idx;  // index of this gate in the _gate_set of the device
    GateDelayNanoSec time;
    float error;
};

enum struct TwoQubitGateInfoAccessError : uint8_t {
    invalid_first_qubit_id,
    invalid_second_qubit_id,
    invalid_qubit_pair,
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
    virtual ~Device()         = default;
    using CouplingGraph       = dvlab::Digraph<std::vector<GateInfo>, std::vector<GateInfo>>;
    using QubitPair           = CouplingGraph::Edge;
    using OneQubitGateInfoMap = std::unordered_map<size_t, std::vector<GateInfo>>;
    using TwoQubitGateInfoMap = std::unordered_map<QubitPair, std::vector<GateInfo>, AdjacencyPairHash>;
    using AdjacencyMap        = std::unordered_map<size_t, std::vector<size_t>>;

    std::string get_name() const { return _name; }
    auto get_gate_set() const { return _gate_set; }
    size_t get_num_adjacencies() const { return _graph.num_edges(); }
    size_t get_num_qubits() const { return _graph.num_vertices(); }
    void set_name(std::string n) { _name = std::move(n); }
    void add_gate_type(std::string const& gt) { _gate_set.emplace_back(gt); }
    void add_gate_info(QubitPair const& qubit_id_pair, GateInfo info);
    void add_gate_info(size_t qubit_id, GateInfo info);

    CouplingGraph const& get_coupling_graph() const { return _graph; }

    std::vector<GateInfo> const& get_gate_info(QubitIdType qubit_id) const { return _graph.vertex_attr(qubit_id); }
    std::vector<GateInfo> const& get_gate_info(QubitPair const& qubit_pair) const { return _graph.edge_attr(qubit_pair); }

    CouplingGraph::NeighborSet const& get_adjacencies(size_t qubit_id) const { return _graph.out_neighbors(qubit_id); }
    size_t get_num_adjacencies(QubitIdType qubit_id) const { return _graph.out_degree(qubit_id); }
    bool is_adjacent(QubitPair const& qubit_pair) const { return _graph.has_edge(qubit_pair); }
    bool is_adjacent(QubitIdType src, QubitIdType dst) const { return _graph.has_edge(src, dst); }

    virtual std::string info_string() const;

    std::optional<std::string> gate_info_string(std::size_t qubit_id) const;
    tl::expected<std::string, TwoQubitGateInfoAccessError> gate_info_string(QubitPair const& qubit_pair) const;

protected:
    std::string _name;
    std::vector<std::string> _gate_set;
    // OneQubitGateInfoMap _1q_gate_info;
    // TwoQubitGateInfoMap _2q_gate_info;
    // AdjacencyMap _adjacency_map;
    CouplingGraph _graph;
};

std::optional<Device> read_qsyn_device_file(std::string const& filename);

}  // namespace qsyn::device
