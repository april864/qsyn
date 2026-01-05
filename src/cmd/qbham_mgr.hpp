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
inline std::string dvlab::utils::data_structure_info_string(qsyn::hamiltonian::QubitHamiltonian const& t) {
    return fmt::format("{:<19} {}", t.get_filename().substr(0, 19),
                       fmt::join(t.get_procedures(), " ➔ "));
}

template <>
inline std::string dvlab::utils::data_structure_name(qsyn::hamiltonian::QubitHamiltonian const& t) {
    return t.get_filename();
}
