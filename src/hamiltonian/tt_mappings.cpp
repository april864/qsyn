/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Ternary tree mappings ]
  Author       [ April Wang (april864) ]
*/

#include <memory>
#include <vector>
#include <unordered_map>

#include "tt_mappings.hpp"
#include "qubit_hamiltonian.hpp"
#include "ternary_tree.hpp"

namespace qsyn::hamiltonian {

std::unordered_map<BranchType, std::string> simple_branch_assignment = {
  {BranchType::LEFT,  "X"},
  {BranchType::MID,   "Y"},
  {BranchType::RIGHT, "Z"}
};

/**
 * @brief Basic method of getting fermionic operators of each mode.
 *        Return the fermionic operators of a given mode j by walking down the tree
 *        beginning at qubit j and obtaining the corresponding Majorana strings.
 */
void TTMapping::_basic_load_ferm_ops() {
    for (int i = 0; i < _tree.num_qubits; i++) {
        int mode = i;

        TernaryNode* node = _tree.get_node_by_qubit(mode);
        std::string left_branch, right_branch;

        TernaryNode* curr = node->left.get();
        left_branch += simple_branch_assignment.at(curr->branch) + std::to_string(node->qubit_label);   
        while (curr != nullptr) {
            left_branch += simple_branch_assignment.at(curr->branch) + std::to_string(curr->qubit_label);
            curr = curr->right.get();
        }

        curr = node->mid.get();
        right_branch += simple_branch_assignment.at(curr->branch) + std::to_string(node->qubit_label);
        while (curr != nullptr) {
            right_branch += simple_branch_assignment.at(curr->branch) + std::to_string(curr->qubit_label);
            curr = curr->right.get();
        }

        std::string ferm_raising_op = "1/2(" + left_branch + " - i" + right_branch + ")";
        std::string ferm_lowering_op = "1/2(" + left_branch + " + i" + right_branch + ")";

        _mode_to_ferm_ops[mode] = std::pair(ferm_raising_op, ferm_lowering_op);
    }

   return;
}

} // namespace qsyn::hamiltonian