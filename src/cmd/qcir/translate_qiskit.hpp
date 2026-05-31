/****************************************************************************
  PackageName  [ qcir/translate_qiskit ]
  Synopsis     [ Qiskit translate/optimize hook for qcir translate --qiskit ]
  Author       [ Design Verification Lab ]
****************************************************************************/

#pragma once

#include <optional>
#include <string>

#include "cli/cli.hpp"
#include "cmd/device_mgr.hpp"
#include "cmd/qcir_mgr.hpp"

namespace qsyn::qcir {

dvlab::CmdExecResult translate_qiskit(
    QCirMgr& qcir_mgr,
    device::DeviceMgr const& device_mgr,
    std::optional<std::string> backend,
    bool use_real_backend,
    bool no_optimize);

}  // namespace qsyn::qcir
