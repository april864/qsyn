/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Fermionic Hamiltonian workspace (Hamiltonian + F2Q encoding) ]
  Author       [ Design Verification Lab ]
*/

#pragma once

#include <memory>
#include <utility>

#include "hamiltonian/fermionic_hamiltonian.hpp"

namespace qsyn::hamiltonian {

class FermionToQubitMapping;

struct FhamWorkspace {
    FermionHamiltonian hamiltonian;
    std::unique_ptr<FermionToQubitMapping> encoding;

    explicit FhamWorkspace(FermionHamiltonian ham);

    FhamWorkspace(FhamWorkspace const& other);
    FhamWorkspace(FhamWorkspace&&) noexcept;
    ~FhamWorkspace();

    FhamWorkspace& operator=(FhamWorkspace copy);
    FhamWorkspace& operator=(FhamWorkspace&&) noexcept;

    friend void swap(FhamWorkspace& lhs, FhamWorkspace& rhs) noexcept {
        using std::swap;
        swap(lhs.hamiltonian, rhs.hamiltonian);
        swap(lhs.encoding, rhs.encoding);
    }
};

}  // namespace qsyn::hamiltonian
