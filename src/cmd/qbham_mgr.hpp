/****************************************************************************
  PackageName  [ hamiltonian ]
  Synopsis     [ Define class QubitHamiltonian manager structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "hamiltonian/qubit_hamiltonian.hpp"
#include "util/data_structure_manager.hpp"

namespace qsyn::hamiltonian {

using QubitHamiltonianMgr = dvlab::utils::DataStructureManager<QubitHamiltonian>;

}  // namespace qsyn::hamiltonian

template <>
inline std::string dvlab::utils::data_structure_info_string(
    dvlab::utils::DataStructureManager<qsyn::hamiltonian::QubitHamiltonian> const& mgr, size_t id) {
    return fmt::format("{:<19} {}", mgr.get_filename(id).substr(0, 19),
                       fmt::join(mgr.get_procedures(id), " ➔ "));
}

template <>
inline std::string dvlab::utils::data_structure_name(
    dvlab::utils::DataStructureManager<qsyn::hamiltonian::QubitHamiltonian> const& mgr, size_t id) {
    return mgr.get_filename(id);
}
