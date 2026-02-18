/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Implement Jordan-Wigner transformation ]
  Author       [ April Wang (april864) ]
*/

#include <fmt/core.h>

#include "hamiltonian/jordan_wigner.hpp"
#include <complex>
#include <iostream>
#include <map>

#include "tableau/pauli_product_trait.hpp"

namespace qsyn::hamiltonian {

using namespace qsyn::tableau;
using Pauli = qsyn::tableau::Pauli;

struct ComplexTerm {
    QubitHamiltonianTerm term;
    std::complex<double> coeff;

    ComplexTerm(QubitHamiltonianTerm t, std::complex<double> c) : term(t), coeff(c) {}
};

// Multiplies two Pauli terms, keeping track of phase (e.g. X*Y=iZ)
// Could implement with something from pauli_product_trait?
std::pair<QubitHamiltonianTerm, std::complex<double>> multiply_terms(
    QubitHamiltonianTerm const& a, QubitHamiltonianTerm const& b) {
    
    // Store result as vector of Paulis and a phase factor   
    size_t n = std::max(a.n_qubits(), b.n_qubits());
    std::vector<Pauli> new_paulis;
    new_paulis.reserve(n);
    
    std::complex<double> phase = 1.0;

    for (size_t i = 0; i < n; ++i) {
        Pauli p1;
        Pauli p2;
        // If one term has fewer operators, treat missing operators as I
        if (i < a.n_qubits()) {
            p1 = a.get_pauli_type(i);
        } else { p1 = Pauli::i; }
        if (i < b.n_qubits()) {
            p2 = b.get_pauli_type(i);
        } else { p2 = Pauli::i; }
        
        // Do the multiplication, adding each qubit's result and phase to new_paulis
        if (p1 == Pauli::i) {
            new_paulis.push_back(p2);
        } else if (p2 == Pauli::i) {
            new_paulis.push_back(p1);
        } else if (p1 == p2) {
            new_paulis.push_back(Pauli::i);
        } else {
            if (p1 == Pauli::x && p2 == Pauli::y) {      // XY = iZ
                new_paulis.push_back(Pauli::z); phase *= std::complex<double>(0, 1);
            } else if (p1 == Pauli::x && p2 == Pauli::z) { // XZ = -iY
                new_paulis.push_back(Pauli::y); phase *= std::complex<double>(0, -1);
            } else if (p1 == Pauli::y && p2 == Pauli::x) { // YX = -iZ
                new_paulis.push_back(Pauli::z); phase *= std::complex<double>(0, -1);
            } else if (p1 == Pauli::y && p2 == Pauli::z) { // YZ = iX
                new_paulis.push_back(Pauli::x); phase *= std::complex<double>(0, 1);
            } else if (p1 == Pauli::z && p2 == Pauli::x) { // ZX = iY
                new_paulis.push_back(Pauli::y); phase *= std::complex<double>(0, 1);
            } else if (p1 == Pauli::z && p2 == Pauli::y) { // ZY = -iX
                new_paulis.push_back(Pauli::x); phase *= std::complex<double>(0, -1);
            }
        }
    }

    return {QubitHamiltonianTerm(new_paulis, 1.0), phase * a.coeff() * b.coeff()};
}

QubitHamiltonian jordan_wigner(FermionHamiltonian const& f_hamilt) {
    size_t n_qubits = f_hamilt.n_modes();
    
    // Accumulator for the final result; maps Pauli strings to their coefficients
    // Key: string representation of Pauli (for easy summing), Value: ComplexTerm
    std::map<std::string, std::complex<double>> term_map;

    for (const auto& f_term : f_hamilt.get_terms()) {
        double original_coeff = f_term.first;
        const auto& operators = f_term.second;

        // Start with identity and original coefficient
        std::vector<ComplexTerm> current_state;
        current_state.emplace_back(QubitHamiltonianTerm({}, 1.0), std::complex<double>(original_coeff, 0.0));

        // Create JW term for each operator in the fermionic term
        for (const auto& op : operators) {
            size_t p = op.first;
            bool is_dagger = op.second;

            // Make Z string for qubits 0 to p-1
            std::vector<Pauli> z_string(n_qubits, Pauli::i);
            for(size_t k=0; k<p; k++) z_string[k] = Pauli::z;
            
            // X part: (Z...Z X_p)
            std::vector<Pauli> x_part = z_string;
            x_part[p] = Pauli::x;
            
            // Y part: (Z...Z Y_p)
            std::vector<Pauli> y_part = z_string;
            y_part[p] = Pauli::y;

            // Coefficients
            // a  -> 0.5 (X + iY)
            // a^ -> 0.5 (X - iY)
            std::complex<double> x_coeff = 0.5;
            std::complex<double> y_coeff;
            if (is_dagger) { y_coeff = std::complex<double>(0, -0.5); } 
            else { y_coeff = std::complex<double>(0, 0.5); }

            std::vector<ComplexTerm> op_terms;
            op_terms.emplace_back(QubitHamiltonianTerm(x_part, 1.0), x_coeff);
            op_terms.emplace_back(QubitHamiltonianTerm(y_part, 1.0), y_coeff);

            // Multiply current_state * op_terms
            std::vector<ComplexTerm> next_state;
            for (const auto& s : current_state) {
                for (const auto& o : op_terms) {
                    auto res = multiply_terms(s.term, o.term);
                    // Combine coeffs: (s.coeff * o.coeff) * (phase from mult)
                    std::complex<double> final_c = s.coeff * o.coeff * res.second;
                    next_state.emplace_back(res.first, final_c);
                }
            }
            current_state = next_state;
        }

        // Add term to term_map, combining like terms
        for (const auto& t : current_state) {
            std::string pauli_str = "";
            for (size_t i = 0; i < n_qubits; ++i) {
                Pauli p = t.term.get_pauli_type(i);
                if (p == Pauli::i) pauli_str += 'I';
                else if (p == Pauli::x) pauli_str += 'X';
                else if (p == Pauli::y) pauli_str += 'Y';
                else if (p == Pauli::z) pauli_str += 'Z';
            }
            
            term_map[pauli_str] += t.coeff;
        }
    }

    // Make final qubit hamiltonian from term_map
    QubitHamiltonian final_ham(n_qubits);
    for (const auto& [pauli_str, coeff] : term_map) {
        // Ignore 0 terms
        if (coeff == std::complex<double>(0, 0)) continue; 

        QubitHamiltonianTerm new_term(pauli_str, coeff.real());
        final_ham.add_term(new_term);
    }
    
    return final_ham;
}

} // namespace qsyn::hamiltonian