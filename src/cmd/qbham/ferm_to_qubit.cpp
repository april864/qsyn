/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Implement command to lower fermionic hamiltonian to qubit hamiltonian ]
  Author       [ April Wang (april864) ]
*/

#include "./ferm_to_qubit.hpp"

#include <spdlog/spdlog.h>

#include "cmd/device_mgr.hpp"
#include "hamiltonian/f2q_mappings.hpp"
#include "hamiltonian/fermionic_hamiltonian.hpp"
#include "hamiltonian/treespile.hpp"

using namespace dvlab::argparse;

namespace qsyn::hamiltonian {

dvlab::Command qbham_jw_cmd(QubitHamiltonianMgr& qbham_mgr) {
    return dvlab::Command(
        "jw",
        [](ArgumentParser& parser) {
            parser.description(
                "Transform fermionic Hamiltonian to qubit Hamiltonian "
                "using Jordan-Wigner. Currently testing with a hard-coded fermionic Hamiltonian");
        },
        [&](ArgumentParser const& /*parser*/) {
            // Hard coded fermionic Hamiltonians:
            // fmt::println("Fermionic Hamiltonian: a0^ a1 + a1^ a0");
            // FermionHamiltonian f_ham(2);
            // f_ham.add_term(1.0, {{0, true}, {1, false}});
            // f_ham.add_term(1.0, {{1, true}, {0, false}});

            // fmt::println("Fermionic Hamiltonian: a_5^ a_5");
            // FermionHamiltonian f_ham(6);
            // f_ham.add_term(1.0, {{5, true}, {5, false}});

            fmt::println("Fermionic Hamiltonian: a0^ a2 + a2^ a0");
            FermionHamiltonian f_ham(3);
            f_ham.add_term(1.0, {{0, true}, {2, false}});
            f_ham.add_term(1.0, {{2, true}, {0, false}});

            // JW transformation
            QubitHamiltonian q_ham = qubitize(f_ham, JordanWignerMapping{f_ham.n_modes()});

            size_t id = qbham_mgr.get_next_id();
            qbham_mgr.add(id, std::make_unique<QubitHamiltonian>(std::move(q_ham)));

            fmt::println("Transformed to QubitHamiltonian with ID: {}", id);
            return dvlab::CmdExecResult::done;
        });
}

dvlab::Command qbham_ternary_tree_cmd(QubitHamiltonianMgr& qbham_mgr) {
    return dvlab::Command(
        "ternary_tree",
        [](ArgumentParser& parser) {
            parser.description(
                "Transform fermionic Hamiltonian to qubit Hamiltonian "
                "using ternary tree mapping. Currently testing with a hard-coded fermionic Hamiltonian");
        },
        [&](ArgumentParser const& /*parser*/) {
            // Hard coded fermionic Hamiltonians:
            fmt::println("Fermionic Hamiltonian: a0^ a9 + a9^ a0");
            FermionHamiltonian f_ham(11);
            f_ham.add_term(1.0, {{0, true}, {9, false}});
            f_ham.add_term(1.0, {{9, true}, {0, false}});

            // fmt::println("Fermionic Hamiltonian: a0^ a2 + a2^ a0");
            // FermionHamiltonian f_ham(3);
            // f_ham.add_term(1.0, {{0, true}, {2, false}});
            // f_ham.add_term(1.0, {{2, true}, {0, false}});

            // Ternary tree transformation
            QubitHamiltonian q_ham = qubitize(f_ham, TernaryTreeMapping{f_ham.n_modes()});

            size_t id = qbham_mgr.get_next_id();
            qbham_mgr.add(id, std::make_unique<QubitHamiltonian>(std::move(q_ham)));

            fmt::println("Transformed to QubitHamiltonian with ID: {}", id);
            return dvlab::CmdExecResult::done;
        });
}

}  // namespace qsyn::hamiltonian
