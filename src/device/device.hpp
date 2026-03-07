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
    using QubitPair           = std::pair<size_t, size_t>;
    using OneQubitGateInfoMap = std::unordered_map<size_t, std::vector<GateInfo>>;
    using TwoQubitGateInfoMap = std::unordered_map<QubitPair, std::vector<GateInfo>, AdjacencyPairHash>;
    using AdjacencyMap        = std::unordered_map<size_t, std::vector<size_t>>;

    std::string get_name() const { return _name; }
    auto get_gate_set() const { return _gate_set; }
    size_t get_num_adjacencies() const { return _2q_gate_info.size(); }
    size_t get_num_qubits() const { return _adjacency_map.size(); }
    void set_name(std::string n) { _name = std::move(n); }
    void add_gate_type(std::string const& gt) { _gate_set.emplace_back(gt); }
    void add_gate_info(QubitPair const& qubit_id_pair, GateInfo info);
    void add_gate_info(size_t qubit_id, GateInfo info);

    void print_single_edge(size_t a, size_t b) const;
    TwoQubitGateInfoMap const& get_2q_gate_info_map() const { return _2q_gate_info; }

    std::vector<size_t> const& get_adjacencies(size_t qubit_id) const { return _adjacency_map.at(qubit_id); }
    size_t get_num_adjacencies(size_t qubit_id) const { return _adjacency_map.at(qubit_id).size(); }
    bool is_adjacency(size_t a, size_t b) const { return dvlab::contains(_adjacency_map.at(a), b); }

    virtual std::string info_string() const;

    std::optional<std::string> gate_info_string(std::size_t qubit_id) const;
    tl::expected<std::string, TwoQubitGateInfoAccessError> gate_info_string(QubitPair const& qubit_pair) const;

protected:
    std::string _name;
    std::vector<std::string> _gate_set;
    OneQubitGateInfoMap _1q_gate_info;
    TwoQubitGateInfoMap _2q_gate_info;
    AdjacencyMap _adjacency_map;
};

template <typename CostType>
struct APSPResult {
    std::vector<std::vector<std::optional<QubitIdType>>> predecessor;
    std::vector<std::vector<std::optional<CostType>>> distance;
};

std::optional<Device> read_qsyn_device_file(std::string const& filename);

/**
 * @brief Floyd-Warshall Algorithm. Solve All Pairs Shortest Path (APSP)
 *
 * @param device Physical qubit adjacency information
 */
template <typename CostType>
APSPResult<CostType>
floyd_warshall(
    Device const& device,
    std::function<CostType(Device::QubitPair const&, Device const&)> const& cost_fn =
        [](Device::QubitPair const&, Device const&) -> CostType {
        return 1;
    }) {
    auto const n = device.get_num_qubits();

    APSPResult<CostType> result;
    result.distance.assign(n, std::vector<std::optional<size_t>>(n, std::nullopt));
    result.predecessor.assign(n, std::vector<std::optional<QubitIdType>>(n, std::nullopt));

    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            if (i == j) {
                result.distance[i][j]    = 0;
                result.predecessor[i][j] = std::nullopt;
            }
        }
    }

    for (auto const& [adj, _] : device.get_2q_gate_info_map()) {
        auto const& [i, j]       = adj;
        result.distance[i][j]    = cost_fn(adj, device);
        result.distance[j][i]    = cost_fn(adj, device);
        result.predecessor[i][j] = i;
        result.predecessor[j][i] = j;
    }

    for (size_t k = 0; k < n; k++) {
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
    }

    return result;
}

}  // namespace qsyn::device

template <>
struct fmt::formatter<qsyn::device::GateInfo> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(qsyn::device::GateInfo const& info, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "Delay: {:>7.3}    Error: {:7.3}    ", info.time.count(), info.error);
    }
};
