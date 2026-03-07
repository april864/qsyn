/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Ternary tree mappings ]
  Author       [ April Wang (april864) ]
*/

#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "qubit_hamiltonian.hpp"
#include "ternary_tree.hpp"

namespace qsyn::hamiltonian {

using Pauli = qsyn::tableau::Pauli;

// struct FermionOpPair {
//     ComplexPauliTerm left;
//     ComplexPauliTerm right;
// };

struct FermionOps {
    std::vector<ComplexPauliTerm> creation;
    std::vector<ComplexPauliTerm> annihilation;
};

class TTMapper {
public:
    TTMapper(int num_qubits)
        : _tree(TernaryTree(num_qubits)) {
        _basic_assign_qubits();
        _basic_load_pauli_strs();
        _pair_legs();
        _load_ferm_ops();
    }

    std::vector<ComplexPauliTerm> get_pauli_str(int mode, bool is_creation) const {
        return is_creation ? _mode_to_ferm_ops.at(mode).creation : _mode_to_ferm_ops.at(mode).annihilation;
    }

private:
    TernaryTree _tree;
    std::unordered_map<TernaryLeg*, std::vector<Pauli>> _pauli_strs;
    std::unordered_map<int, std::pair<TernaryLeg*, TernaryLeg*>> _leg_pairs;
    std::unordered_map<int, FermionOps> _mode_to_ferm_ops;

    void _basic_assign_qubits();
    void _basic_load_pauli_strs();
    void _pair_legs();
    void _load_ferm_ops();
};

}  // namespace qsyn::hamiltonian
