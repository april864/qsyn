/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Ternary tree mappings ]
  Author       [ April Wang (april864) ]
*/

#include "tt_mappings.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

#include "qubit_hamiltonian.hpp"
#include "ternary_tree.hpp"

namespace qsyn::hamiltonian {

using Pauli = qsyn::tableau::Pauli;

std::unordered_map<BranchType, Pauli> simple_branch_assignment = {
    {BranchType::LEFT, Pauli::x},
    {BranchType::MID, Pauli::y},
    {BranchType::RIGHT, Pauli::z}};

/**
 * @brief Basic method of assigning qubits to nodes.
 *        Assigns qubit i to node i.
 */
void TTMapper::_basic_assign_qubits() {
    for (int i = 0; i < _tree.num_qubits; i++) {
        _tree.assign_qubit(i, i);
    }
}

/**
 * @brief Basic method of getting fermionic operators from Bonsai paper.
 */
void TTMapper::_basic_load_pauli_strs() {
    for (TernaryLeg* leg : _tree.get_legs()) {
        std::vector<Pauli> pauli_str(_tree.num_qubits, Pauli::i);

        TernaryNode* curr = leg;
        while (curr->incoming_edge) {
            TernaryEdge* edge   = curr->incoming_edge;
            TernaryNode* parent = edge->source;

            pauli_str[parent->qubit_label] = simple_branch_assignment.at(edge->branch);

            curr = parent;
        }

        _pauli_strs.emplace(leg, std::move(pauli_str));
    }
}

void TTMapper::_pair_legs() {
    for (int i = 0; i < _tree.num_qubits; i++) {
        TernaryNode* curr = _tree.get_node_by_index(i);

        TernaryNode* left_path = curr->left.get()->target.get();
        while (!left_path->is_leg()) {
            left_path = left_path->right.get()->target.get();
        }

        TernaryNode* right_path = curr->mid.get()->target.get();
        while (!right_path->is_leg()) {
            right_path = right_path->right.get()->target.get();
        }

        assert(left_path && left_path->is_leg());
        assert(right_path && right_path->is_leg());

        auto left_leg  = static_cast<TernaryLeg*>(left_path);
        auto right_leg = static_cast<TernaryLeg*>(right_path);
        _leg_pairs.emplace(i, std::pair{left_leg, right_leg});
    }
}

void TTMapper::_load_ferm_ops() {
    for (int i = 0; i < _tree.num_qubits; ++i) {
        auto left_str  = _pauli_strs.at(_leg_pairs.at(i).first);
        auto right_str = _pauli_strs.at(_leg_pairs.at(i).second);

        FermionOps ops;
        ops.creation = {ComplexPauliTerm(left_str, std::complex<double>(0.5, 0)),
                        ComplexPauliTerm(right_str, std::complex<double>(0, -0.5))};

        ops.annihilation = {ComplexPauliTerm(left_str, std::complex<double>(0.5, 0)),
                            ComplexPauliTerm(right_str, std::complex<double>(0, 0.5))};

        _mode_to_ferm_ops.emplace(i, std::move(ops));
    }
}

}  // namespace qsyn::hamiltonian
