/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Implement the Treespilation mapping from fermionic Hamiltonian directly to mapped quantum circuits ]
  Author       [ Mu-Te (Joshua) Lau (joshmtlau) ]
*/

#pragma once

#include <tl/expected.hpp>

#include "device/device.hpp"
#include "device/device_analysis.hpp"
#include "hamiltonian/fermionic_hamiltonian.hpp"
#include "qcir/qcir.hpp"
#include "hamiltonian/tree_rotations.hpp"
#include "ternary_tree.hpp"

namespace qsyn::hamiltonian {

enum class TreespileFailReason : uint8_t {
    empty_hamiltonian,
    device_too_small,
    invalid_n_trotterization_steps,
    tt_build_failed_not_enough_qubits,
};

double pauli_weight_cost(const TernaryTree& tt, const FermionHamiltonian& f_ham);

TernaryTree optimize_mapping(
    TernaryTree initial_tree, 
    const FermionHamiltonian& f_ham, 
    const qsyn::device::Device* device = nullptr);

tl::expected<qcir::QCir, TreespileFailReason>
treespile(
    FermionHamiltonian const& hamiltonian,
    device::Device const& device,
    double time,
    size_t n_trotterization_steps,
    device::APSPCostFnType const& cost_fn = device::default_floyd_warshall_cost,
    bool optimize = false);


}  // namespace qsyn::hamiltonian
