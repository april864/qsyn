/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Ternary tree rotations ]
  Author       [ April Wang (april864)]
*/

#include "tree_rotations.hpp"

#include <algorithm>
#include <random>
#include <vector>

#include "./ternary_tree.hpp"
#include "hamiltonian/f2q_mappings.hpp"
#include "hamiltonian/fermionic_hamiltonian.hpp"
#include "hamiltonian/qubit_hamiltonian.hpp"

namespace qsyn::hamiltonian {

// For non-connectivity preserving (NCP), choose a random terminal node-v with
// three legs and attach it to the free leg of another node-w that is neither
// v nor its parent. For CP random leaf move, choose random move that preserves
// connectivity. For CP leaf move, choose moves more likely to improve error.
void TreeRotator::ncp_leaf_move(TernaryTree* tt) {
    if (!tt || tt->num_qubits() < 2) return;

    // Find nodes with three legs
    std::vector<TernaryNode*> candidate_nodes;
    for (size_t i = 0; i < tt->num_qubits(); ++i) {
        TernaryNode* node = tt->get_node_by_index(i);
        if (node == tt->get_root()) continue;

        TernaryEdge* left_edge  = node->get_left();
        TernaryEdge* mid_edge   = node->get_mid();
        TernaryEdge* right_edge = node->get_right();

        if (left_edge && left_edge->target && left_edge->target->is_leg() &&
            mid_edge && mid_edge->target && mid_edge->target->is_leg() &&
            right_edge && right_edge->target && right_edge->target->is_leg()) {
            candidate_nodes.push_back(node);
        }
    }
    if (candidate_nodes.empty()) return;

    // Choose random node
    std::uniform_int_distribution<size_t> dist_v(0, candidate_nodes.size() - 1);
    TernaryNode* v = candidate_nodes[dist_v(_rng)];

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
    TernaryLeg* l = candidate_legs[dist_leg(_rng)];

    // Do the swap
    TernaryEdge* edge_v = v->incoming_edge;
    TernaryEdge* edge_l = l->incoming_edge;
    edge_v->target.swap(edge_l->target);

    v->parent        = edge_l->source;
    v->incoming_edge = edge_l;

    l->parent        = edge_v->source;
    l->incoming_edge = edge_v;
}

void TreeRotator::cp_random_leaf_move(TernaryTree* tt, qsyn::device::Device const& device) {
    if (!tt || tt->num_qubits() < 2) return;

    struct CandidatePair {
        TernaryNode* v;
        TernaryLeg* leg;
    };
    std::vector<CandidatePair> candidates;

    // Find all valid node and leg combinations
    for (size_t i = 0; i < tt->num_qubits(); ++i) {
        TernaryNode* v = tt->get_node_by_index(i);
        if (v == tt->get_root()) continue;

        // Find v
        auto* e_left  = v->get_left();
        auto* e_mid   = v->get_mid();
        auto* e_right = v->get_right();

        if (e_left && e_left->target && e_left->target->is_leg() &&
            e_mid && e_mid->target && e_mid->target->is_leg() &&
            e_right && e_right->target && e_right->target->is_leg()) {
            auto* v_qubit = static_cast<TernaryQubitNode*>(v);
            if (!v_qubit->qubit_label.has_value()) continue;
            auto v_qindex = v_qubit->qubit_label.value();

            // Find w
            for (TernaryLeg* leg : tt->get_legs()) {
                TernaryNode* w = leg->parent;

                if (w != v && w != v->parent) {
                    auto* w_qubit = static_cast<TernaryQubitNode*>(w);
                    if (!w_qubit->qubit_label.has_value()) continue;
                    auto w_qindex = w_qubit->qubit_label.value();

                    // Check if the hardware has the necessary connection
                    if (device.is_adjacent(v_qindex, w_qindex) ||
                        device.is_adjacent(w_qindex, v_qindex)) {
                        candidates.push_back({v, leg});
                    }
                }
            }
        }
    }
    if (candidates.empty()) return;

    // Choose a pair to apply leaf move to
    std::uniform_int_distribution<size_t> dist(0, candidates.size() - 1);

    CandidatePair move_pair = candidates[dist(_rng)];
    TernaryNode* v          = move_pair.v;
    TernaryLeg* l           = move_pair.leg;

    // Do the swap
    TernaryEdge* edge_v = v->incoming_edge;
    TernaryEdge* edge_l = l->incoming_edge;

    edge_v->target.swap(edge_l->target);

    v->parent        = edge_l->source;
    v->incoming_edge = edge_l;

    l->parent        = edge_v->source;
    l->incoming_edge = edge_v;
}

void TreeRotator::cp_leaf_move(TernaryTree* tt, qsyn::device::Device const& device) {
    if (!tt || tt->num_qubits() < 2) return;

    struct CandidatePair {
        TernaryNode* v;
        TernaryLeg* leg;
    };
    std::vector<CandidatePair> candidates;

    std::vector<double> weights;  // weight: error of old connection/error of new connection

    // Lambda func to get error rate of a connection
    auto get_error = [&device](size_t q1, size_t q2) -> float {
        if (device.is_adjacent(q1, q2)) return device.get_gate_info(qsyn::device::Device::QubitPair{q1, q2})[0].error;
        if (device.is_adjacent(q2, q1)) return device.get_gate_info(qsyn::device::Device::QubitPair{q2, q1})[0].error;
        return 1.0f;
    };

    // Find all valid node and leg combinations
    for (size_t i = 0; i < tt->num_qubits(); ++i) {
        TernaryNode* v = tt->get_node_by_index(i);
        if (v == tt->get_root()) continue;

        // Find v
        auto* e_left  = v->get_left();
        auto* e_mid   = v->get_mid();
        auto* e_right = v->get_right();

        if (e_left && e_left->target && e_left->target->is_leg() &&
            e_mid && e_mid->target && e_mid->target->is_leg() &&
            e_right && e_right->target && e_right->target->is_leg()) {
            auto* v_qubit = static_cast<TernaryQubitNode*>(v);
            if (!v_qubit->qubit_label.has_value()) continue;
            auto v_qindex = v_qubit->qubit_label.value();

            // Calculate error of v's current physical connection
            float old_error    = 1.0f;
            auto* parent_qubit = dynamic_cast<TernaryQubitNode*>(v->parent);
            if (parent_qubit && parent_qubit->qubit_label.has_value()) {
                old_error = get_error(v_qindex, parent_qubit->qubit_label.value());
            }

            // Find w
            for (TernaryLeg* leg : tt->get_legs()) {
                TernaryNode* w = leg->parent;

                if (w != v && w != v->parent) {
                    auto* w_qubit = static_cast<TernaryQubitNode*>(w);
                    if (!w_qubit->qubit_label.has_value()) continue;
                    auto w_qindex = w_qubit->qubit_label.value();

                    // Check if the hardware has the necessary connection
                    if (device.is_adjacent(v_qindex, w_qindex) || device.is_adjacent(w_qindex, v_qindex)) {
                        candidates.push_back({v, leg});

                        // Calculate weight of the move
                        float new_error = get_error(v_qindex, w_qindex);
                        double weight   = static_cast<double>(old_error) / (static_cast<double>(new_error) + 1e-6);  // Added 1e-6 to prevent division by 0 error
                        weights.push_back(weight);
                    }
                }
            }
        }
    }
    if (candidates.empty()) return;

    // Choose a pair to apply leaf move to
    std::discrete_distribution<size_t> dist(weights.begin(), weights.end());

    CandidatePair move_pair = candidates[dist(_rng)];
    TernaryEdge* edge_v     = move_pair.v->incoming_edge;
    TernaryEdge* edge_l     = move_pair.leg->incoming_edge;

    edge_v->target.swap(edge_l->target);
    move_pair.v->parent          = edge_l->source;
    move_pair.v->incoming_edge   = edge_l;
    move_pair.leg->parent        = edge_v->source;
    move_pair.leg->incoming_edge = edge_v;
}

// A node v different from the root with out-degree at most 2 is chosen
// as a new root. The path from root to v is identified, and the tree is
// updated so that child and parent designations are swapped along the path.
void TreeRotator::root_change(TernaryTree* tt) {
    if (!tt || tt->num_qubits() < 2) return;

    // Find candidate nodes for new root
    std::vector<TernaryNode*> candidate_roots;
    for (size_t i = 0; i < tt->num_qubits(); ++i) {
        TernaryNode* node = tt->get_node_by_index(i);
        if (node == tt->get_root()) continue;

        bool has_leg = false;
        for (auto b : {BranchType::left, BranchType::mid, BranchType::right}) {
            TernaryEdge* edge = node->get_edge(b);
            if (edge && edge->target && edge->target->is_leg()) {
                has_leg = true;
                break;
            }
        }
        if (has_leg) {
            candidate_roots.push_back(node);
        }
    }
    if (candidate_roots.empty()) return;

    // Pick random new root
    std::uniform_int_distribution<size_t> dist_root(0, candidate_roots.size() - 1);
    TernaryNode* new_root = candidate_roots[dist_root(_rng)];

    // Choose which leg will hold the new root's previous parent
    std::vector<TernaryEdge*> candidate_edges;
    for (auto b : {BranchType::left, BranchType::mid, BranchType::right}) {
        TernaryEdge* edge = new_root->get_edge(b);
        if (edge && edge->target && edge->target->is_leg()) {
            candidate_edges.push_back(edge);
        }
    }
    std::uniform_int_distribution<size_t> dist_leg(0, candidate_edges.size() - 1);
    TernaryEdge* leg_edge = candidate_edges[dist_leg(_rng)];
    TernaryNode* leg      = leg_edge->target.get();

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
    if (k == 0) return;
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
    tt->get_root_ptr() = std::move(node_ptrs.back());                     // assigns new root
    leg_edge->target   = std::move(k > 1 ? node_ptrs[k - 2] : root_ptr);  // edge to previously chosen leg points to next node

    for (size_t i = k - 1; i >= 1; --i) {
        path_edges[i]->target = std::move(i == 1 ? root_ptr : node_ptrs[i - 2]);
    }
    path_edges[0]->target = std::move(leg_ptr);  // previous edge to new root points to leg

    // Update parent and incoming_edge pointers
    leg->parent        = path_nodes[0];
    leg->incoming_edge = path_edges[0];

    for (size_t i = 0; i < k; ++i) {
        path_nodes[i]->parent        = path_nodes[i + 1];
        path_nodes[i]->incoming_edge = (i == k - 1) ? leg_edge : path_edges[i + 1];
    }

    path_nodes[k]->parent        = nullptr;
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
    std::uniform_int_distribution<size_t> dist_v(0, candidate_nodes.size() - 1);
    TernaryNode* v = candidate_nodes[dist_v(_rng)];

    // Get unique_ptrs of the node's outgoing edges
    std::array<std::unique_ptr<TernaryEdge>, 3> edge_ptrs;
    edge_ptrs[0] = std::move(v->edges[static_cast<int>(BranchType::left)]);
    edge_ptrs[1] = std::move(v->edges[static_cast<int>(BranchType::mid)]);
    edge_ptrs[2] = std::move(v->edges[static_cast<int>(BranchType::right)]);

    // Shuffle edges
    std::ranges::shuffle(edge_ptrs, _rng);

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
    std::uniform_int_distribution<size_t> dist_i(0, tt->num_qubits() - 1);
    std::uniform_int_distribution<size_t> dist_i_prime(0, tt->num_qubits() - 2);

    size_t i       = dist_i(_rng);
    size_t i_prime = dist_i_prime(_rng);

    if (i_prime >= i) {
        i_prime++;
    }

    // swap braiding flags of the two nodes
    auto* qubit_i       = static_cast<TernaryQubitNode*>(tt->get_node_by_index(i));
    auto* qubit_i_prime = static_cast<TernaryQubitNode*>(tt->get_node_by_index(i_prime));
    std::swap(qubit_i->is_braided, qubit_i_prime->is_braided);

    // Swap
    tt->swap_indices(i, i_prime);
}

// For a node with label (i, u, b), the braiding b is changed to the
// opposite one, i.e., '+' is changed to '-' and vice versa.
void TreeRotator::majorana_braiding_change(TernaryTree* tt) {
    if (!tt || tt->num_qubits() == 0) return;

    // Choose node
    std::uniform_int_distribution<size_t> dist_v(0, tt->num_qubits() - 1);
    size_t index = dist_v(_rng);

    // Change braiding flag
    TernaryNode* node = tt->get_node_by_index(index);
    auto* qubit       = static_cast<TernaryQubitNode*>(node);
    qubit->is_braided = !qubit->is_braided;
}

}  // namespace qsyn::hamiltonian
