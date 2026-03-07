/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Ternary tree structure ]
  Author       [ April Wang (april864) ]
*/

#include "ternary_tree.hpp"

#include <cassert>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "qubit_hamiltonian.hpp"

namespace qsyn::hamiltonian {

TernaryEdge* TernaryNode::get_edge(BranchType branch) const {
    switch (branch) {
        case BranchType::LEFT:
            return left.get();
        case BranchType::MID:
            return mid.get();
        case BranchType::RIGHT:
            return right.get();
    }
    return nullptr;
}

TernaryTree::TernaryTree(int num_qubits) : num_qubits(num_qubits) {
    if (num_qubits < 0) {
        throw std::invalid_argument("invalid number of qubits");
    }

    std::vector<TernaryNode*> nodes;

    _root = std::make_unique<TernaryNode>(0);
    nodes.push_back(_root.get());
    _index_to_node[0] = _root.get();

    int index = 1;

    int parent_index = 0;
    while (index < num_qubits) {
        TernaryNode* parent = nodes[parent_index];

        for (auto [branch, edge_slot] : {
                 std::pair{BranchType::LEFT, &parent->left},
                 std::pair{BranchType::MID, &parent->mid},
                 std::pair{BranchType::RIGHT, &parent->right}}) {
            if (index >= num_qubits) break;

            auto child             = std::make_unique<TernaryNode>(index++, parent);
            TernaryNode* child_ptr = child.get();

            nodes.push_back(child_ptr);
            _index_to_node[child_ptr->node_index] = child_ptr;

            auto edge                = std::make_unique<TernaryEdge>(parent, std::move(child), branch);
            child_ptr->incoming_edge = static_cast<TernaryEdge*>(edge.get());
            *edge_slot               = std::move(edge);
        }

        parent_index++;
    }

    // Add legs
    // TODO: Make into helper func or integrate into above loop?
    size_t num_qubit_nodes = nodes.size();
    for (size_t i = 0; i < num_qubit_nodes; ++i) {
        TernaryNode* node = nodes[i];
        for (auto [branch, edge_slot] : {
                 std::pair{BranchType::LEFT, &node->left},
                 std::pair{BranchType::MID, &node->mid},
                 std::pair{BranchType::RIGHT, &node->right}}) {
            if (!*edge_slot) {
                auto leg_node       = std::make_unique<TernaryLeg>(node);
                TernaryLeg* leg_ptr = leg_node.get();

                auto edge              = std::make_unique<TernaryEdge>(node, std::move(leg_node), branch);
                leg_ptr->incoming_edge = static_cast<TernaryEdge*>(edge.get());
                *edge_slot             = std::move(edge);
                _legs.push_back(leg_ptr);
            }
        }
    }
}

// Current invariant: qubit labels start at 0 and increment by 1 until num_qubits - 1
// Required for _basic_load_pauli_strs() in tt_mappings.
// TODO: fix this^ somehow
void TernaryTree::assign_qubit(int node_index, int qubit_label) {
    {
        TernaryNode* node = _index_to_node.at(node_index);

        node->qubit_label           = qubit_label;
        _qubit_to_node[qubit_label] = node;
    }
}

}  // namespace qsyn::hamiltonian
