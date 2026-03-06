/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Duostra structure ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <memory>

#include "./duostra_def.hpp"
#include "./scheduler.hpp"
#include "qcir/qcir.hpp"

namespace qsyn {

namespace qcir {

class QCir;

}

namespace duostra {

class Duostra {
public:
    using DeviceState = qsyn::duostra::DeviceState;
    struct DuostraExecutionOptions {
        bool verify_result = false;
        bool silent        = false;
        bool use_tqdm      = true;
    };
    Duostra(
        qcir::QCir* qcir,
        DeviceState dev,
        DuostraConfig const& config,
        DuostraExecutionOptions const& exe_opts = {.verify_result = false, .silent = false, .use_tqdm = true});

    std::unique_ptr<qcir::QCir> const& get_physical_circuit() const { return _physical_circuit; }
    std::unique_ptr<qcir::QCir>&& get_physical_circuit() { return std::move(_physical_circuit); }
    std::vector<qcir::QCirGate> const& get_result() const { return _result; }
    DeviceState get_device_state() const { return _device_state; }

    void make_dependency();
    bool map(bool use_device_as_placement = false);
    void build_circuit_by_result();

private:
    std::unique_ptr<qcir::QCir> _physical_circuit =
        std::make_unique<qcir::QCir>();
    DeviceState _device_state;
    DuostraConfig _config;
    bool _check;
    bool _tqdm;
    bool _silent;
    std::unique_ptr<BaseScheduler> _scheduler;
    std::shared_ptr<qcir::QCir> _logical_circuit;
    std::vector<qcir::QCirGate> _result;
};

}  // namespace duostra

}  // namespace qsyn
