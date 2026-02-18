/**
  PackageName  [ hamiltonian ]
  Synopsis     [ Define command to lower fermionic hamiltonian to qubit hamiltonian ]
  Author       [ April Wang (april864) ]
*/

#pragma once

#include "cli/cli.hpp"
#include "cmd/qbham_mgr.hpp"

namespace qsyn::hamiltonian {

dvlab::Command qbham_ferm_to_qubit_cmd(QubitHamiltonianMgr& qbham_mgr);

} // namespace qsyn::hamiltonian