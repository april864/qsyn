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

std::unordered_map<BranchType, Pauli> const simple_branch_assignment = {
    {BranchType::left, Pauli::x},
    {BranchType::mid, Pauli::y},
    {BranchType::right, Pauli::z}};

/**
 * @brief Basic method of assigning qubits to nodes.
 *        Assigns qubit i to node i.
 */
void TTMapper::_basic_assign_qubits() {
    for (size_t i = 0; i < _tree.num_qubits(); i++) {
        _tree.assign_qubit(i, i);
    }
}

/**
 * @brief Basic method of getting fermionic operators from Bonsai paper.
 */
void TTMapper::_basic_load_pauli_strs() {
    for (TernaryLeg* leg : _tree.get_legs()) {
        std::vector<Pauli> pauli_str(_tree.num_qubits(), Pauli::i);

        TernaryNode* curr = leg;
        while (curr->incoming_edge) {
            TernaryEdge* edge   = curr->incoming_edge;
            TernaryNode* parent = edge->source;

            auto const parent_qubit_node = dynamic_cast<TernaryQubitNode*>(parent);
            assert(parent_qubit_node);

            auto const parent_id = parent_qubit_node->id;

            assert(parent_id < _tree.num_qubits());

            pauli_str[parent_id] = simple_branch_assignment.at(edge->branch);

            curr = parent;
        }

        _pauli_strs.emplace(leg, std::move(pauli_str));
    }
}

void TTMapper::_pair_legs() {
    for (size_t i = 0; i < _tree.num_qubits(); i++) {
        TernaryNode* curr = _tree.get_node_by_index(i);

        TernaryNode* left_path = curr->get_left()->target.get();
        while (!left_path->is_leg()) {
            left_path = left_path->get_right()->target.get();
        }

        TernaryNode* right_path = curr->get_mid()->target.get();
        while (!right_path->is_leg()) {
            right_path = right_path->get_right()->target.get();
        }

        assert(left_path && left_path->is_leg());
        assert(right_path && right_path->is_leg());

        auto left_leg  = dynamic_cast<TernaryLeg*>(left_path);
        auto right_leg = dynamic_cast<TernaryLeg*>(right_path);
        DVLAB_ASSERT(left_leg, "left_path is not a leg");
        DVLAB_ASSERT(right_path, "right_path is not a leg");
        _leg_pairs.emplace(i, std::pair{left_leg, right_leg});
    }
}

void TTMapper::_load_ferm_ops() {
    for (size_t i = 0; i < _tree.num_qubits(); ++i) {
        auto left_str  = _pauli_strs.at(_leg_pairs.at(i).first);
        auto right_str = _pauli_strs.at(_leg_pairs.at(i).second);

        auto* qubit_node = dynamic_cast<TernaryQubitNode*>(_tree.get_node_by_index(i));
        assert(qubit_node);

        FermionOps ops;
        if (!qubit_node->is_braided) {
            // Bonsai: a_j^ = 0.5 Sx - 0.5i Sy
            // Treespilation (USE THIS ONE): a_j^ = 0.5 Sx + 0.5i Sy
            ops.creation = {ComplexPauliTerm(left_str, std::complex<double>(0.5, 0)),
                            ComplexPauliTerm(right_str, std::complex<double>(0, 0.5))};
            // Bonsai: a_j = 0.5 Sx + 0.5i Sy
            // Treespilation (USE THIS ONE): a_j = 0.5 Sx - 0.5i Sy
            ops.annihilation = {ComplexPauliTerm(left_str, std::complex<double>(0.5, 0)),
                                ComplexPauliTerm(right_str, std::complex<double>(0, -0.5))};
        } else {
            // a_j^ = -0.5 Sy + 0.5i Sx
            ops.creation = {ComplexPauliTerm(left_str, std::complex<double>(0, 0.5)),
                            ComplexPauliTerm(right_str, std::complex<double>(-0.5, 0))};
            // a_j = -0.5 Sy - 0.5i Sx
            ops.annihilation = {ComplexPauliTerm(left_str, std::complex<double>(0, -0.5)),
                                ComplexPauliTerm(right_str, std::complex<double>(-0.5, 0))};
        }

        _mode_to_ferm_ops.emplace(i, std::move(ops));
    }
}

}  // namespace qsyn::hamiltonian
