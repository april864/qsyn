/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Optimizations for ternary tree represenation ]
  Author       [ April Wang (april864) ]
*/

#pragma once

#include "ternary_tree.hpp"
#include "hamiltonian/fermionic_hamiltonian.hpp"
#include "device/device.hpp"
#include "util/graph/floyd_warshall.hpp"

namespace qsyn::hamiltonian {

double pauli_weight_cost(const TernaryTree& tt, const FermionHamiltonian& f_ham);

// Oracle for computing tree distances and tail weights for fast_tree_cost.
class TreeOracle {
public:
    TreeOracle(const TernaryTree& tree, const dvlab::APSPResult<QubitIdType>& apsp);

    double get_tree_distance(size_t u, size_t v) const;
    double get_tail_weight(size_t u) const { return _tail_weight[u]; }
    double get_subtree_weight(const std::vector<size_t>& nodes) const;

private:
    size_t _n_modes;
    std::vector<size_t> _parent; // _parent[v] gives node v's parent
    std::vector<int> _depth; // _depth[v] gives distance from node v to root
    std::vector<double> _edge_cost_up; // error of edge from v to its parent
    std::vector<int> _dfs_start; // when v was reached in dfs (ie which timestep)
    std::vector<double> _tail_weight; // [(dist from v -> S_x^v) + (dist from v -> S_y^v)]/2

    void dfs(const TernaryTree& tree, const dvlab::APSPResult<QubitIdType>& apsp, int& timer,
      size_t v, size_t p, int current_depth, double cost_from_parent);

    double calc_branch_tail(const dvlab::APSPResult<QubitIdType>& apsp, TernaryQubitNode* start_node, 
      BranchType initial_branch);
};

// Approximates CNOT count for qubit interactions in f_ham
double fast_tree_cost(const TernaryTree& tree, const FermionHamiltonian& f_ham, const dvlab::APSPResult<QubitIdType>& apsp);
// Calculates proxy infidelity by multiplying infidelities of each gate in circuit
double infidelity_cost(const TernaryTree& tree, const FermionHamiltonian& f_ham, const dvlab::APSPResult<QubitIdType>& apsp);

TernaryTree infidelity_proxy_optimize_mapping(
    TernaryTree const& initial_tree,
    const FermionHamiltonian& f_ham,
    const qsyn::device::Device* device);

TernaryTree pauli_weight_optimize_mapping(
    TernaryTree const& initial_tree,
    const FermionHamiltonian& f_ham,
    const qsyn::device::Device* device = nullptr);
  
TernaryTree cnot_proxy_optimize_mapping(
    TernaryTree const& initial_tree,
    const FermionHamiltonian& f_ham,
    const qsyn::device::Device* device = nullptr);

}