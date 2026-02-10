/****************************************************************************
  PackageName  [ qcir/transpile_qiskit ]
  Synopsis     [ Define transpile_qiskit command header ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "cli/cli.hpp"
#include "cmd/qcir_mgr.hpp"

namespace qsyn::qcir {

dvlab::Command qcir_transpile_qiskit_cmd(QCirMgr& qcir_mgr);

}  // namespace qsyn::qcir
