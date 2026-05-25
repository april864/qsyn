/*
  Unit tests for TernaryTree (ternary tree structure used in Hamiltonian handling).
*/

#include "hamiltonian/ternarytree/ternary_tree.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace qsyn::hamiltonian;

size_t num_legs(size_t num_qubits) {
    // root has 3 legs, adding each child removes one leg but adds three more
    // so total legs is 3 + 2 * (num_qubits - 1) = 1 + 2 * num_qubits
    return 1 + 2 * num_qubits;
}

TEST_CASE("TernaryTree constructor rejects invalid num_qubits", "[ternary_tree]") {
    REQUIRE_THROWS_AS(TernaryTree(0), std::invalid_argument);
}

TEST_CASE("TernaryTree single qubit (root only)", "[ternary_tree]") {
    TernaryTree tree(1);

    REQUIRE(tree.num_qubits() == 1);

    TernaryNode* root = tree.get_root();
    REQUIRE(root != nullptr);
    REQUIRE(root->parent == nullptr);
    auto const root_qubit_node = dynamic_cast<TernaryQubitNode*>(root);
    REQUIRE(root_qubit_node != nullptr);
    REQUIRE(root_qubit_node->qubit_label == std::nullopt);
    REQUIRE(!root->is_leg());

    // Root has three edges (to legs)
    REQUIRE(root->get_edge(BranchType::left) != nullptr);
    REQUIRE(root->get_edge(BranchType::mid) != nullptr);
    REQUIRE(root->get_edge(BranchType::right) != nullptr);

    // Three legs
    REQUIRE(tree.get_legs().size() == 3);
    for (size_t i = 0; i < 3; ++i) {
        TernaryLeg* leg = tree.get_leg(i);
        REQUIRE(leg != nullptr);
        REQUIRE(leg->is_leg());
        REQUIRE(leg->parent == root);
        REQUIRE(leg->incoming_edge != nullptr);
        REQUIRE(leg->incoming_edge->source == root);
    }

    REQUIRE(tree.get_node_by_index(0) == root);
}

TEST_CASE("TernaryTree four qubits (root + 3 children)", "[ternary_tree]") {
    TernaryTree tree(4);

    REQUIRE(tree.num_qubits() == 4);

    TernaryNode* root = tree.get_root();
    REQUIRE(root != nullptr);

    TernaryEdge* left_edge  = root->get_edge(BranchType::left);
    TernaryEdge* mid_edge   = root->get_edge(BranchType::mid);
    TernaryEdge* right_edge = root->get_edge(BranchType::right);
    REQUIRE(left_edge != nullptr);
    REQUIRE(mid_edge != nullptr);
    REQUIRE(right_edge != nullptr);

    REQUIRE(left_edge->source == root);
    REQUIRE(mid_edge->source == root);
    REQUIRE(right_edge->source == root);
    REQUIRE(left_edge->branch == BranchType::left);
    REQUIRE(mid_edge->branch == BranchType::mid);
    REQUIRE(right_edge->branch == BranchType::right);

    TernaryNode* left_child  = left_edge->target.get();
    TernaryNode* mid_child   = mid_edge->target.get();
    TernaryNode* right_child = right_edge->target.get();
    REQUIRE(left_child != nullptr);
    REQUIRE(mid_child != nullptr);
    REQUIRE(right_child != nullptr);

    REQUIRE(left_child->parent == root);
    REQUIRE(mid_child->parent == root);
    REQUIRE(right_child->parent == root);

    REQUIRE(left_child->incoming_edge == left_edge);
    REQUIRE(mid_child->incoming_edge == mid_edge);
    REQUIRE(right_child->incoming_edge == right_edge);

    REQUIRE(tree.get_node_by_index(0) == root);
    REQUIRE(tree.get_node_by_index(1) == left_child);
    REQUIRE(tree.get_node_by_index(2) == mid_child);
    REQUIRE(tree.get_node_by_index(3) == right_child);

    // root has 3 legs, adding each child removes one leg but adds three more
    // so total legs is 1 + 2 * 4 = 9
    REQUIRE(tree.get_legs().size() == num_legs(4));
}

TEST_CASE("TernaryTree assign_qubit and get_node_by_qubit", "[ternary_tree]") {
    TernaryTree tree(4);

    tree.assign_qubit(0, 10);
    tree.assign_qubit(1, 20);
    tree.assign_qubit(2, 30);
    tree.assign_qubit(3, 40);

    REQUIRE(tree.get_node_by_qubit(10) == tree.get_root());
    auto const qubit_10_node = dynamic_cast<TernaryQubitNode*>(tree.get_node_by_qubit(10));
    REQUIRE(qubit_10_node != nullptr);
    REQUIRE(qubit_10_node->qubit_label == 10);
    auto const qubit_20_node = dynamic_cast<TernaryQubitNode*>(tree.get_node_by_qubit(20));
    REQUIRE(qubit_20_node != nullptr);
    REQUIRE(qubit_20_node->qubit_label == 20);

    // Reassign
    tree.assign_qubit(1, 99);
    auto const qubit_99_node = dynamic_cast<TernaryQubitNode*>(tree.get_node_by_qubit(99));
    REQUIRE(qubit_99_node != nullptr);
    REQUIRE(qubit_99_node->qubit_label == 99);
}

TEST_CASE("TernaryTree legs are TernaryLeg and have correct parent", "[ternary_tree]") {
    TernaryTree tree(2);

    TernaryNode* root  = tree.get_root();
    TernaryNode* child = root->get_edge(BranchType::left)->target.get();

    // root has 3 legs, adding each child removes one leg but adds three more
    // so total legs is 1 + 2 * 2 = 5
    REQUIRE(tree.get_legs().size() == num_legs(2));

    for (TernaryLeg* leg : tree.get_legs()) {
        REQUIRE(leg->is_leg());
        REQUIRE(dynamic_cast<TernaryLeg*>(leg) == leg);
        REQUIRE(leg->parent != nullptr);
        REQUIRE((leg->parent == root || leg->parent == child));
        REQUIRE(leg->incoming_edge != nullptr);
        REQUIRE(leg->incoming_edge->source == leg->parent);
    }
}

TEST_CASE("TernaryTree larger tree structure", "[ternary_tree]") {
    // 10 qubits: root + 3 children, then each of those gets up to 3 children until we have 10 nodes
    TernaryTree tree(10);

    REQUIRE(tree.num_qubits() == 10);
    REQUIRE(tree.get_root() != nullptr);

    for (size_t i = 0; i < 10; ++i) {
        TernaryNode* node = tree.get_node_by_index(i);
        REQUIRE(node != nullptr);
        if (i > 0) {
            REQUIRE(node->parent != nullptr);
            REQUIRE(node->incoming_edge != nullptr);
            REQUIRE(node->incoming_edge->source == node->parent);
            REQUIRE(node->incoming_edge->target.get() == node);
        }
    }

    REQUIRE(tree.get_legs().size() == num_legs(10));
}

TEST_CASE("TernaryTree copy constructor deep copy", "[ternary_tree]") {
    TernaryTree original(4);
    original.assign_qubit(0, 10);
    original.assign_qubit(1, 20);
    original.assign_qubit(2, 30);
    original.assign_qubit(3, 40);

    TernaryTree copy(original);

    REQUIRE(copy.num_qubits() == original.num_qubits());
    REQUIRE(copy.num_qubits() == 4);

    REQUIRE(copy.get_root() != nullptr);
    REQUIRE(copy.get_root() != original.get_root());

    for (size_t i = 0; i < 4; ++i) {
        TernaryNode* orig_node = original.get_node_by_index(i);
        TernaryNode* copy_node = copy.get_node_by_index(i);
        REQUIRE(copy_node != nullptr);
        REQUIRE(copy_node != orig_node);
        auto const orig_qubit_node = dynamic_cast<TernaryQubitNode*>(orig_node);
        auto const copy_qubit_node = dynamic_cast<TernaryQubitNode*>(copy_node);
        REQUIRE(copy_qubit_node != nullptr);
        REQUIRE(copy_qubit_node != orig_qubit_node);
        REQUIRE(copy_qubit_node->qubit_label == orig_qubit_node->qubit_label);
    }

    REQUIRE(copy.get_node_by_qubit(10) == copy.get_root());
    {
        auto const qubit_10_node = dynamic_cast<TernaryQubitNode*>(copy.get_node_by_qubit(10));
        REQUIRE(qubit_10_node != nullptr);
        REQUIRE(qubit_10_node->qubit_label == 10);
        auto const qubit_20_node = dynamic_cast<TernaryQubitNode*>(copy.get_node_by_qubit(20));
        REQUIRE(qubit_20_node != nullptr);
        REQUIRE(qubit_20_node->qubit_label == 20);
        auto const qubit_30_node = dynamic_cast<TernaryQubitNode*>(copy.get_node_by_qubit(30));
        REQUIRE(qubit_30_node != nullptr);
        REQUIRE(qubit_30_node->qubit_label == 30);
        auto const qubit_40_node = dynamic_cast<TernaryQubitNode*>(copy.get_node_by_qubit(40));
        REQUIRE(qubit_40_node != nullptr);
        REQUIRE(qubit_40_node->qubit_label == 40);
    }

    REQUIRE(copy.get_legs().size() == original.get_legs().size());
    REQUIRE(copy.get_legs().size() == num_legs(4));

    // Mutating the copy does not affect the original
    copy.assign_qubit(1, 99);
    {
        auto const qubit_20_node = dynamic_cast<TernaryQubitNode*>(original.get_node_by_qubit(20));
        REQUIRE(qubit_20_node != nullptr);
        REQUIRE(qubit_20_node->qubit_label == 20);
        auto const qubit_id_1_node = dynamic_cast<TernaryQubitNode*>(original.get_node_by_index(1));
        REQUIRE(qubit_id_1_node != nullptr);
        REQUIRE(qubit_id_1_node->qubit_label == 20);
        auto const qubit_99_node = dynamic_cast<TernaryQubitNode*>(copy.get_node_by_qubit(99));
        REQUIRE(qubit_99_node != nullptr);
        REQUIRE(qubit_99_node->qubit_label == 99);
    }
}

TEST_CASE("TernaryTree copy constructor single qubit", "[ternary_tree]") {
    TernaryTree original(1);
    TernaryTree copy(original);

    REQUIRE(copy.num_qubits() == 1);
    REQUIRE(copy.get_root() != nullptr);
    REQUIRE(copy.get_root() != original.get_root());
    REQUIRE(copy.get_legs().size() == 3);
}

TEST_CASE("TernaryTree move constructor", "[ternary_tree]") {
    TernaryTree original(4);
    original.assign_qubit(0, 5);
    original.assign_qubit(1, 6);
    TernaryNode* orig_root = original.get_root();
    size_t orig_num_qubits = original.num_qubits();
    size_t orig_legs_size  = original.get_legs().size();

    TernaryTree moved(std::move(original));

    REQUIRE(moved.num_qubits() == orig_num_qubits);
    REQUIRE(moved.num_qubits() == 4);
    REQUIRE(moved.get_root() == orig_root);
    REQUIRE(moved.get_node_by_index(0) == orig_root);
    auto const qubit_5_node = dynamic_cast<TernaryQubitNode*>(moved.get_node_by_qubit(5));
    REQUIRE(qubit_5_node != nullptr);
    REQUIRE(qubit_5_node->qubit_label == 5);
    auto const qubit_6_node = dynamic_cast<TernaryQubitNode*>(moved.get_node_by_qubit(6));
    REQUIRE(qubit_6_node != nullptr);
    REQUIRE(qubit_6_node->qubit_label == 6);
    REQUIRE(moved.get_legs().size() == orig_legs_size);
    REQUIRE(moved.get_legs().size() == num_legs(4));

    // Moved-from tree is in a valid state (can be destroyed or reassigned)
    REQUIRE(original.num_qubits() == 4);
    REQUIRE(original.get_root() == nullptr);
}

TEST_CASE("TernaryTree remap_node_ids permutes index slots", "[ternary_tree]") {
    TernaryTree tree;
    auto* root = tree.get_root();
    tree.add_qubit_node(root, BranchType::left);

    tree.remap_node_ids(std::array<size_t, 2>{1, 0});

    REQUIRE(dynamic_cast<TernaryQubitNode*>(tree.get_root())->id == 1);
    REQUIRE(dynamic_cast<TernaryQubitNode*>(tree.get_node_by_index(0))->id == 0);
    REQUIRE(dynamic_cast<TernaryQubitNode*>(tree.get_node_by_index(1))->id == 1);
    REQUIRE(tree.get_node_by_index(1) == tree.get_root());
}

TEST_CASE("TernaryTree move then use moved-to", "[ternary_tree]") {
    TernaryTree a(3);
    a.assign_qubit(0, 0);
    a.assign_qubit(1, 1);
    a.assign_qubit(2, 2);

    TernaryTree b(std::move(a));

    REQUIRE(b.get_node_by_index(0) != nullptr);
    auto const qubit_0_node = dynamic_cast<TernaryQubitNode*>(b.get_node_by_index(0));
    REQUIRE(qubit_0_node != nullptr);
    REQUIRE(qubit_0_node->qubit_label == 0);
    auto const qubit_1_node = dynamic_cast<TernaryQubitNode*>(b.get_node_by_index(1));
    REQUIRE(qubit_1_node != nullptr);
    REQUIRE(qubit_1_node->qubit_label == 1);
    auto const qubit_2_node = dynamic_cast<TernaryQubitNode*>(b.get_node_by_index(2));
    REQUIRE(qubit_2_node != nullptr);
    REQUIRE(qubit_2_node->qubit_label == 2);
    REQUIRE(b.get_leg(0) != nullptr);
    REQUIRE(b.get_leg(0)->is_leg());
}
