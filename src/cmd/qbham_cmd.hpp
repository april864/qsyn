/****************************************************************************
  PackageName  [ hamiltonian ]
  Synopsis     [ Define hamiltonian commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "cli/cli.hpp"
#include "cmd/device_mgr.hpp"
#include "cmd/qbham_mgr.hpp"
#include "cmd/tableau_mgr.hpp"

namespace qsyn::hamiltonian {

dvlab::Command qbham_cmd(device::DeviceMgr& device_mgr, QubitHamiltonianMgr& qbham_mgr, tableau::TableauMgr& tableau_mgr);

bool add_qbham_cmds(dvlab::CommandLineInterface& cli, device::DeviceMgr& device_mgr, QubitHamiltonianMgr& qbham_mgr, tableau::TableauMgr& tableau_mgr);

}  // namespace qsyn::hamiltonian
