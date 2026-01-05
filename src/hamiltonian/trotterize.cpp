/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Define Trotterization functions ]
  Author       [ Mu-Te Lau (joshmtlau) ]
    */

#include "trotterize.hpp"

#include "tableau/pauli_rotation.hpp"

namespace qsyn::hamiltonian {

namespace {

/**
 * trotterize a single step of a Hamiltonian composed of only commutative terms.
 * Basically, for each term $H_i$ in a Hamiltonian $H$, this function appends
 * Pauli rotations $U_i = \exp(-i H_i \Delta t / 2)$ to the given tableau.
 *
 * @param tableau The tableau to append the Trotterization to.
 * @param hamiltonian The Hamiltonian to trotterize.
 * @param dt The time step.
 */
void append_trotterize_step(
    qsyn::tableau::PauliRotationTableau& prtabl,
    QubitHamiltonian const& hamilt,
    double dt) noexcept {
    using qsyn::tableau::PauliRotation;
    // TODO: Implement the Trotterization for a single step.

    for (auto const& term : hamilt) {
        prtabl.push_back(
            PauliRotation(term.pauli_product(),
                          dvlab::Phase(-term.coeff() * dt)));
    }
}

}  // namespace

/**
 * Trotterize a Hamiltonian for a given time and number of steps.
 * If the Hamiltonian is commutative, the resulting PauliRotationTableau is
 * exact. Otherwise, it implements the Hamiltonian up to an error of
 * order $O(t^2/\mathrm{n\_steps})$.
 *
 * @param hamiltonian The Hamiltonian to trotterize.
 * @param time The time to trotterize for.
 * @param n_steps The number of steps to trotterize for. setting this to 0
 *        will return an empty PauliRotationTableau.
 * @return The Trotterized PauliRotationTableau.
 */
qsyn::tableau::PauliRotationTableau trotterize(
    QubitHamiltonian const& hamiltonian, double time, size_t n_steps) noexcept {
    using dvlab::iterator::next;
    using qsyn::tableau::PauliRotationTableau;

    auto prtabl = PauliRotationTableau{};
    if (hamiltonian.n_terms() == 0 || time == 0.0 || n_steps == 0) {
        return prtabl;
    }

    auto all_commutative = is_all_commutative(hamiltonian);
    auto n_terms         = hamiltonian.n_terms();

    if (all_commutative) {
        prtabl.reserve(n_terms);
    } else {
        prtabl.reserve(n_terms * n_steps);
    }

    auto dt = all_commutative ? time : (time / static_cast<double>(n_steps));

    append_trotterize_step(prtabl, hamiltonian, dt);

    // if the Hamiltonian is not commutative, repeat (n_steps - 1) time more
    // since all steps are the same, we can just copy the existing terms
    if (!all_commutative) {
        for (size_t i = 1; i < n_steps; ++i) {
            prtabl.insert(prtabl.end(),
                          prtabl.begin(), next(prtabl.begin(), n_terms));
        }
    }

    return prtabl;
}

}  // namespace qsyn::hamiltonian
