/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Ternary tree rotations ]
  Author       [ April Wang (april864)]
*/

#include <random>
#include <algorithm>
#include <vector>

#include "tree_rotations.hpp"

namespace qsyn::hamiltonian {

// For non-connectivity preserving (NCP), choose a random terminal node-v with 
// three legs and attach it to the free leg of another node-w that is neither 
// v nor its parent.
void TreeRotator::ncp_leaf_move(TernaryTree* tt) {
    if (!tt || tt->num_qubits() < 2) return;

    // Find nodes with three legs
    std::vector<TernaryNode*> candidate_nodes;
    for (size_t i = 1; i < tt->num_qubits(); ++i) {
        TernaryNode* node = tt->get_node_by_index(i);
        TernaryEdge* left_edge = node->get_left();
        TernaryEdge* mid_edge = node->get_mid();
        TernaryEdge* right_edge = node->get_right();

        if (left_edge->target->is_leg() &&
            mid_edge->target->is_leg() &&
            right_edge->target->is_leg()) {
            candidate_nodes.push_back(node);
        }
    }
    if (candidate_nodes.empty()) return;

    // Choose random node
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist_v(0, candidate_nodes.size() - 1);
    TernaryNode* v = candidate_nodes[dist_v(gen)];

    // Find free leg of another node that is neither v nor its parent
    std::vector<TernaryLeg*> candidate_legs;
    for (TernaryLeg* leg : tt->get_legs()) {
        TernaryNode* w = leg->parent;
        if (w != v && w != v->parent) {
            candidate_legs.push_back(leg);
        }
    }
    if (candidate_legs.empty()) return;

    // Choose random leg
    std::uniform_int_distribution<size_t> dist_leg(0, candidate_legs.size() - 1);
    TernaryLeg* l = candidate_legs[dist_leg(gen)];

    // Do the swap
    TernaryEdge* edge_v = v->incoming_edge;
    TernaryEdge* edge_l = l->incoming_edge;
    edge_v->target.swap(edge_l->target);

    v->parent = edge_l->source;
    v->incoming_edge = edge_l;

    l->parent = edge_v->source;
    l->incoming_edge = edge_v;
}

// A node v different from the root with out-degree at most 2 is chosen 
// as a new root. The path from root to v is identified, and the tree is 
// updated so that child and parent designations are swapped along the path. 
void TreeRotator::root_change(TernaryTree* tt) {
    if (!tt || tt->num_qubits() < 2) return;

    // Find candidate nodes for new root
    std::vector<TernaryNode*> candidate_roots;
    for (size_t i = 1; i < tt->num_qubits(); ++i) {
        TernaryNode* node = tt->get_node_by_index(i);
        
        bool has_leg = false;
        for (auto b : {BranchType::left, BranchType::mid, BranchType::right}) {
            TernaryEdge* edge = node->get_edge(b);
            if (edge->target->is_leg()) {
                has_leg = true;
                break;
            }
        }
        if (has_leg) { candidate_roots.push_back(node); }
    }
    if (candidate_roots.empty()) return;

    // Pick random new root
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist_root(0, candidate_roots.size() - 1);
    TernaryNode* new_root = candidate_roots[dist_root(gen)];

    // Choose which leg will hold the new root's previous parent
    std::vector<TernaryEdge*> candidate_edges;
    for (auto b : {BranchType::left, BranchType::mid, BranchType::right}) {
        TernaryEdge* edge = new_root->get_edge(b);
        if (edge->target->is_leg()) {
            candidate_edges.push_back(edge);
        }
    }
    std::uniform_int_distribution<size_t> dist_leg(0, candidate_edges.size() - 1);
    TernaryEdge* leg_edge = candidate_edges[dist_leg(gen)];
    TernaryNode* leg = leg_edge->target.get();

    // Get path from old root to new root
    std::vector<TernaryNode*> path_nodes;
    TernaryNode* curr = new_root;
    while (curr != nullptr) {
        path_nodes.push_back(curr);
        curr = curr->parent;
    }
    std::reverse(path_nodes.begin(), path_nodes.end());
    // path_nodes: [n0, n1, ..., nk] where n0 is old root and nk is new root
    
    size_t k = path_nodes.size() - 1;
    std::vector<TernaryEdge*> path_edges;
    for (size_t i = 1; i <= k; ++i) {
        path_edges.push_back(path_nodes[i]->incoming_edge);
    }
    // path_edges: [e1, e2, ..., ek] where ei is the edge from n{i-1} to ni (edges from old root->new root)

    // Get all unique_ptrs involved in the swaps
    std::unique_ptr<TernaryNode> root_ptr = std::move(tt->get_root_ptr());
    std::vector<std::unique_ptr<TernaryNode>> node_ptrs;
    for (size_t i = 0; i < k; ++i) {
        node_ptrs.push_back(std::move(path_edges[i]->target));
    }
    std::unique_ptr<TernaryNode> leg_ptr = std::move(leg_edge->target);

    // Reverse edge targets
    tt->get_root_ptr() = std::move(node_ptrs.back()); // assigns new root
    leg_edge->target = std::move(k > 1 ? node_ptrs[k-2] : root_ptr); // edge to previously chosen leg points to next node
    
    for (size_t i = k - 1; i >= 1; --i) {
        path_edges[i]->target = std::move(i == 1 ? root_ptr : node_ptrs[i-2]);
    }
    path_edges[0]->target = std::move(leg_ptr); // previous edge to new root points to leg

    // Update parent and incoming_edge pointers
    leg->parent = path_nodes[0];
    leg->incoming_edge = path_edges[0];

    for (size_t i = 0; i < k; ++i) {
        path_nodes[i]->parent = path_nodes[i+1];
        path_nodes[i]->incoming_edge = (i == k - 1) ? leg_edge : path_edges[i+1];
    }

    path_nodes[k]->parent = nullptr;
    path_nodes[k]->incoming_edge = nullptr;
}

// A node with an out-degree of at least 1 is chosen, and the Pauli operators
// associated with the links are changed.
void TreeRotator::pauli_shuffle(TernaryTree* tt) {
    if (!tt || tt->num_qubits() == 0) return;

    // Get candidate nodes
    std::vector<TernaryNode*> candidate_nodes;
    for (size_t i = 0; i < tt->num_qubits(); ++i) {
        TernaryNode* node = tt->get_node_by_index(i);
        
        int out_degree = 0;
        for (auto b : {BranchType::left, BranchType::mid, BranchType::right}) {
            TernaryEdge* edge = node->get_edge(b);
            if (!edge->target->is_leg()) {
                out_degree++;
            }
        }
        if (out_degree >= 1) {
            candidate_nodes.push_back(node);
        }
    }
    if (candidate_nodes.empty()) return;

    // Choose random node
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist_v(0, candidate_nodes.size() - 1);
    TernaryNode* v = candidate_nodes[dist_v(gen)];

    // Get unique_ptrs of the node's outgoing edges
    std::array<std::unique_ptr<TernaryEdge>, 3> edge_ptrs;
    edge_ptrs[0] = std::move(v->edges[static_cast<int>(BranchType::left)]);
    edge_ptrs[1] = std::move(v->edges[static_cast<int>(BranchType::mid)]);
    edge_ptrs[2] = std::move(v->edges[static_cast<int>(BranchType::right)]);

    // Shuffle edges
    std::shuffle(edge_ptrs.begin(), edge_ptrs.end(), gen);

    // Reattach edges and update branch field
    int branch_idx = 0;
    for (auto& edge_ptr : edge_ptrs) {
        auto new_branch = static_cast<BranchType>(branch_idx);
        if (edge_ptr) {
            edge_ptr->branch = new_branch;
        }
        
        v->set_edge(new_branch, std::move(edge_ptr));
        branch_idx++;
    }
}

// For nodes with labels (i, u, b) and (i', u', b') the labels are changed to
// (i', u, b) and (i, u', b') respectively. 
void TreeRotator::mode_association_swap(TernaryTree* tt) {
    if (!tt || tt->num_qubits() < 2) return;

    // Choose two nodes
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, tt->num_qubits() - 1);

    size_t i = dist(gen);
    size_t i_prime = dist(gen);

    // don't pick same node twice
    while (i == i_prime) {
        i_prime = dist(gen);
    }

    // Swap
    tt->swap_indices(i, i_prime);
}

// For a node with label (i, u, b), the braiding b is changed to the
// opposite one, i.e., '+' is changed to '-' and vice versa.
void TreeRotator::majorana_braiding_change(TernaryTree* tt) {
    if (!tt || tt->num_qubits() == 0) return;

    // Choose node
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist_v(0, tt->num_qubits() - 1);
    size_t index = dist_v(gen);

    // Change braiding flag
    TernaryNode* node = tt->get_node_by_index(index);
    auto* qubit = static_cast<TernaryQubitNode*>(node);
    qubit->is_braided = !qubit->is_braided;
}

} // namespace