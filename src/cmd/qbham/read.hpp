/**
  PackageName  [ hamiltonian ]
  Synopsis     [ Define command to read hamiltonian from file ]
  Author       [ April Wang (april864) ]
*/

#pragma once

#include "cli/cli.hpp"
#include "cmd/qbham_mgr.hpp"

namespace qsyn::hamiltonian {

dvlab::Command qbham_read_cmd(QubitHamiltonianMgr& qbham_mgr);

}  // namespace qsyn::hamiltonian
