/****************************************************************************
  PackageName  [ hamiltonian ]
  Synopsis     [ Define hamiltonian commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "cli/cli.hpp"
#include "cmd/hamiltonian_mgr.hpp"

namespace qsyn::hamiltonian {

dvlab::Command hamiltonian_cmd(QubitHamiltonianMgr& hamiltonian_mgr);

bool add_hamiltonian_cmds(dvlab::CommandLineInterface& cli, QubitHamiltonianMgr& hamiltonian_mgr);

}  // namespace qsyn::hamiltonian
