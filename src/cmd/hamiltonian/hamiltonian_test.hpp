/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Define hamiltonian test commands ]
  Author       [ Mu-Te Lau (joshmtlau) ]
*/

#pragma once

#include "cli/cli.hpp"

namespace qsyn::hamiltonian {

bool add_hamiltonian_test_cmds(dvlab::CommandLineInterface& cli);
}  // namespace qsyn::hamiltonian
