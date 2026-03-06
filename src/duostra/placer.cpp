/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Placer member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./placer.hpp"

#include <cassert>
#include <random>

#include "./duostra.hpp"
#include "device/device.hpp"
#include "duostra/duostra_def.hpp"
#include "qsyn/qsyn_type.hpp"
#include "util/util.hpp"

namespace qsyn::duostra {

/**
 * @brief Get the Placer object
 *
 * @return unique_ptr<BasePlacer>
 */
std::unique_ptr<BasePlacer> get_placer(PlacerType type) {
    switch (type) {
        case PlacerType::naive:
            return std::make_unique<StaticPlacer>();
        case PlacerType::random:
            return std::make_unique<RandomPlacer>();
        case PlacerType::dfs:
            return std::make_unique<DFSPlacer>();
    }
    return nullptr;
}

// SECTION - Class BasePlacer Member Functions

/**
 * @brief Place and assign logical qubit
 *
 * @param device
 */
std::vector<QubitIdType> BasePlacer::place_and_assign(DeviceState& device) {
    auto assign = _place(device);
    device.place(assign);
    return assign;
}

// SECTION - Class RandomPlacer Member Functions

/**
 * @brief Place logical qubit
 *
 * @param device
 * @return vector<size_t>
 */
std::vector<QubitIdType> RandomPlacer::_place(DeviceState& device) const {
    std::vector<QubitIdType> assign;
    for (size_t i = 0; i < device.get_num_qubits(); ++i)
        assign.emplace_back(i);

    std::ranges::shuffle(assign, std::default_random_engine(std::random_device{}()));
    return assign;
}

// SECTION - Class StaticPlacer Member Functions

/**
 * @brief Place logical qubit
 *
 * @param device
 * @return vector<size_t>
 */
std::vector<QubitIdType> StaticPlacer::_place(DeviceState& device) const {
    std::vector<QubitIdType> assign;
    for (size_t i = 0; i < device.get_num_qubits(); ++i)
        assign.emplace_back(i);

    return assign;
}

// SECTION - Class DFSPlacer Member Functions

/**
 * @brief Place logical qubit
 *
 * @param device
 * @return vector<size_t>
 */
std::vector<QubitIdType> DFSPlacer::_place(DeviceState& device) const {
    std::vector<QubitIdType> assign;
    std::vector<bool> qubit_mark(device.get_num_qubits(), false);
    _dfs_device(0, device, assign, qubit_mark);
    assert(assign.size() == device.get_num_qubits());
    return assign;
}

/**
 * @brief Depth-first search the device
 *
 * @param current
 * @param device
 * @param assign
 * @param qubitMark
 */
void DFSPlacer::_dfs_device(QubitIdType current, DeviceState& device_state, std::vector<QubitIdType>& assign, std::vector<bool>& qubit_marks) const {
    if (qubit_marks[current]) {
        fmt::println("{}", current);
    }
    assert(!qubit_marks[current]);
    qubit_marks[current] = true;
    assign.emplace_back(current);

    std::vector<QubitIdType> adjacency_waitlist;

    for (auto& adj : device_state.get_device().get_adjacencies(current)) {
        // already marked
        if (qubit_marks[adj])
            continue;
        assert(device_state.get_device().get_num_adjacencies(current) > 0);
        // corner
        if (device_state.get_device().get_num_adjacencies(current) == 1)
            _dfs_device(adj, device_state, assign, qubit_marks);
        else
            adjacency_waitlist.emplace_back(adj);
    }

    for (size_t i = 0; i < adjacency_waitlist.size(); ++i) {
        auto adj = adjacency_waitlist[i];
        if (qubit_marks[adj])
            continue;
        _dfs_device(adj, device_state, assign, qubit_marks);
    }
}

}  // namespace qsyn::duostra
