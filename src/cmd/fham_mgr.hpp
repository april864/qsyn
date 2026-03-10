/****************************************************************************
  PackageName  [ hamiltonian ]
  Synopsis     [ Define class FermionHamiltonian manager structure ]
  Author       [ Design Verification Lab ]
****************************************************************************/

#pragma once

#include "hamiltonian/fermionic_hamiltonian.hpp"
#include "util/data_structure_manager.hpp"

namespace qsyn::hamiltonian {

using FermionHamiltonianMgr = dvlab::utils::DataStructureManager<FermionHamiltonian>;

}  // namespace qsyn::hamiltonian

template <>
inline std::string dvlab::utils::data_structure_info_string(
    dvlab::utils::DataStructureManager<qsyn::hamiltonian::FermionHamiltonian> const& mgr, size_t id) {
    return fmt::format(
        "{:<19} {}",
        mgr.get_filename(id).substr(0, 19),
        fmt::join(mgr.get_procedures(id), " ➔ "));
}

template <>
inline std::string dvlab::utils::data_structure_name(
    dvlab::utils::DataStructureManager<qsyn::hamiltonian::FermionHamiltonian> const& mgr, size_t id) {
    return mgr.get_filename(id);
}

