/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Ternary tree structure ]
  Author       [ April Wang (april864) ]
*/

#pragma once

#include <memory>
#include <unordered_map>
#include <variant>
#include <vector>

#include "qubit_hamiltonian.hpp"

namespace qsyn::hamiltonian {

enum class BranchType {
    LEFT,
    MID,
    RIGHT,
    ROOT
};

struct TernaryEdge;

struct TernaryNode {
    int node_index;
    int qubit_label = -1;

    TernaryNode* parent;

    std::unique_ptr<TernaryEdge> left, mid, right;
    TernaryEdge* incoming_edge;

    TernaryNode(int index, TernaryNode* p = nullptr, TernaryEdge* e = nullptr)
        : node_index(index), parent(p), incoming_edge(e) {}

    virtual ~TernaryNode() = default;
    virtual bool is_leg() const { return false; }

    TernaryEdge* get_edge(BranchType branch) const;
};

struct TernaryLeg : public TernaryNode {
    TernaryLeg(TernaryNode* p = nullptr, TernaryEdge* e = nullptr)
        : TernaryNode(-1, p, e) {}

    bool is_leg() const override { return true; }
};

struct TernaryEdge {
    TernaryNode* source;
    std::unique_ptr<TernaryNode> target;
    BranchType branch;

    TernaryEdge(TernaryNode* s, std::unique_ptr<TernaryNode>&& t, BranchType b)
        : source(s), target(std::move(t)), branch(b) {}
};

class TernaryTree {
public:
    int const num_qubits;

    TernaryTree(int num_qubits);
    void assign_qubit(int node_index, int qubit_label);

    TernaryNode* get_root() const { return _root.get(); }

    TernaryNode* get_node_by_qubit(int qubit_label) const { return _qubit_to_node.at(qubit_label); }
    TernaryNode* get_node_by_index(int node_index) const { return _index_to_node.at(node_index); }

    const std::vector<TernaryLeg*>& get_legs() const { return _legs; }
    TernaryLeg* get_leg(size_t i) const { return _legs.at(i); }
    // bool is_leg(const TernaryNode* node) { return node->is_leg();}

private:
    std::unique_ptr<TernaryNode> _root;
    std::unordered_map<int, TernaryNode*> _index_to_node;
    std::unordered_map<int, TernaryNode*> _qubit_to_node;
    std::vector<TernaryLeg*> _legs;
};

}  // namespace qsyn::hamiltonian
