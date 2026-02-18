/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Implement Jordan-Wigner transformation ]
  Author       [ April Wang (april864) ]
*/

#pragma once

#include "hamiltonian/qubit_hamiltonian.hpp"
#include "hamiltonian/fermionic_hamiltonian.hpp"

namespace qsyn::hamiltonian {

QubitHamiltonian jordan_wigner(FermionHamiltonian const& f_hamilt);

}  // namespace qsyn::hamiltonian