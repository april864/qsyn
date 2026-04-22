/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Fermion-to-qubit mappings (reusable F2Q logic) ]
  Author       [ April Wang (april864) ]
*/

#pragma once

#include "hamiltonian/fermionic_hamiltonian.hpp"
#include "hamiltonian/qubit_hamiltonian.hpp"
#include "ternarytree/tt_mappings.hpp"
#include "tableau/pauli_rotation.hpp"

namespace qsyn::hamiltonian {

// Fermion-to-qubit mapping interface
class FermionToQubitMapping {
public:
    FermionToQubitMapping(std::size_t n_modes) : _n_modes(n_modes) {}
    virtual ~FermionToQubitMapping() = default;
    /**
     * @brief Map an annihilation or creation operator to a qubit Hamiltonian.
     * @param n_qubits The number of qubits.
     * @param p The index of the fermion.
     * @param is_creation Whether the operator is a creation (true) or
                          annihilation (false) operator.
     * @return The list of ComplexPauliTerms representing the mapped operator.
     */
    virtual std::vector<ComplexPauliTerm>
    map(std::size_t p, bool is_creation) const = 0;

    std::size_t n_modes() const { return _n_modes; }

protected:
    std::size_t _n_modes;
};

class JordanWignerMapping : public FermionToQubitMapping {
public:
    JordanWignerMapping(std::size_t n_modes)
        : FermionToQubitMapping(n_modes) {}
    ~JordanWignerMapping() override = default;
    std::vector<ComplexPauliTerm> map(
        std::size_t p, bool is_creation) const override;
};

class TernaryTreeMapping : public FermionToQubitMapping {
public:
    TernaryTreeMapping(std::size_t n_modes)
        : FermionToQubitMapping(n_modes), _mapper(n_modes) {}
    TernaryTreeMapping(TernaryTree tree)
        : FermionToQubitMapping(tree.num_qubits()), _mapper(std::move(tree)) {}
    ~TernaryTreeMapping() override = default;

    std::vector<ComplexPauliTerm> map(std::size_t p, bool is_creation) const override;

private:
    TTMapper _mapper;
};

QubitHamiltonian qubitize(
    FermionHamiltonian const& f_hamilt,
    FermionToQubitMapping const& mapping);

}  // namespace qsyn::hamiltonian
