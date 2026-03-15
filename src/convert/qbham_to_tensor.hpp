/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Define conversion from QubitHamiltonian to Tensor ]
  Author       [ Design Verification Lab ]
****************************************************************************/

#pragma once

#include <optional>

#include "hamiltonian/qubit_hamiltonian.hpp"
#include "tensor/qtensor.hpp"

namespace qsyn {

namespace hamiltonian {
class QubitHamiltonian;
}

std::optional<tensor::QTensor<double>> to_tensor(hamiltonian::QubitHamiltonian const& hamilt);

}  // namespace qsyn
