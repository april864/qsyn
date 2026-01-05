/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define pauli rotation class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "pauli_rotation.hpp"

#include <ranges>
#include <tl/adjacent.hpp>
#include <tl/to.hpp>

#include "util/boolean_matrix.hpp"

namespace qsyn {

namespace tableau {

PauliRotation::PauliRotation(std::initializer_list<Pauli> const& pauli_list, dvlab::Phase const& phase)
    : _pauli_product(pauli_list, false), _phase(phase) { _normalize(); }

PauliRotation::PauliRotation(std::string_view pauli_str, dvlab::Phase const& phase)
    : _pauli_product(pauli_str), _phase(phase) { _normalize(); }

PauliRotation::PauliRotation(PauliProduct const& pauli_product, dvlab::Phase const& phase) : _pauli_product(pauli_product), _phase(phase) {
    _normalize();
}

std::string PauliRotation::to_string(char signedness) const {
    return fmt::format("exp(i * {} * {})", _phase.get_print_string(), _pauli_product.to_string(signedness));
}

std::string PauliRotation::to_bit_string() const {
    return fmt::format("{} {}", _pauli_product.to_bit_string().substr(0, 2 * n_qubits() + 1), _phase.get_print_string());
}

PauliRotation& PauliRotation::h(size_t qubit) noexcept {
    _pauli_product.h(qubit);
    _normalize();
    return *this;
}

PauliRotation& PauliRotation::s(size_t qubit) noexcept {
    _pauli_product.s(qubit);
    _normalize();
    return *this;
}

PauliRotation& PauliRotation::cx(size_t control, size_t target) noexcept {
    _pauli_product.cx(control, target);
    _normalize();
    return *this;
}

std::pair<CliffordOperatorString, size_t> extract_clifford_operators(PauliRotation pauli_rotation) {
    using COT = CliffordOperatorType;
    std::vector<CliffordOperator> clifford_ops;
    for (size_t i = 0; i < pauli_rotation.n_qubits(); ++i) {
        if (pauli_rotation.get_pauli_type(i) == Pauli::x) {
            clifford_ops.emplace_back(COT::h, std::array<size_t, 2>{i});
        } else if (pauli_rotation.get_pauli_type(i) == Pauli::y) {
            clifford_ops.emplace_back(COT::v, std::array<size_t, 2>{i});
        }
    }
    auto const non_i_qubits = std::ranges::views::iota(0ul, pauli_rotation.n_qubits()) |
                              std::ranges::views::filter([&pauli_rotation](size_t i) {
                                  return pauli_rotation.get_pauli_type(i) != Pauli::i;
                              }) |
                              tl::to<std::vector>();

    for (auto const& [c, t] : tl::views::adjacent<2>(non_i_qubits)) {
        clifford_ops.emplace_back(COT::cx, std::array<size_t, 2>{c, t});
    }

    return {clifford_ops, non_i_qubits.back()};
}

size_t matrix_rank(std::vector<PauliRotation> const& rotations) {
    auto const n_qubits    = rotations.front().n_qubits();
    auto const n_rotations = rotations.size();

    // load the matrix
    // REVIEW - may be revised if we move to use dynamic_bitset for the BooleanMatrix too
    auto matrix = dvlab::BooleanMatrix{n_rotations, n_qubits};
    for (size_t i = 0; i < n_rotations; ++i) {
        for (size_t j = 0; j < n_qubits; ++j) {
            matrix[i][j] = rotations[i].pauli_product().is_z_set(j);
        }
    }

    return matrix.matrix_rank();
};

}  // namespace tableau

}  // namespace qsyn
