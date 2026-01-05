/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Define qubit Hamiltonian term class ]
  Author       [ Mu-Te Lau (joshmtlau) ]
*/

#include "qubit_hamiltonian.hpp"

#include <fmt/format.h>

namespace qsyn {

namespace hamiltonian {

QubitHamiltonianTerm::QubitHamiltonianTerm(
    std::initializer_list<Pauli> const& pauli_list, double coeff)
    : _pauli_product(pauli_list, false), _coeff(coeff) {
    _normalize();
}

QubitHamiltonianTerm::QubitHamiltonianTerm(
    std::string_view pauli_str, double coeff)
    : _pauli_product(pauli_str), _coeff(coeff) {
    _normalize();
}

QubitHamiltonianTerm::QubitHamiltonianTerm(
    PauliProduct const& pauli_product, double coeff)
    : _pauli_product(pauli_product), _coeff(coeff) {
    _normalize();
}

std::string QubitHamiltonianTerm::to_string(char signedness) const {
    return fmt::format("{} * {}", _coeff, _pauli_product.to_string(signedness));
}

std::string QubitHamiltonianTerm::to_bit_string() const {
    return fmt::format(
        "{}  {}",
        _pauli_product.to_bit_string().substr(0, 2 * n_qubits() + 1), _coeff);
}

QubitHamiltonianTerm&
QubitHamiltonianTerm::h(size_t qubit) noexcept {
    _pauli_product.h(qubit);
    _normalize();
    return *this;
}

QubitHamiltonianTerm&
QubitHamiltonianTerm::s(size_t qubit) noexcept {
    _pauli_product.s(qubit);
    _normalize();
    return *this;
}

QubitHamiltonianTerm&
QubitHamiltonianTerm::cx(size_t control, size_t target) noexcept {
    _pauli_product.cx(control, target);
    _normalize();
    return *this;
}

QubitHamiltonian::QubitHamiltonian(size_t n_qubits) : _n_qubits(n_qubits) {}

QubitHamiltonian::QubitHamiltonian(
    std::initializer_list<QubitHamiltonianTerm> const& terms)
    : _terms(terms),
      _n_qubits(_terms.begin()->n_qubits()) {}

QubitHamiltonian&
QubitHamiltonian::h(size_t qubit) noexcept {
    for (auto& term : _terms) {
        term.h(qubit);
    }
    return *this;
}

QubitHamiltonian&
QubitHamiltonian::s(size_t qubit) noexcept {
    for (auto& term : _terms) {
        term.s(qubit);
    }
    return *this;
}

QubitHamiltonian&
QubitHamiltonian::cx(size_t control, size_t target) noexcept {
    for (auto& term : _terms) {
        term.cx(control, target);
    }
    return *this;
}

QubitHamiltonian&
QubitHamiltonian::add_term(QubitHamiltonianTerm const& term) {
    if (term.n_qubits() != _n_qubits) {
        throw std::invalid_argument("term has different number of qubits");
    }
    _terms.push_back(term);
    return *this;
}

std::string QubitHamiltonian::to_string() const {
    if (_terms.empty()) {
        return "0";
    }
    return fmt::format(
        "{}",
        fmt::join(
            _terms | std::views::transform([](auto const& term) {
                return term.to_string();
            }),
            " + "));
}

bool is_all_commutative(QubitHamiltonian const& hamilt) {
    for (auto it = hamilt.begin(); it != hamilt.end(); ++it) {
        for (auto jt = std::next(it); jt != hamilt.end(); ++jt) {
            if (!is_commutative(*it, *jt)) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace hamiltonian
}  // namespace qsyn
