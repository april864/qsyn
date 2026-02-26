/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Ternary tree mappings ]
  Author       [ April Wang (april864) ]
*/

#pragma once

#include <memory>
#include <vector>
#include <unordered_map>

#include "qubit_hamiltonian.hpp"
#include "ternary_tree.hpp"

namespace qsyn::hamiltonian {
  
class TTMapping {
public:
  TTMapping(TernaryTree& tree)
    : _tree(tree) { _basic_load_ferm_ops(); } 
  
  std::string get_ferm_raising_op(int mode) const {
    return _mode_to_ferm_ops.at(mode).first;
  }
  std::string get_ferm_lowering_op(int mode) const {
    return _mode_to_ferm_ops.at(mode).second;
  }
  
private:
  TernaryTree& _tree;
  std::unordered_map<int, std::pair<std::string, std::string>> _mode_to_ferm_ops;

  void _basic_load_ferm_ops();
};


} // namespace qsyn::hamiltonian