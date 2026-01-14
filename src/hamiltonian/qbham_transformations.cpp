/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Implement transformations on QubitHamiltonians ]
  Author       [ Mu-Te Lau (joshmtlau) ]
*/

#include "./qbham_transformations.hpp"

#include <algorithm>
#include <ranges>

#include "./qubit_hamiltonian.hpp"

namespace qsyn::hamiltonian {

using PauliTermsSorter = std::function<void(QubitHamiltonian& hamilt)>;

/**
 * @brief Sort the Pauli terms of a QubitHamiltonian lexicographically.
 *        For example, the Pauli terms "XZ" will be sorted before "YZ" if
 *        'Pauli::x' is before 'Pauli::y' in the `order` span.
 *
 * @param hamilt The QubitHamiltonian to sort.
 * @param order The order of the Pauli terms. Must contain all the Pauli types
 *              in the QubitHamiltonian exactly once.
 */
void lexicographic_sort(
    QubitHamiltonian& hamilt,
    std::span<qsyn::tableau::Pauli const, 4> order) {
    using QbHamTerm = QubitHamiltonianTerm;
    using Pauli     = qsyn::tableau::Pauli;

    constexpr std::array<Pauli, 4>
        all_pauli_types = {Pauli::i, Pauli::x, Pauli::y, Pauli::z};

    DVLAB_ASSERT(
        std::ranges::all_of(order, [&](Pauli p) {
            return dvlab::contains(all_pauli_types, p);
        }),
        "`order` must contain each of the Pauli type exactly once");

    auto const compare_pauli_terms =
        [&](QbHamTerm const& a, QbHamTerm const& b) {
            for (size_t i = 0; i < a.n_qubits(); ++i) {
                if (a.get_pauli_type(i) != b.get_pauli_type(i)) {
                    return std::ranges::find(order, a.get_pauli_type(i)) <
                           std::ranges::find(order, b.get_pauli_type(i));
                }
            }
            return false;
        };

    std::ranges::sort(hamilt, compare_pauli_terms);
}

/**
 * @brief Sort the Pauli terms of a QubitHamiltonian lexicographically.
 *        For example, the Pauli terms "XZ" will be sorted before "YZ" if
 *        'x' is before 'y' in the `order_str` string_view.
 *
 * @param hamilt The QubitHamiltonian to sort.
 * @param order_str The order of the Pauli terms. Case-sensitive. Must contains
 *                  all the characters in "xyzi" exactly once.
 */
void lexicographic_sort(
    QubitHamiltonian& hamilt,
    std::string_view order_str) {
    DVLAB_ASSERT(
        is_valid_pauli_letter_order(order_str),
        "`order_str` must contain all characters in 'xyzi' exactly once");

    std::array<qsyn::tableau::Pauli, 4> order_array{};
    std::ranges::copy(
        order_str |
            std::views::transform([](char c) -> qsyn::tableau::Pauli {
                switch (c) {
                    case 'x':
                        return qsyn::tableau::Pauli::x;
                    case 'y':
                        return qsyn::tableau::Pauli::y;
                    case 'z':
                        return qsyn::tableau::Pauli::z;
                    case 'i':
                        return qsyn::tableau::Pauli::i;
                    default:
                        DVLAB_UNREACHABLE("Invalid Pauli letter");
                        return qsyn::tableau::Pauli::i;
                }
            }),
        order_array.begin());

    lexicographic_sort(hamilt, std::span{order_array});
}

bool is_valid_pauli_letter_order(std::string_view order_str) {
    return order_str.size() == 4 &&
           std::ranges::all_of(order_str, [](char c) {
               using namespace std::literals;
               return "xyzi"sv.find(c) != std::string_view::npos;
           });
}

void lexicographic_sort(QubitHamiltonian& hamilt) {
    lexicographic_sort(hamilt, "xyzi");
}

}  // namespace qsyn::hamiltonian
