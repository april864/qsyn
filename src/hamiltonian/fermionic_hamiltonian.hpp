/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Define fermionic Hamiltonian term class ]
  Author       [ April Wang (april864) ]
*/

#pragma once

#include <vector>
#include <tuple>
#include <string>

namespace qsyn::hamiltonian {

class FermionHamiltonian {
public:
    using Term = std::pair<double, std::vector<std::pair<size_t, bool>>>; 
    // Term format: {coefficient, [(mode_index, is_creation_op), ...]}

    FermionHamiltonian(size_t n_modes) : _n_modes(n_modes) {}

    void add_term(double coeff, std::vector<std::pair<size_t, bool>> const& ops) {
        _terms.emplace_back(coeff, ops);
    }

    size_t n_modes() const { return _n_modes; }
    
    std::vector<Term> const& get_terms() const { return _terms; }

private:
    size_t _n_modes;
    std::vector<Term> _terms;
};

}  // namespace qsyn::hamiltonian