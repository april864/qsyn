/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Define hamiltonian test commands ]
  Author       [ Mu-Te Lau (joshmtlau) ]
*/

#pragma once

#include "cli/cli.hpp"
#include "cmd/qbham_mgr.hpp"
#include "cmd/tableau_mgr.hpp"

namespace qsyn::hamiltonian {

dvlab::Command qbham_trotterize_cmd(QubitHamiltonianMgr const& qbham_mgr, tableau::TableauMgr& tableau_mgr);

}  // namespace qsyn::hamiltonian
