/****************************************************************************
  PackageName  [ hamiltonian ]
  Synopsis     [ Define fermionic Hamiltonian commands ]
  Author       [ Design Verification Lab ]
****************************************************************************/

#pragma once

#include "cli/cli.hpp"
#include "cmd/device_mgr.hpp"
#include "cmd/fham_mgr.hpp"
#include "cmd/qbham_mgr.hpp"
#include "cmd/qcir_mgr.hpp"

namespace qsyn::hamiltonian {

dvlab::Command fham_cmd(
    FermionHamiltonianMgr& fham_mgr,
    QubitHamiltonianMgr& qbham_mgr,
    qcir::QCirMgr& qcir_mgr,
    device::DeviceMgr& device_mgr);

bool add_fham_cmds(
    dvlab::CommandLineInterface& cli,
    FermionHamiltonianMgr& fham_mgr,
    QubitHamiltonianMgr& qbham_mgr,
    qcir::QCirMgr& qcir_mgr,
    device::DeviceMgr& device_mgr);

}  // namespace qsyn::hamiltonian
