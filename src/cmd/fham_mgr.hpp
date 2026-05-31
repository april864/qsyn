/****************************************************************************
  PackageName  [ hamiltonian ]
  Synopsis     [ Define class FermionHamiltonian manager structure ]
  Author       [ Design Verification Lab ]
****************************************************************************/

#pragma once

#include "hamiltonian/fham_workspace.hpp"
#include "util/data_structure_manager.hpp"

namespace qsyn::hamiltonian {

using FermionHamiltonianMgr = dvlab::utils::DataStructureManager<FhamWorkspace>;

inline FhamWorkspace* fham_workspace(FermionHamiltonianMgr& mgr) { return mgr.get(); }

inline FhamWorkspace const* fham_workspace(FermionHamiltonianMgr const& mgr) {
    return mgr.get();
}

inline FermionHamiltonian const* fham_hamiltonian(FermionHamiltonianMgr const& mgr) {
    auto const* ws = fham_workspace(mgr);
    return ws != nullptr ? &ws->hamiltonian : nullptr;
}

}  // namespace qsyn::hamiltonian

template <>
inline std::string dvlab::utils::data_structure_info_string(
    dvlab::utils::DataStructureManager<qsyn::hamiltonian::FhamWorkspace> const& mgr, size_t id) {
    return fmt::format(
        "{:<19} {}",
        mgr.get_filename(id).substr(0, 19),
        fmt::join(mgr.get_procedures(id), " ➔ "));
}

template <>
inline std::string dvlab::utils::data_structure_name(
    dvlab::utils::DataStructureManager<qsyn::hamiltonian::FhamWorkspace> const& mgr, size_t id) {
    return mgr.get_filename(id);
}
