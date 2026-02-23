/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Ternary tree structure ]
  Author       [ April Wang (april864) ]
*/

#pragma once

#include <memory>
#include <vector>
#include <unordered_map>

namespace qsyn::hamiltonian {

enum class BranchType {
    LEFT,
    MID,
    RIGHT,
    ROOT
};

struct TernaryNode {
    std::unique_ptr<TernaryNode> left, mid, right;
    TernaryNode* parent = nullptr;

    int node_index;
    int qubit_label = -1;
    BranchType branch;

    TernaryNode(int index, TernaryNode* p = nullptr, BranchType b = BranchType::ROOT) 
        : node_index(index), parent(p), branch(b) {}
};

class TernaryTree {
    public:
        TernaryTree(int num_qubits);

        void assign_label(int node_index, int qubit_label);

    private:
        std::unique_ptr<TernaryNode> _root;
        std::vector<TernaryNode*> _nodes;
        std::unordered_map<int, TernaryNode*> _label_to_node;
};

} // namespace