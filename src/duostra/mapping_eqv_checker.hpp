/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class MappingEquivalenceChecker structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <unordered_set>

#include "device/device.hpp"
#include "duostra/duostra.hpp"
#include "qsyn/qsyn_type.hpp"

namespace qsyn {

namespace qcir {

class QCir;
class QCirGate;
struct QubitInfo;

}  // namespace qcir

namespace duostra {

class MappingEquivalenceChecker {
public:
    using DeviceState = qsyn::device::DeviceState;
    MappingEquivalenceChecker(qcir::QCir* phy, qcir::QCir* log, DeviceState dev, PlacerType placer_type, std::vector<QubitIdType> init = {}, bool reverse = false);

    bool check();
    bool is_swap(qcir::QCirGate* candidate);
    bool execute_swap(qcir::QCirGate* first, std::unordered_set<qcir::QCirGate*>& swaps);
    bool execute_single(qcir::QCirGate* gate);
    bool execute_double(qcir::QCirGate* gate);

    void check_remaining();
    qcir::QCirGate* get_next(qcir::QCir const& qcir, size_t gate_id, size_t pin) const;

private:
    qcir::QCir* _physical;
    qcir::QCir* _logical;
    DeviceState _device_state;
    bool _reverse;
    // <qubit, gate to execute (from back)> for logical circuit
    std::unordered_map<QubitIdType, qcir::QCirGate*> _dependency;
};

}  // namespace duostra

}  // namespace qsyn
