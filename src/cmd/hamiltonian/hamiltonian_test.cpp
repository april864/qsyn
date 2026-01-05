/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Implement hamiltonian test commands ]
  Author       [ Mu-Te Lau (joshmtlau) ]
*/

#include "./hamiltonian_test.hpp"

#include "hamiltonian/qubit_hamiltonian.hpp"
#include "hamiltonian/trotterize.hpp"
#include "util/data_structure_manager_common_cmd.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::hamiltonian {

dvlab::Command hamiltonian_trotterize_cmd(QubitHamiltonianMgr const& hamiltonian_mgr) {
    return Command(
        "trotterize",
        [](ArgumentParser& parser) {
            parser.description("Trotterize the focused Hamiltonian");

            parser.add_argument<double>("time")
                .help("Time to trotterize for");

            parser.add_argument<size_t>("n-steps")
                .help("Number of steps to trotterize for");
        },
        [&](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(hamiltonian_mgr)) {
                return CmdExecResult::error;
            }

            auto const* hamiltonian = hamiltonian_mgr.get();
            fmt::println("Hamiltonian: {}", hamiltonian->to_string());

            auto time    = parser.get<double>("time");
            auto n_steps = parser.get<size_t>("n-steps");

            fmt::println("Trotterizing Hamiltonian for {} time steps of {} time", n_steps, time);
            fmt::println("time step: {}", time / static_cast<double>(n_steps));
            auto prtabl = trotterize(*hamiltonian, time, n_steps);

            for (auto const& pr : prtabl) {
                fmt::println("PauliRotation: {}", pr.to_string());
            }

            return CmdExecResult::done;
        });
}

}  // namespace qsyn::hamiltonian
