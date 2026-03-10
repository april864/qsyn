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

namespace qsyn::hamiltonian {

enum class TreespileFailReason : uint8_t {
    empty_hamiltonian,
    device_too_small,
    invalid_n_trotterization_steps,
    tt_build_failed_not_enough_qubits,
};

tl::expected<qcir::QCir, TreespileFailReason>
treespile(
    FermionHamiltonian const& hamiltonian,
    device::Device const& device,
    size_t n_trotterization_steps,
    device::APSPCostFnType const& cost_fn = device::default_floyd_warshall_cost);

}  // namespace qsyn::hamiltonian
