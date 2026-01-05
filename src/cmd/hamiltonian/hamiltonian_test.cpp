/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Implement hamiltonian test commands ]
  Author       [ Mu-Te Lau (joshmtlau) ]
*/

#include "./hamiltonian_test.hpp"

#include "hamiltonian/qubit_hamiltonian.hpp"
#include "hamiltonian/trotterize.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::hamiltonian {

QubitHamiltonian const hamilt = {
    {"XX", 2.0},
    {"ZZ", 3.0},
    {"YY", 4.0}};

dvlab::Command hamiltonian_test_cmd() {
    return Command(
        "trotterize",
        [](ArgumentParser& parser) {
            parser.description("Test Trotterization. Only for testing purposes");

            parser.add_argument<double>("time")
                .help("Time to trotterize for");

            parser.add_argument<size_t>("n-steps")
                .help("Number of steps to trotterize for");
        },
        [](ArgumentParser const& parser) {
            fmt::println("Hamiltonian: {}", hamilt.to_string());

            auto time    = parser.get<double>("time");
            auto n_steps = parser.get<size_t>("n-steps");

            fmt::println("Trotterizing Hamiltonian for {} time steps of {} time", n_steps, time);
            fmt::println("time step: {}", time / static_cast<double>(n_steps));
            auto prtabl = trotterize(hamilt, time, n_steps);

            for (auto const& pr : prtabl) {
                fmt::println("PauliRotation: {}", pr.to_string());
            }

            return CmdExecResult::done;
        });
}

bool add_hamiltonian_test_cmds(dvlab::CommandLineInterface& cli) {
    return cli.add_command(hamiltonian_test_cmd());
}

}  // namespace qsyn::hamiltonian
