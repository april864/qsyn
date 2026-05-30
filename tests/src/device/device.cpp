/*
  Unit tests for device::Device (physical device topology and gate info).
*/

#include "device/device.hpp"

#include <array>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace qsyn::device;
using qsyn::QubitIdType;
using GateDelayNanoSec = qsyn::device::GateDelayNanoSec;

static Device make_line_3() {
    // 3 qubits in a line: 0 -- 1 -- 2
    Device d;
    d.set_name("line-3");
    d.add_gate_type("h");
    d.add_gate_type("cx");
    d.add_gate_info(Device::QubitPair{0, 1}, GateInfo{.gate_idx = 1, .time = GateDelayNanoSec{50.f}, .error = 0.01f});
    d.add_gate_info(Device::QubitPair{1, 0}, GateInfo{.gate_idx = 1, .time = GateDelayNanoSec{50.f}, .error = 0.01f});
    d.add_gate_info(Device::QubitPair{1, 2}, GateInfo{.gate_idx = 1, .time = GateDelayNanoSec{50.f}, .error = 0.01f});
    d.add_gate_info(Device::QubitPair{2, 1}, GateInfo{.gate_idx = 1, .time = GateDelayNanoSec{50.f}, .error = 0.01f});
    for (size_t i = 0; i < 3; ++i) {
        d.add_gate_info(i, GateInfo{.gate_idx = 0, .time = GateDelayNanoSec{20.f}, .error = 0.001f});
    }
    return d;
}

TEST_CASE("Device default construction", "[device]") {
    Device d;
    REQUIRE(d.get_name().empty());
    REQUIRE(d.get_gate_set().empty());
    REQUIRE(d.get_num_qubits() == 0);
    REQUIRE(d.get_num_adjacencies() == 0);
}

TEST_CASE("Device set_name and get_name", "[device]") {
    Device d;
    d.set_name("ibmq_lima");
    REQUIRE(d.get_name() == "ibmq_lima");
}

TEST_CASE("Device add_gate_type and get_gate_set", "[device]") {
    Device d;
    d.add_gate_type("h");
    d.add_gate_type("cx");
    auto gate_set = d.get_gate_set();
    REQUIRE(gate_set.size() == 2);
    REQUIRE(gate_set[0] == "h");
    REQUIRE(gate_set[1] == "cx");
}

TEST_CASE("Device add_gate_info single qubit creates vertex and stores info", "[device]") {
    Device d;
    d.add_gate_type("h");
    d.add_gate_info(0, GateInfo{.gate_idx = 0, .time = GateDelayNanoSec{10.f}, .error = 0.001f});
    REQUIRE(d.get_num_qubits() == 1);
    auto const& info = d.get_gate_info(0);
    REQUIRE(info.size() == 1);
    REQUIRE(info[0].gate_idx == 0);
    REQUIRE(info[0].time.count() == 10.f);
    REQUIRE(info[0].error == 0.001f);
}

TEST_CASE("Device add_gate_info single qubit appends multiple infos", "[device]") {
    Device d;
    d.add_gate_type("h");
    d.add_gate_type("x");
    d.add_gate_info(0, GateInfo{.gate_idx = 0, .time = GateDelayNanoSec{10.f}, .error = 0.001f});
    d.add_gate_info(0, GateInfo{.gate_idx = 1, .time = GateDelayNanoSec{20.f}, .error = 0.002f});
    auto const& info = d.get_gate_info(0);
    REQUIRE(info.size() == 2);
    REQUIRE(info[0].gate_idx == 0);
    REQUIRE(info[1].gate_idx == 1);
}

TEST_CASE("Device add_gate_info qubit pair creates edge and stores info", "[device]") {
    Device d;
    d.add_gate_type("cx");
    d.add_gate_info(Device::QubitPair{0, 1}, GateInfo{.gate_idx = 0, .time = GateDelayNanoSec{50.f}, .error = 0.01f});
    REQUIRE(d.get_num_qubits() == 2);
    REQUIRE(d.get_num_adjacencies() == 1);
    REQUIRE(d.is_adjacent(Device::QubitPair{0, 1}));
    REQUIRE(d.is_adjacent(0, 1));
    auto const& info = d.get_gate_info(Device::QubitPair{0, 1});
    REQUIRE(info.size() == 1);
    REQUIRE(info[0].gate_idx == 0);
    REQUIRE(info[0].time.count() == 50.f);
    REQUIRE(info[0].error == 0.01f);
}

TEST_CASE("Device add_gate_info qubit pair appends multiple infos", "[device]") {
    Device d;
    d.add_gate_type("cx");
    d.add_gate_type("cz");
    d.add_gate_info(Device::QubitPair{0, 1}, GateInfo{.gate_idx = 0, .time = GateDelayNanoSec{50.f}, .error = 0.01f});
    d.add_gate_info(Device::QubitPair{0, 1}, GateInfo{.gate_idx = 1, .time = GateDelayNanoSec{30.f}, .error = 0.005f});
    auto const& info = d.get_gate_info(Device::QubitPair{0, 1});
    REQUIRE(info.size() == 2);
    REQUIRE(info[0].gate_idx == 0);
    REQUIRE(info[1].gate_idx == 1);
}

TEST_CASE("Device get_adjacencies and get_num_adjacencies per qubit", "[device]") {
    Device d = make_line_3();
    REQUIRE(d.get_num_qubits() == 3);
    REQUIRE(d.get_num_adjacencies() == 4);  // (0,1), (1,0), (1,2), (2,1)
    auto const& adj0 = d.get_adjacencies(0);
    REQUIRE(adj0.size() == 1);
    REQUIRE(adj0.contains(1u));
    auto const& adj1 = d.get_adjacencies(1);
    REQUIRE(adj1.size() == 2);
    REQUIRE(adj1.contains(0u));
    REQUIRE(adj1.contains(2u));
    auto const& adj2 = d.get_adjacencies(2);
    REQUIRE(adj2.size() == 1);
    REQUIRE(adj2.contains(1u));
    REQUIRE(d.get_num_adjacencies(0) == 1);
    REQUIRE(d.get_num_adjacencies(1) == 2);
    REQUIRE(d.get_num_adjacencies(2) == 1);
}

TEST_CASE("Device is_adjacent two-arg form", "[device]") {
    Device d = make_line_3();
    REQUIRE(d.is_adjacent(0, 1));
    REQUIRE(d.is_adjacent(1, 0));
    REQUIRE(d.is_adjacent(1, 2));
    REQUIRE(d.is_adjacent(2, 1));
    REQUIRE(!d.is_adjacent(0, 2));
    REQUIRE(!d.is_adjacent(2, 0));
}

TEST_CASE("Device info_string", "[device]") {
    Device d         = make_line_3();
    std::string info = d.info_string();
    REQUIRE(info.find("line-3") != std::string::npos);
    REQUIRE(info.find("3 qubits") != std::string::npos);
    REQUIRE(info.find("h") != std::string::npos);
    REQUIRE(info.find("cx") != std::string::npos);
}

TEST_CASE("Device gate_info_string single qubit", "[device]") {
    Device d = make_line_3();
    auto s0  = d.gate_info_string(0);
    REQUIRE(s0.has_value());
    REQUIRE(s0->find("Qubit 0") != std::string::npos);
    REQUIRE(s0->find("adjacencies") != std::string::npos);
    REQUIRE(s0->find("h") != std::string::npos);
    auto s99 = d.gate_info_string(99);
    REQUIRE(!s99.has_value());
}

TEST_CASE("Device gate_info_string qubit pair", "[device]") {
    Device d = make_line_3();
    auto s01 = d.gate_info_string(Device::QubitPair{0, 1});
    REQUIRE(s01.has_value());
    REQUIRE(s01->find("Adjacency (0, 1)") != std::string::npos);
    REQUIRE(s01->find("cx") != std::string::npos);
    auto invalid_first = d.gate_info_string(Device::QubitPair{99, 0});
    REQUIRE(!invalid_first.has_value());
    REQUIRE(invalid_first.error() == TwoQubitGateInfoAccessError::invalid_first_qubit_id);
    auto invalid_second = d.gate_info_string(Device::QubitPair{0, 99});
    REQUIRE(!invalid_second.has_value());
    REQUIRE(invalid_second.error() == TwoQubitGateInfoAccessError::invalid_second_qubit_id);
    auto invalid_pair = d.gate_info_string(Device::QubitPair{0, 2});
    REQUIRE(!invalid_pair.has_value());
    REQUIRE(invalid_pair.error() == TwoQubitGateInfoAccessError::invalid_qubit_pair);
}

TEST_CASE("Device get_coupling_graph returns consistent reference", "[device]") {
    Device d       = make_line_3();
    auto const& g1 = d.get_coupling_graph();
    auto const& g2 = d.get_coupling_graph();
    REQUIRE(&g1 == &g2);
    REQUIRE(g1.num_vertices() == 3);
    REQUIRE(g1.num_edges() == 4);  // (0,1), (1,0), (1,2), (2,1)
}

TEST_CASE("Device read_qsyn_device_file missing file returns nullopt", "[device]") {
    auto dev = read_qsyn_device_file("/nonexistent/path/device.device");
    REQUIRE(!dev.has_value());
}

TEST_CASE("Device induced_subdevice path and single-qubit calibrations", "[device]") {
    Device d = make_line_3();
    auto sub = d.induced_subdevice(std::array<QubitIdType, 2>{0, 1});
    REQUIRE(sub.has_value());
    REQUIRE(sub->physical_qubits == std::vector<QubitIdType>{0, 1});
    REQUIRE(sub->device.get_name() == "line-3_sub");
    REQUIRE(sub->device.get_num_qubits() == 2);
    REQUIRE(sub->device.get_gate_set() == d.get_gate_set());
    REQUIRE(sub->device.is_adjacent(0, 1));
    REQUIRE(sub->device.is_adjacent(1, 0));
    REQUIRE(!sub->device.is_adjacent(0, 2));
    auto const& edge01 = sub->device.get_gate_info(Device::QubitPair{0, 1});
    REQUIRE(edge01.size() == 1);
    REQUIRE(edge01[0].error == 0.01f);
    REQUIRE(sub->device.get_gate_info(0).size() == 1);
    REQUIRE(sub->device.get_gate_info(1).size() == 1);
}

TEST_CASE("Device induced_subdevice non-adjacent qubits have no coupling edge", "[device]") {
    Device d = make_line_3();
    auto sub = d.induced_subdevice(std::array<QubitIdType, 2>{0, 2});
    REQUIRE(sub.has_value());
    REQUIRE(sub->device.get_num_qubits() == 2);
    REQUIRE(sub->device.get_num_adjacencies() == 0);
    REQUIRE(sub->device.get_gate_info(0).size() == 1);
    REQUIRE(sub->device.get_gate_info(1).size() == 1);
}

TEST_CASE("Device induced_subdevice preserves input order", "[device]") {
    Device d = make_line_3();
    auto sub = d.induced_subdevice(std::array<QubitIdType, 2>{2, 0});
    REQUIRE(sub.has_value());
    REQUIRE(sub->physical_qubits == std::vector<QubitIdType>{2, 0});
    REQUIRE(sub->device.get_gate_info(0)[0].error == d.get_gate_info(2)[0].error);
    REQUIRE(sub->device.get_gate_info(1)[0].error == d.get_gate_info(0)[0].error);
}

TEST_CASE("Device induced_subdevice empty selection", "[device]") {
    Device d = make_line_3();
    auto sub = d.induced_subdevice(std::span<QubitIdType const>{});
    REQUIRE(sub.has_value());
    REQUIRE(sub->physical_qubits.empty());
    REQUIRE(sub->device.get_num_qubits() == 0);
}

TEST_CASE("Device induced_subdevice duplicate qubit id", "[device]") {
    Device d = make_line_3();
    auto sub = d.induced_subdevice(std::array<QubitIdType, 2>{1, 1});
    REQUIRE(!sub.has_value());
    REQUIRE(sub.error() == InducedSubdeviceError::duplicate_qubit_id);
}

TEST_CASE("Device induced_subdevice unknown qubit id", "[device]") {
    Device d = make_line_3();
    auto sub = d.induced_subdevice(std::array<QubitIdType, 1>{99});
    REQUIRE(!sub.has_value());
    REQUIRE(sub.error() == InducedSubdeviceError::unknown_qubit_id);
}

TEST_CASE("Device qubit T1 T2 readout_error and gate_name", "[device]") {
    Device d;
    d.add_gate_type("sx");
    d.set_qubit_t1(0, 100.f);
    d.set_qubit_t2(0, 50.f);
    d.set_readout_error(0, 0.02f);
    d.add_gate_info(0, GateInfo{.gate_idx = 0, .time = GateDelayNanoSec{32.f}, .error = 0.001f});

    auto const& props = d.get_qubit_properties(0);
    REQUIRE(props.t1 == 100.f);
    REQUIRE(props.t2 == 50.f);
    REQUIRE(props.readout_error == 0.02f);
    REQUIRE(props.gate_infos.size() == 1);
    REQUIRE(d.gate_name(props.gate_infos[0]) == "sx");
    REQUIRE(d.gate_name(0) == "sx");

    auto s = d.gate_info_string(0);
    REQUIRE(s.has_value());
    REQUIRE(s->find("T1") != std::string::npos);
    REQUIRE(s->find("readout_error") != std::string::npos);
    REQUIRE(s->find("sx") != std::string::npos);
}

TEST_CASE("Device induced_subdevice copies qubit properties", "[device]") {
    Device d = make_line_3();
    d.set_qubit_t1(0, 10.f);
    d.set_qubit_t2(1, 20.f);
    d.set_readout_error(2, 0.03f);

    auto sub = d.induced_subdevice(std::array<QubitIdType, 2>{0, 2});
    REQUIRE(sub.has_value());
    REQUIRE(sub->device.get_qubit_properties(0).t1 == 10.f);
    REQUIRE(!sub->device.get_qubit_properties(0).t2.has_value());
    REQUIRE(!sub->device.get_qubit_properties(1).t1.has_value());
    REQUIRE(sub->device.get_qubit_properties(1).readout_error == 0.03f);
}
