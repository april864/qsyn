/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Implement conversion from QubitHamiltonian to Tensor ]
  Author       [ Design Verification Lab ]
****************************************************************************/

#include "./qbham_to_tensor.hpp"

#include <spdlog/spdlog.h>

#include <complex>
#include <limits>

#include "tableau/pauli_product_trait.hpp"

namespace qsyn {

/**
 * Build the full (2^n x 2^n) matrix representation of a QubitHamiltonian in the
 * computational basis.
 *
 * Mathematically, if H = sum_i c_i P_i where each P_i is a tensor product of single-qubit
 * Paulis, this routine fills a dense matrix M such that
 *
 *     M[row, col] = <row | H | col>.
 *
 * Implementation sketch:
 *   - Interpret `col` as the bitstring of a basis state |col>.
 *   - For each Pauli term c * P:
 *       * Walk through all qubits q and update:
 *           - `row`  = which basis state |row> = P |col> lands on (via bit flips).
 *           - `phase` = complex phase accumulated from the local Pauli action:
 *               Z: possibly flip sign depending on the input bit
 *               X: flip the corresponding bit in `row`
 *               Y: flip bit in `row` and multiply by ±i depending on the input bit.
 *       * Accumulate c * phase into M[row, col].
 */
std::optional<tensor::QTensor<double>> to_tensor(hamiltonian::QubitHamiltonian const& hamilt) {
    using qsyn::tableau::Pauli;

    // Deduce number of qubits from any term (all terms share the same size).
    std::size_t n_qubits = 0;
    if (hamilt.begin() != hamilt.end()) {
        n_qubits = hamilt.begin()->n_qubits();
    }

    if (n_qubits >= std::numeric_limits<std::size_t>::digits) {
        spdlog::error("Too many qubits ({}) to form a dense matrix representation.", n_qubits);
        return std::nullopt;
    }

    // Hilbert space dimension: 2^n.
    std::size_t const dim = std::size_t{1} << n_qubits;
    tensor::QTensor<double> mat(tensor::TensorShape{dim, dim});

    // Loop over all Pauli terms in the Hamiltonian.
    for (auto const& term : hamilt) {
        double const coeff = term.coeff();
        if (coeff == 0.0) continue;

        auto const& pauli_product = term.pauli_product();

        // Calculate P_i |col>.
        for (std::size_t col = 0; col < dim; ++col) {
            // Start from |row> = |col>; Pauli letters may flip individual bits.
            std::size_t row = col;
            // Global complex phase accumulated from this Pauli product on |col>.
            std::complex<double> phase{1.0, 0.0};

            // Walk through each qubit and apply the corresponding single-qubit Pauli.
            for (std::size_t q = 0; q < n_qubits; ++q) {
                auto const p   = pauli_product.get_pauli_type(q);
                auto const bit = (col >> q) & 1U;  // value of qubit q in |col>

                switch (p) {
                    case Pauli::i:
                        break;
                    case Pauli::z:
                        if (bit) {
                            phase = -phase;
                        }
                        break;
                    case Pauli::x:
                        // X flips the q-th bit: |b> -> |b xor 1>.
                        row ^= (std::size_t{1} << q);
                        break;
                    case Pauli::y:
                        // Y also flips the bit, but adds a phase:
                        //   Y|0> =  i|1>,  Y|1> = -i|0>
                        row ^= (std::size_t{1} << q);
                        phase *= (bit ? std::complex<double>(0.0, -1.0)
                                      : std::complex<double>(0.0, 1.0));
                        break;
                }
            }

            // This term contributes <row| c * P |col> = c * phase to matrix entry (row, col).
            mat(row, col) += coeff * phase;
        }
    }

    return mat;
}

}  // namespace qsyn
