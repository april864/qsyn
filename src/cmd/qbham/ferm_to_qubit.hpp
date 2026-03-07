/**
  PackageName  [ hamiltonian ]
  Synopsis     [ Define command to lower fermionic hamiltonian to qubit hamiltonian ]
  Author       [ April Wang (april864) ]
*/

#pragma once

#include "cli/cli.hpp"
#include "cmd/qbham_mgr.hpp"

namespace qsyn::hamiltonian {

dvlab::Command qbham_jw_cmd(QubitHamiltonianMgr& qbham_mgr);

dvlab::Command qbham_ternary_tree_cmd(QubitHamiltonianMgr& qbham_mgr);

}  // namespace qsyn::hamiltonian
