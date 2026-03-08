/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Implement the Bonsai mapping for fermionic Hamiltonian to qubit Hamiltonian ]
  Author       [ Mu-Te (Joshua) Lau (joshmtlau) ]
*/

#pragma once

#include <limits>

#include "device/device.hpp"
#include "device/device_analysis.hpp"
#include "ternary_tree.hpp"

namespace qsyn::hamiltonian {

std::optional<TernaryTree> build_bonsai_ternary_tree(std::size_t root_qubit_id, device::Device const& device, device::APSPResult const& apsp, size_t n_qubits = std::numeric_limits<size_t>::max());

std::optional<TernaryTree> build_bonsai_ternary_tree(device::Device const& device, device::APSPResult const& apsp, size_t n_qubits = std::numeric_limits<size_t>::max());

}  // namespace qsyn::hamiltonian
