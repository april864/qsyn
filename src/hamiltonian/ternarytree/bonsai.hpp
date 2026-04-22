/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Implement the Bonsai mapping for fermionic Hamiltonian to qubit Hamiltonian ]
  Author       [ Mu-Te (Joshua) Lau (joshmtlau) ]
*/

#pragma once

#include <limits>
#include <tl/expected.hpp>

#include "device/device.hpp"
#include "device/device_analysis.hpp"
#include "ternary_tree.hpp"

namespace qsyn::hamiltonian {

enum class BonsaiFailReason : uint8_t {
    not_enough_qubits,
    invalid_root_qubit,
};

tl::expected<TernaryTree, BonsaiFailReason>
build_bonsai_ternary_tree(
    std::size_t root_qubit_id,
    device::Device const& device,
    device::APSPResult const& apsp,
    size_t n_qubits = std::numeric_limits<size_t>::max());

tl::expected<TernaryTree, BonsaiFailReason>
build_bonsai_ternary_tree(
    device::Device const& device,
    device::APSPResult const& apsp,
    size_t n_qubits = std::numeric_limits<size_t>::max());

tl::expected<TernaryTree, BonsaiFailReason>
build_bonsai_ternary_tree_exhaustive(
    device::Device const& device,
    device::APSPResult const& apsp,
    size_t n_qubits = std::numeric_limits<size_t>::max());

}  // namespace qsyn::hamiltonian
