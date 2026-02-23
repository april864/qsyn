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

namespace qsyn::hamiltonian {

TernaryTree::TernaryTree(int num_qubits) {
    assert(num_qubits > 0);

    _root = std::make_unique<TernaryNode>(0);
    _nodes.push_back(_root.get());

    int index = 1;
    int parent_index = 0;
    while (index < num_qubits) {
        TernaryNode* parent = _nodes[parent_index];

        if (index < num_qubits) {
            auto child = std::make_unique<TernaryNode>(index++, parent, BranchType::LEFT);
            _nodes.push_back(child.get());
            parent->left = std::move(child);
        }

        if (index < num_qubits) {
            auto child = std::make_unique<TernaryNode>(index++, parent, BranchType::MID);
            _nodes.push_back(child.get());
            parent->mid = std::move(child);
        }

        if (index < num_qubits) {
            auto child = std::make_unique<TernaryNode>(index++, parent, BranchType::RIGHT);
            _nodes.push_back(child.get());
            parent->right = std::move(child);
        }

        parent_index++;
    }
}

void TernaryTree::assign_label(int node_index, int qubit_label) {
    {
        auto node = _nodes[node_index];
        node->qubit_label = qubit_label;
        _label_to_node[qubit_label] = node;
    }
}

}