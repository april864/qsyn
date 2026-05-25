/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Ternary tree mappings ]
  Author       [ April Wang (april864) ]
*/

#include "tt_mappings.hpp"

#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "hamiltonian/qubit_hamiltonian.hpp"
#include "tableau/pauli_product_trait.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "ternary_tree.hpp"

namespace qsyn::hamiltonian {

using Pauli = qsyn::tableau::Pauli;

namespace {
Pauli to_pauli(BranchType branch) {
    switch (branch) {
        case BranchType::left:
            return Pauli::x;
        case BranchType::mid:
            return Pauli::y;
        case BranchType::right:
            return Pauli::z;
    }
    DVLAB_UNREACHABLE("Every branch type should be handled in the switch-case");
}
}  // namespace

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

            pauli_str[parent_id] = to_pauli(edge->branch);

            curr = parent;
        }

        _pauli_strs.emplace(leg, std::move(pauli_str));
    }
}

void TTMapper::_pair_legs() {
    for (size_t i = 0; i < _tree.num_qubits(); i++) {
        TernaryNode* curr = _tree.get_node_by_index(i);

        TernaryNode* left_path = curr->get_left_child();
        while (!left_path->is_leg()) {
            left_path = left_path->get_right_child();
        }

        TernaryNode* right_path = curr->get_mid_child();
        while (!right_path->is_leg()) {
            right_path = right_path->get_right_child();
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

/**
 * @brief Pauli-string product P * Q for the strings mapped from two legs.
 *
 * Paired legs in the ternary-tree mapping anticommute, so the product is
 * ±i times a Pauli string (not a real ±1). The phase uses the same rules
 * as ComplexPauliTerm multiplication (e.g. for exp(-π/4 · P Q)).
 */
ComplexPauliTerm TTMapper::leg_pauli_product(TernaryLeg* leg_p, TernaryLeg* leg_q) const {
    auto const& p_str = _pauli_strs.at(leg_p);
    auto const& q_str = _pauli_strs.at(leg_q);
    return ComplexPauliTerm(p_str, std::complex<double>(1.0, 0.0)) *
           ComplexPauliTerm(q_str, std::complex<double>(1.0, 0.0));
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

namespace {

TernaryQubitNode* as_qubit_node(TernaryNode* node) {
    return static_cast<TernaryQubitNode*>(node);
}

TernaryQubitNode* qubit_child(TernaryNode* parent, BranchType branch) {
    auto* child = parent->get_child(branch);
    if (!child || child->is_leg()) {
        return nullptr;
    }
    return as_qubit_node(child);
}

TernaryNode* get_node_by_index(TernaryTree const& tree, size_t index) {
    return tree.get_node_by_index(index);
}

void debraid_and_append_gates(
    TTMapper const& mapper,
    size_t mode,
    std::pair<TernaryLeg*, TernaryLeg*> const& leg_pair,
    std::vector<tableau::CliffordOperator>& clifford_circuit_adjoint) {
    auto* qubit_node = as_qubit_node(get_node_by_index(mapper.tree(), mode));
    if (!qubit_node->is_braided) {
        return;
    }

    // Recall that for mode $i$,
    // not braided: $a_i =  0.5 S_{2i}   + 0.5i S_{2i+1}$
    //     braided: $a_i = -0.5 S_{2i+1} + 0.5i S_{2i}$
    // To debraid, it suffices to apply
    // $$U_{2i, 2i+1} = \exp(-pi/4 S_{2i} S_{2i+1})$$
    // to each braided leg pair.

    auto [left_leg, right_leg] = leg_pair;
    auto pauli_product         = mapper.leg_pauli_product(left_leg, right_leg);

    constexpr std::complex<double> plus_i(0, 1);
    constexpr std::complex<double> minus_i(0, -1);

    DVLAB_ASSERT(pauli_product.coeff() == plus_i || pauli_product.coeff() == minus_i,
                 "Coefficient must be ±i");

    auto const phase_gate = pauli_product.coeff() == plus_i
                                ? tableau::CliffordOperatorType::s
                                : tableau::CliffordOperatorType::sdg;

    // REVIEW: maybe make extract_clifford_operators work on PauliProduct directly?
    auto const pauli_rotation = tableau::PauliRotation(
        pauli_product.pauli_product(),
        dvlab::Phase(1));  // this phase is just a placeholder

    auto [ops, qubit] = tableau::extract_clifford_operators(pauli_rotation);

    clifford_circuit_adjoint.insert(clifford_circuit_adjoint.end(), ops.begin(), ops.end());
    clifford_circuit_adjoint.push_back({phase_gate, {qubit, 0}});
    tableau::adjoint_inplace(ops);
    clifford_circuit_adjoint.insert(clifford_circuit_adjoint.end(), ops.begin(), ops.end());
}

std::vector<size_t> post_order_qubit_indices(TernaryTree const& tree) {
    std::vector<size_t> traversal;
    std::vector<TernaryQubitNode*> stack;
    stack.push_back(as_qubit_node(tree.get_root()));
    while (!stack.empty()) {
        auto const v = stack.back();
        stack.pop_back();
        traversal.push_back(v->id);
        for (auto const& branch : {BranchType::left, BranchType::mid, BranchType::right}) {
            auto const child = v->get_child(branch);
            if (child && !child->is_leg()) {
                stack.push_back(as_qubit_node(child));
            }
        }
    }
    std::ranges::reverse(traversal);
    return traversal;
}

// Virtual SWAP of qubit labels mode_id_lo and mode_id_hi by rewriting CX gates already
// recorded for this descendant subtree (e.g. CX(5, 7) -> CX(7, 5), CX(6, 7) -> CX(6, 5)).
void retroactive_swap_cx_for_pair(
    size_t mode_id_lo,
    size_t mode_id_hi,
    std::set<size_t> const& descendant_indices,
    std::vector<tableau::CliffordOperator>& clifford_circuit_adjoint) {
    using CliffordOperatorType = tableau::CliffordOperatorType;
    for (auto& op : clifford_circuit_adjoint) {
        if (op.first != CliffordOperatorType::cx) {
            continue;
        }
        auto& ctrl = op.second[0];
        auto& targ = op.second[1];
        if (ctrl == mode_id_lo && targ == mode_id_hi) {
            std::swap(ctrl, targ);
        } else if (targ == mode_id_hi && descendant_indices.contains(ctrl)) {
            targ = mode_id_lo;
        }
    }
}

void append_swap_network(
    std::set<size_t> const& descendant_indices,
    std::vector<tableau::CliffordOperator>& clifford_circuit_adjoint) {
    if (descendant_indices.size() < 2) {
        return;
    }
    auto it_lo = descendant_indices.begin();
    auto it_hi = std::prev(descendant_indices.end());
    for (size_t i = 0; i < descendant_indices.size() / 2; ++i, ++it_lo, --it_hi) {
        retroactive_swap_cx_for_pair(*it_lo, *it_hi, descendant_indices, clifford_circuit_adjoint);
    }
}

void append_cx_from_descendants(
    std::set<size_t> const& control_indices,
    size_t target_idx,
    std::vector<tableau::CliffordOperator>& clifford_circuit_adjoint) {
    for (auto const ctrl_idx : control_indices) {
        clifford_circuit_adjoint.push_back(
            {tableau::CliffordOperatorType::cx, {ctrl_idx, target_idx}});
    }
}

}  // namespace

/**
 * @brief Convert a ternary-tree mapping to a Clifford tableau.
 *        The Clifford tableau represents an operator C such that
 *        \Phi_{T} = C \Phi_{\text{JW}} C^\dagger represents the F2Q mapping
 *        associated with the ternary tree.
 * @param mapper Ternary-tree mapper with leg pairing and Pauli strings loaded.
 * @return Stabilizer tableau
 */
tableau::StabilizerTableau to_clifford(TTMapper const& mapper) {
    using CliffordOperator     = tableau::CliffordOperator;
    using CliffordOperatorType = tableau::CliffordOperatorType;
    // collect gates instead of directly applying them
    // so taking adjoint at the end is easier.
    std::vector<CliffordOperator> clifford_circuit_adjoint;

    for (auto const& [mode, leg_pair] : mapper.leg_pairs()) {
        debraid_and_append_gates(mapper, mode, leg_pair, clifford_circuit_adjoint);
    }

    // calculate the adjoint circuit for the PPTT mapping
    // Here we adapt Algorithm 1 in the paper
    // [[2505.06212] From Fermions to Qubits: A ZX-Calculus Perspective](https://arxiv.org/abs/2505.06212)
    // we follow the recursive ZXW diagrams in the paper implicitly.

    // descendants of each qubit, including itself
    std::vector<std::set<size_t>> descendants(
        mapper.tree().num_qubits(), std::set<size_t>{});

    for (auto const& idx : post_order_qubit_indices(mapper.tree())) {
        auto* qubit_node  = as_qubit_node(get_node_by_index(mapper.tree(), idx));
        auto* left_child  = qubit_child(qubit_node, BranchType::left);
        auto* mid_child   = qubit_child(qubit_node, BranchType::mid);
        auto* right_child = qubit_child(qubit_node, BranchType::right);

        if (mid_child) {
            append_swap_network(descendants[mid_child->id], clifford_circuit_adjoint);
        }

        if (left_child) {
            append_cx_from_descendants(
                descendants[left_child->id],
                idx,
                clifford_circuit_adjoint);
        }

        if (mid_child) {
            append_cx_from_descendants(
                descendants[mid_child->id],
                idx,
                clifford_circuit_adjoint);
        }

        // collects the descendants of the children and add itself
        descendants[idx].emplace(idx);
        if (left_child) {
            descendants[idx].merge(descendants[left_child->id]);
        }
        if (mid_child) {
            descendants[idx].merge(descendants[mid_child->id]);
        }
        if (right_child) {
            descendants[idx].merge(descendants[right_child->id]);
        }
    }

    // consolidate the clifford gates into a StabilizerTableau
    auto clifford = tableau::StabilizerTableau(mapper.tree().num_qubits());
    clifford.apply(tableau::adjoint(clifford_circuit_adjoint));
    return clifford;
}

}  // namespace qsyn::hamiltonian
