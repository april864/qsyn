/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Fermionic Hamiltonian workspace (Hamiltonian + F2Q encoding) ]
  Author       [ Design Verification Lab ]
*/

#include "hamiltonian/fham_workspace.hpp"

#include "hamiltonian/f2q_mappings.hpp"

namespace qsyn::hamiltonian {

FhamWorkspace::FhamWorkspace(FermionHamiltonian ham)
    : hamiltonian(std::move(ham)) {}

FhamWorkspace::FhamWorkspace(FhamWorkspace const& other)
    : hamiltonian(other.hamiltonian),
      encoding(other.encoding ? other.encoding->clone() : nullptr) {}

FhamWorkspace::FhamWorkspace(FhamWorkspace&&) noexcept = default;

FhamWorkspace::~FhamWorkspace() = default;

FhamWorkspace& FhamWorkspace::operator=(FhamWorkspace copy) {
    swap(*this, copy);
    return *this;
}

FhamWorkspace& FhamWorkspace::operator=(FhamWorkspace&&) noexcept = default;

}  // namespace qsyn::hamiltonian
