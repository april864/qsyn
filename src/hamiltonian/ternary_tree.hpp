/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Ternary tree structure ]
  Author       [ April Wang (april864) ]
*/

#pragma once

#include <memory>
#include <vector>
#include <unordered_map>

#include "qubit_hamiltonian.hpp"

namespace qsyn::hamiltonian {

enum class BranchType {
    LEFT,
    MID,
    RIGHT,
    ROOT
};

struct TernaryNode {
    std::unique_ptr<TernaryNode> left, mid, right;
    TernaryNode* parent;

    int node_index;
    int qubit_label = -1;
    BranchType branch;

    TernaryNode(int index, TernaryNode* p = nullptr, BranchType b = BranchType::ROOT) 
        : node_index(index), parent(p), branch(b) {}
};

// struct TernaryTreeLeg{
//     TernaryNode* parent;
//     BranchType branch;

//     TernaryTreeLeg(TernaryNode* p = nullptr, BranchType b = BranchType::ROOT)
//         : parent(p), branch(b) {}
// };

class TernaryTree {
    public:
        int num_qubits;
        TernaryTree(int num_qubits);

        TernaryNode* get_root() { return _root.get();}
        void assign_qubit(int node_index, int qubit_label);
        TernaryNode* get_node_by_qubit(int qubit_label);

    private:
        std::unique_ptr<TernaryNode> _root;
        std::unordered_map<int, TernaryNode*> _index_to_node;
        std::unordered_map<int, TernaryNode*> _qubit_to_node;
        // std::vector<std::unique_ptr<TernaryTreeLeg>> _available_legs;
};

} // namespace qsyn::hamiltonian