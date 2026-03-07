/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Define transformations on QubitHamiltonians ]
  Author       [ Mu-Te Lau (joshmtlau) ]
*/

#pragma once

#include <span>
#include <string_view>

#include "./qubit_hamiltonian.hpp"

namespace qsyn::hamiltonian {

using PauliTermsSorter = std::function<void(QubitHamiltonian& hamilt)>;

/*
 * Lexicographic sort
 */
void lexicographic_sort(
    QubitHamiltonian& hamilt,
    std::span<qsyn::tableau::Pauli const, 4> order);

void lexicographic_sort(
    QubitHamiltonian& hamilt,
    std::string_view order_str);

bool is_valid_pauli_letter_order(std::string_view order_str);

void lexicographic_sort(QubitHamiltonian& hamilt);

/*
 * Magnitude sort
 */
void magnitude_sort(QubitHamiltonian& hamilt);

}  // namespace qsyn::hamiltonian
