/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Define fermionic Hamiltonian term class ]
  Author       [ April Wang (april864) ]
*/

#pragma once

#include <complex>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <vector>

namespace qsyn::hamiltonian {

class FermionHamiltonian {
public:
    using Term = std::pair<std::complex<double>, std::vector<std::pair<std::size_t, bool>>>;
    // Term format: {coefficient, [(mode_index, is_creation_op), ...]}

    FermionHamiltonian(std::size_t n_modes) : _n_modes(n_modes) {}

    void add_term(std::complex<double> coeff, std::vector<std::pair<std::size_t, bool>> const& ops) {
        _terms.emplace_back(coeff, ops);
    }

    std::size_t n_modes() const { return _n_modes; }

    std::vector<Term> const& get_terms() const { return _terms; }

private:
    std::size_t _n_modes;
    std::vector<Term> _terms;
};

}  // namespace qsyn::hamiltonian

std::optional<qsyn::hamiltonian::FermionHamiltonian> read_fermionic_hamiltonian(
    std::filesystem::path const& filepath);
