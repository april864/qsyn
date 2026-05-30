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
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tl/expected.hpp>
#include <unordered_map>
#include <vector>

#include "qsyn/qsyn_type.hpp"
#include "util/graph/digraph.hpp"
#include "util/util.hpp"

namespace qsyn::device {

/** Gate delay in nanoseconds (float for fractional ns). */
using GateDelayNanoSec = std::chrono::duration<float, std::nano>;

struct GateInfo {
    size_t gate_idx;  // index into Device::_gate_set for the gate name
    GateDelayNanoSec time;
    float error;
};

/** Per-qubit calibrations not tied to a specific gate operation. */
struct QubitProperties {
    std::vector<GateInfo> gate_infos;
    /// Relaxation times in microseconds (IBM ``properties`` convention).
    std::optional<float> t1;
    std::optional<float> t2;
    std::optional<float> readout_error;
};

enum struct TwoQubitGateInfoAccessError : uint8_t {
    invalid_first_qubit_id,
    invalid_second_qubit_id,
    invalid_qubit_pair,
};

enum struct InducedSubdeviceError : uint8_t {
    duplicate_qubit_id,
    unknown_qubit_id,
};

std::ostream& operator<<(std::ostream& os, GateInfo const& info);

struct InducedSubdevice;

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
    virtual ~Device()   = default;
    using CouplingGraph = dvlab::Digraph<QubitProperties, std::vector<GateInfo>>;
    using QubitPair     = CouplingGraph::Edge;

    std::string get_name() const { return _name; }
    auto get_gate_set() const { return _gate_set; }
    size_t get_num_adjacencies() const { return _graph.num_edges(); }
    size_t get_num_qubits() const { return _graph.num_vertices(); }
    void set_name(std::string n) { _name = std::move(n); }
    void add_gate_type(std::string const& gt) { _gate_set.emplace_back(gt); }
    void add_gate_info(QubitPair const& qubit_id_pair, GateInfo info);
    void add_gate_info(size_t qubit_id, GateInfo info);

    void set_qubit_t1(QubitIdType qubit_id, float t1_microseconds);
    void set_qubit_t2(QubitIdType qubit_id, float t2_microseconds);
    void set_readout_error(QubitIdType qubit_id, float readout_error);

    CouplingGraph const& get_coupling_graph() const { return _graph; }

    QubitProperties const& get_qubit_properties(QubitIdType qubit_id) const { return _graph.vertex_attr(qubit_id); }
    std::vector<GateInfo> const& get_gate_info(QubitIdType qubit_id) const { return get_qubit_properties(qubit_id).gate_infos; }
    std::vector<GateInfo> const& get_gate_info(QubitPair const& qubit_pair) const { return _graph.edge_attr(qubit_pair); }

    std::string_view gate_name(size_t gate_idx) const { return _gate_set.at(gate_idx); }
    std::string_view gate_name(GateInfo const& info) const { return gate_name(info.gate_idx); }

    CouplingGraph::NeighborSet const& get_adjacencies(size_t qubit_id) const { return _graph.out_neighbors(qubit_id); }
    size_t get_num_adjacencies(QubitIdType qubit_id) const { return _graph.out_degree(qubit_id); }
    bool is_adjacent(QubitPair const& qubit_pair) const { return _graph.has_edge(qubit_pair); }
    bool is_adjacent(QubitIdType src, QubitIdType dst) const { return _graph.has_edge(src, dst); }

    virtual std::string info_string() const;

    std::optional<std::string> gate_info_string(std::size_t qubit_id) const;
    tl::expected<std::string, TwoQubitGateInfoAccessError> gate_info_string(QubitPair const& qubit_pair) const;

    /**
     * @brief Restrict this device to ``physical_qubits`` and reindex them to 0..n-1.
     *
     * Gate calibrations on kept vertices and edges (both endpoints in the list) are
     * copied. ``physical_qubits`` must not contain duplicates; every id must exist on
     * this device.
     */
    [[nodiscard]] tl::expected<InducedSubdevice, InducedSubdeviceError> induced_subdevice(
        std::span<QubitIdType const> physical_qubits) const;

protected:
    void _ensure_qubit(QubitIdType qubit_id);

    std::string _name;
    std::vector<std::string> _gate_set;
    CouplingGraph _graph;
};

struct InducedSubdevice {
    Device device;
    /// ``physical_qubits[logical]`` is the parent device's physical qubit index.
    std::vector<QubitIdType> physical_qubits;
};

std::optional<Device> read_qsyn_device_file(std::string const& filename);

}  // namespace qsyn::device
