/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Define qubit Hamiltonian term class ]
  Author       [ Mu-Te Lau (joshmtlau) ]
*/

#pragma once

#include "tableau/pauli_product_trait.hpp"

namespace qsyn {

namespace hamiltonian {

class QubitHamiltonianTerm
    : public qsyn::tableau::PauliProductTrait<QubitHamiltonianTerm> {
public:
    using Pauli        = qsyn::tableau::Pauli;
    using PauliProduct = qsyn::tableau::PauliProduct;
    QubitHamiltonianTerm(
        std::initializer_list<Pauli> const& pauli_list,
        double coeff);

    QubitHamiltonianTerm(std::string_view pauli_str, double coeff);

    QubitHamiltonianTerm(
        PauliProduct const& pauli_product, double coeff);

    template <std::input_iterator I, std::sentinel_for<I> S>
    requires std::same_as<std::iter_value_t<I>, Pauli>
    QubitHamiltonianTerm(I first, S last, double coeff)
        : _pauli_product(first, last, false), _coeff(coeff) {
        _normalize();
    }

    template <std::ranges::range R>
    requires std::same_as<std::ranges::range_value_t<R>, Pauli>
    QubitHamiltonianTerm(R const& r, double coeff)
        : _pauli_product(std::ranges::begin(r), std::ranges::end(r), false),
          _coeff(coeff) {
        _normalize();
    }

    size_t n_qubits() const { return _pauli_product.n_qubits(); }
    Pauli get_pauli_type(size_t i) const { return _pauli_product.get_pauli_type(i); }

    bool is_i(size_t i) const { return _pauli_product.is_i(i); }
    bool is_x(size_t i) const { return _pauli_product.is_x(i); }
    bool is_y(size_t i) const { return _pauli_product.is_y(i); }
    bool is_z(size_t i) const { return _pauli_product.is_z(i); }

    PauliProduct const& pauli_product() const { return _pauli_product; }
    double coeff() const { return _coeff; }
    double& coeff() { return _coeff; }

    bool operator==(QubitHamiltonianTerm const& rhs) const {
        return _pauli_product == rhs._pauli_product && _coeff == rhs._coeff;
    }
    bool operator!=(QubitHamiltonianTerm const& rhs) const { return !(*this == rhs); }

    std::string to_string(char signedness = '-') const;
    std::string to_bit_string() const;

    QubitHamiltonianTerm& h(size_t qubit) noexcept override;
    QubitHamiltonianTerm& s(size_t qubit) noexcept override;
    QubitHamiltonianTerm& cx(size_t control, size_t target) noexcept override;

    bool is_commutative(QubitHamiltonianTerm const& rhs) const {
        return _pauli_product.is_commutative(rhs._pauli_product);
    }

    bool is_diagonal() const { return _pauli_product.is_diagonal(); }

private:
    qsyn::tableau::PauliProduct _pauli_product;
    double _coeff;

    void _normalize() {
        if (_pauli_product.is_neg()) {
            _pauli_product.negate();
            _coeff *= -1;
        }
    }
};

inline bool is_commutative(
    QubitHamiltonianTerm const& lhs, QubitHamiltonianTerm const& rhs) {
    return lhs.is_commutative(rhs);
}

class QubitHamiltonian
    : public qsyn::tableau::PauliProductTrait<QubitHamiltonian> {
public:
    QubitHamiltonian(size_t n_qubits);
    QubitHamiltonian(std::initializer_list<QubitHamiltonianTerm> const& terms);

    size_t n_qubits() const { return _terms.begin()->n_qubits(); }

    QubitHamiltonian& h(size_t qubit) noexcept override;
    QubitHamiltonian& s(size_t qubit) noexcept override;
    QubitHamiltonian& cx(size_t control, size_t target) noexcept override;

    QubitHamiltonian& add_term(QubitHamiltonianTerm const& term);
    template <std::input_iterator I, std::sentinel_for<I> S>
    QubitHamiltonian& add_terms(I first, S last) {
        for (auto it = first; it != last; ++it) {
            add_term(*it);
        }
        return *this;
    }
    template <std::ranges::range R>
    QubitHamiltonian& add_terms(R const& r) {
        return add_terms(std::ranges::begin(r), std::ranges::end(r));
    }

    auto begin() const {
        return _terms.begin();
    }
    auto end() const {
        return _terms.end();
    }
    auto begin() {
        return _terms.begin();
    }
    auto end() {
        return _terms.end();
    }

    auto n_terms() const {
        return _terms.size();
    }

    std::string to_string() const;

    // file and procedure related functions
    auto get_filename() const {
        return _filename;
    }
    auto set_filename(std::string const& filename) {
        _filename = filename;
    }

    auto get_procedures() const {
        return _procedures;
    }
    auto add_procedure(std::string const& procedure) {
        _procedures.push_back(procedure);
    }
    auto add_procedures(std::vector<std::string> const& procedures) {
        _procedures.insert(_procedures.end(), procedures.begin(), procedures.end());
    }

private:
    std::vector<QubitHamiltonianTerm> _terms;
    size_t _n_qubits;
    std::string _filename;
    std::vector<std::string> _procedures;
};

/**
 * Check if all terms in the Hamiltonian are commutative.
 *
 * @param hamiltonian The Hamiltonian to check.
 * @return True if all terms are commutative, false otherwise.
 */
bool is_all_commutative(QubitHamiltonian const& hamilt);

}  // namespace hamiltonian
}  // namespace qsyn
