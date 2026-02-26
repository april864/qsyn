/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Ternary tree structure ]
  Author       [ April Wang (april864) ]
*/

#include <memory>
#include <vector>
#include <unordered_map>
#include <cassert>
#include <utility>

#include "ternary_tree.hpp"
#include "qubit_hamiltonian.hpp"

namespace qsyn::hamiltonian {

TernaryTree::TernaryTree(int num_qubits) : num_qubits(num_qubits){
    assert(num_qubits > 0);
    std::vector<TernaryNode*> nodes;

    _root = std::make_unique<TernaryNode>(0);
    nodes.push_back(_root.get());
    _index_to_node[0] = _root.get();

    int index = 1;

    int parent_index = 0;
    while (index < num_qubits) {
        TernaryNode* parent = nodes[parent_index];

        if (index < num_qubits) {
            auto child = std::make_unique<TernaryNode>(index++, parent, BranchType::LEFT);
            nodes.push_back(child.get());
            _index_to_node[child->node_index] = child.get();
            parent->left = std::move(child);
        }

        if (index < num_qubits) {
            auto child = std::make_unique<TernaryNode>(index++, parent, BranchType::MID);
            nodes.push_back(child.get());
            _index_to_node[child->node_index] = child.get();
            parent->mid = std::move(child);
        }

        if (index < num_qubits) {
            auto child = std::make_unique<TernaryNode>(index++, parent, BranchType::RIGHT);
            nodes.push_back(child.get());
            _index_to_node[child->node_index] = child.get();
            parent->right = std::move(child);
        }

        parent_index++;
    }

    // // Add legs
    // size_t num_qubit_nodes = nodes.size();
    // for (size_t i = 0; i < num_qubit_nodes; ++i) {
    //     TernaryNode* node = nodes[i];
    //     if (!node->left) {
    //         auto leg = std::make_unique<TernaryTreeLeg>(node, BranchType::LEFT);
    //         _available_legs.push_back(std::move(leg));
    //     }
    //     if (!node->mid) {
    //         auto leg = std::make_unique<TernaryTreeLeg>(node, BranchType::MID);
    //          _available_legs.push_back(std::move(leg));
    //     }
    //     if (!node->right) {
    //         auto leg = std::make_unique<TernaryTreeLeg>(node, BranchType::RIGHT);
    //          _available_legs.push_back(std::move(leg));
    //     }
    // }
}

// Current invariant: qubit labels start at 0 and increment by 1 until num_qubits - 1
// Required for _basic_load_ferm_ops() in tt_mappings.
// TODO: fix this somehow
void TernaryTree::assign_qubit(int node_index, int qubit_label) {
    {
        TernaryNode* node = _index_to_node.at(node_index);

        node->qubit_label = qubit_label;
        _qubit_to_node[qubit_label] = node;
    }
}

TernaryNode* TernaryTree::get_node_by_qubit(int qubit_label) {
    return _qubit_to_node.at(qubit_label);
}

} // namespace qsyn::hamiltonian