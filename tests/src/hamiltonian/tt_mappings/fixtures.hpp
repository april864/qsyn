/*
  Shared fixtures for TTMapper / to_clifford unit tests.
*/

#pragma once

#include "hamiltonian/ternarytree/tt_mappings.hpp"

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

namespace qsyn::hamiltonian::tt_mappings_test {

using qsyn::tableau::StabilizerTableau;

// --- Chain trees ---

inline TernaryTree make_jw_chain_tree(size_t num_qubits) {
    TernaryTree tree;
    TernaryNode* curr = tree.get_root();
    for (size_t i = 1; i < num_qubits; ++i) {
        tree.add_qubit_node(curr, BranchType::right);
        curr = curr->get_right_child();
    }
    tree.append_legs_to_tree();
    for (size_t i = 0; i < num_qubits; ++i) {
        tree.assign_qubit(i, i);
    }
    return tree;
}

inline TernaryTree make_parity_chain_tree(size_t num_qubits) {
    TernaryTree tree;
    TernaryNode* curr = tree.get_root();
    for (size_t i = 1; i < num_qubits; ++i) {
        tree.add_qubit_node(curr, BranchType::left);
        curr = curr->get_left_child();
    }
    tree.append_legs_to_tree();
    for (size_t i = 0; i < num_qubits / 2; ++i) {
        tree.swap_indices(i, num_qubits - 1 - i);
    }
    for (size_t i = 0; i < num_qubits; ++i) {
        tree.assign_qubit(i, i);
    }
    return tree;
}

// 4 -> (1, 7, 8); 1 -> (0, 2, 3); 7 -> (5, leg, leg); 5 -> (leg, leg, 6)
inline TernaryTree make_branching_example_tree() {
    TernaryTree tree;
    auto* root = dynamic_cast<TernaryQubitNode*>(tree.get_root());
    std::vector<TernaryQubitNode*> nodes;
    nodes.push_back(root);

    size_t const i1 = tree.add_qubit_node(root, BranchType::left);
    size_t const i7 = tree.add_qubit_node(root, BranchType::mid);
    size_t const i8 = tree.add_qubit_node(root, BranchType::right);
    nodes.push_back(dynamic_cast<TernaryQubitNode*>(tree.get_node_by_index(i1)));
    nodes.push_back(dynamic_cast<TernaryQubitNode*>(tree.get_node_by_index(i7)));
    nodes.push_back(dynamic_cast<TernaryQubitNode*>(tree.get_node_by_index(i8)));

    auto* n1        = nodes[1];
    size_t const i0 = tree.add_qubit_node(n1, BranchType::left);
    size_t const i2 = tree.add_qubit_node(n1, BranchType::mid);
    size_t const i3 = tree.add_qubit_node(n1, BranchType::right);
    nodes.push_back(dynamic_cast<TernaryQubitNode*>(tree.get_node_by_index(i0)));
    nodes.push_back(dynamic_cast<TernaryQubitNode*>(tree.get_node_by_index(i2)));
    nodes.push_back(dynamic_cast<TernaryQubitNode*>(tree.get_node_by_index(i3)));

    auto* n7        = nodes[2];
    size_t const i5 = tree.add_qubit_node(n7, BranchType::left);
    nodes.push_back(dynamic_cast<TernaryQubitNode*>(tree.get_node_by_index(i5)));

    auto* n5        = nodes[7];
    size_t const i6 = tree.add_qubit_node(n5, BranchType::right);
    nodes.push_back(dynamic_cast<TernaryQubitNode*>(tree.get_node_by_index(i6)));

    tree.append_legs_to_tree();

    std::array<size_t, 9> const slot_of_label = {4, 1, 5, 6, 0, 7, 8, 2, 3};
    tree.remap_node_ids(slot_of_label);
    for (size_t label = 0; label < slot_of_label.size(); ++label) {
        tree.assign_qubit(label, label);
    }
    return tree;
}

// --- Braiding helpers ---

inline void set_braided(TernaryTree& tree, std::vector<size_t> const& node_indices) {
    for (size_t const index : node_indices) {
        auto* qnode = dynamic_cast<TernaryQubitNode*>(tree.get_node_by_index(index));
        REQUIRE(qnode != nullptr);
        qnode->is_braided = true;
    }
}

// exp(-i pi/4 Z_j) from +i Z_j leg pair (JW / parity mode 0).
inline void apply_single_z_debraid(StabilizerTableau& clifford, size_t qubit) {
    clifford.sdg(qubit);
}

// exp(-i pi/4 Z_{j-1} Z_j) from +i Z_{j-1} Z_j leg pair (parity mode j > 0).
inline void apply_zz_debraid(StabilizerTableau& clifford, size_t j) {
    clifford.cx(j - 1, j).sdg(j).cx(j - 1, j);
}

// --- Expected Cliffords ---

inline StabilizerTableau expected_parity_clifford(size_t num_qubits) {
    StabilizerTableau clifford(num_qubits);
    for (size_t i = num_qubits; i-- > 0;) {
        for (size_t j = num_qubits; j-- > i + 1;) {
            clifford.cx(i, j);
        }
    }
    return clifford;
}

inline StabilizerTableau expected_debraid_clifford(
    size_t num_qubits, std::vector<size_t> const& braided_modes) {
    StabilizerTableau clifford(num_qubits);
    for (size_t const mode : braided_modes) {
        apply_single_z_debraid(clifford, mode);
    }
    return clifford;
}

inline StabilizerTableau expected_parity_with_braiding(
    size_t num_qubits, std::vector<size_t> const& braided_modes) {
    StabilizerTableau clifford = expected_parity_clifford(num_qubits);
    for (size_t const mode : braided_modes) {
        if (mode == 0) {
            apply_single_z_debraid(clifford, 0);
        } else {
            apply_zz_debraid(clifford, mode);
        }
    }
    return clifford;
}

inline StabilizerTableau expected_branching_example_clifford() {
    constexpr size_t num_qubits = 9;
    StabilizerTableau clifford(num_qubits);
    constexpr std::array<std::pair<size_t, size_t>, 11> cx_gates = {{
        {0, 4},
        {1, 4},
        {2, 4},
        {3, 4},
        {5, 4},
        {6, 4},
        {7, 4},
        {2, 1},
        {0, 1},
        {6, 5},
        {7, 5},
    }};
    for (auto const& [ctrl, targ] : cx_gates) {
        clifford.cx(ctrl, targ);
    }
    return clifford;
}

inline StabilizerTableau expected_branching_braided_4_7_clifford() {
    StabilizerTableau clifford(9);
    // PPTT + debraids for exp(-i pi/4 Z_5 Z_6 Z_7) and exp(-i pi/4 Z_1 Z_3 Z_4 Z_7),
    // after the virtual 5<->7 swap network at node 4's mid child.
    clifford.sdg(7)
        .cx(7, 5)
        .cx(6, 5)
        .cx(7, 4)
        .cx(5, 4)
        .sdg(4)
        .cx(7, 4)
        .cx(3, 4)
        .cx(2, 1)
        .cx(1, 4)
        .cx(0, 4)
        .cx(0, 1);
    return clifford;
}

// --- Assertions / diagnostics ---

inline void require_iz_pauli_product(
    ComplexPauliTerm const& product,
    std::vector<size_t> const& z_qubits) {
    REQUIRE(product.coeff() == std::complex<double>(0, 1));
    for (size_t const q : z_qubits) {
        REQUIRE(product.get_pauli_type(q) == qsyn::tableau::Pauli::z);
    }
    for (size_t q = 0; q < product.n_qubits(); ++q) {
        if (std::ranges::find(z_qubits, q) == z_qubits.end()) {
            REQUIRE(product.is_i(q));
        }
    }
}

inline std::string format_synthesized_cx_circuit(StabilizerTableau const& clifford) {
    using qsyn::tableau::CliffordOperatorType;
    std::string out;
    for (auto const& [type, qubits] : qsyn::tableau::extract_clifford_operators(clifford)) {
        if (type == CliffordOperatorType::cx) {
            out += "CX " + std::to_string(qubits[0]) + " " + std::to_string(qubits[1]) + '\n';
        }
    }
    if (out.empty()) {
        out = "(no CX gates synthesized)\n";
    }
    return out;
}

}  // namespace qsyn::hamiltonian::tt_mappings_test
