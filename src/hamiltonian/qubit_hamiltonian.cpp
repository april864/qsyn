/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Define qubit Hamiltonian term class ]
  Author       [ Mu-Te Lau (joshmtlau) ]
*/

#include "qubit_hamiltonian.hpp"

#include <fmt/format.h>

namespace qsyn {

namespace hamiltonian {

ComplexPauliTerm to_complex_pauli_term(HermitianPauliTerm const& term) {
    return ComplexPauliTerm(term.pauli_product(), std::complex<double>(term.coeff(), 0.0));
}

HermitianPauliTerm to_hermitian_pauli_term(ComplexPauliTerm const& term) {
    return HermitianPauliTerm(term.pauli_product(), term.coeff().real());
}

QubitHamiltonian::QubitHamiltonian(size_t n_qubits) : _n_qubits(n_qubits), _filename("") {}

QubitHamiltonian::QubitHamiltonian(
    std::initializer_list<HermitianPauliTerm> const& terms)
    : _terms(terms),
      _n_qubits(_terms.begin()->n_qubits()),
      _filename("") {}

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
QubitHamiltonian::add_term(HermitianPauliTerm const& term) {
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

ComplexPauliTerm& ComplexPauliTerm::operator*=(ComplexPauliTerm const& rhs) {
    auto const power_of_i = qsyn::tableau::power_of_i(
        this->pauli_product(), rhs.pauli_product());
    if ((power_of_i % 2) == 1) {
        this->coeff() *= std::complex<double>(0, 1);
    }
    this->pauli_product() *= rhs.pauli_product();
    this->coeff() *= rhs.coeff();
    this->_normalize();
    return *this;
}

}  // namespace hamiltonian
}  // namespace qsyn
