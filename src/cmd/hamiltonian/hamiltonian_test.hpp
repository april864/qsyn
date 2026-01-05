/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Define hamiltonian test commands ]
  Author       [ Mu-Te Lau (joshmtlau) ]
*/

#pragma once

#include "cli/cli.hpp"
#include "cmd/hamiltonian_mgr.hpp"

namespace qsyn::hamiltonian {

dvlab::Command hamiltonian_trotterize_cmd(QubitHamiltonianMgr const& hamiltonian_mgr);

}  // namespace qsyn::hamiltonian
