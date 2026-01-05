/****************************************************************************
  PackageName  [ hamiltonian ]
  Synopsis     [ Define hamiltonian commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./hamiltonian_cmd.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "argparse/arg_parser.hpp"
#include "argparse/arg_type.hpp"
#include "cli/cli.hpp"
#include "cmd/hamiltonian/hamiltonian_test.hpp"
#include "cmd/hamiltonian_mgr.hpp"
#include "hamiltonian/qubit_hamiltonian.hpp"
#include "util/data_structure_manager_common_cmd.hpp"
#include "util/dvlab_string.hpp"

using namespace dvlab::argparse;

namespace qsyn::hamiltonian {

dvlab::Command hamiltonian_print_cmd(QubitHamiltonianMgr const& hamiltonian_mgr) {
    return dvlab::Command{
        "print",
        [](ArgumentParser& parser) {
            parser.description("Print the hamiltonian");
        },
        [&](ArgumentParser const& /* parser */) {
            if (!dvlab::utils::mgr_has_data(hamiltonian_mgr)) {
                return dvlab::CmdExecResult::error;
            }

            fmt::println("Hamiltonian ({} qubits, {} terms)",
                         hamiltonian_mgr.get()->n_qubits(),
                         hamiltonian_mgr.get()->n_terms());
            fmt::println("{}", hamiltonian_mgr.get()->to_string());

            return dvlab::CmdExecResult::done;
        }};
}

dvlab::Command hamiltonian_cmd(QubitHamiltonianMgr& hamiltonian_mgr) {
    auto cmd = dvlab::utils::mgr_root_cmd(hamiltonian_mgr);

    cmd.add_subcommand("qbham-cmd-group", dvlab::utils::mgr_list_cmd(hamiltonian_mgr));
    // cmd.add_subcommand("qbham-cmd-group", hamiltonian_new_cmd(hamiltonian_mgr));
    cmd.add_subcommand("qbham-cmd-group", dvlab::utils::mgr_delete_cmd(hamiltonian_mgr));
    cmd.add_subcommand("qbham-cmd-group", dvlab::utils::mgr_checkout_cmd(hamiltonian_mgr));
    cmd.add_subcommand("qbham-cmd-group", dvlab::utils::mgr_copy_cmd(hamiltonian_mgr));
    // cmd.add_subcommand("qbham-cmd-group", hamiltonian_read_cmd(hamiltonian_mgr));
    // cmd.add_subcommand("qbham-cmd-group", hamiltonian_write_cmd(hamiltonian_mgr));
    cmd.add_subcommand("qbham-cmd-group", hamiltonian_print_cmd(hamiltonian_mgr));
    cmd.add_subcommand("qbham-cmd-group", hamiltonian_trotterize_cmd(hamiltonian_mgr));

    return cmd;
}

bool add_hamiltonian_cmds(dvlab::CommandLineInterface& cli, QubitHamiltonianMgr& hamiltonian_mgr) {
    if (!cli.add_command(hamiltonian_cmd(hamiltonian_mgr))) {
        spdlog::error("Registering \"hamiltonian\" commands fails... exiting");
        return false;
    }

    // for now, add some test hamiltonians
    hamiltonian_mgr.add(0, std::make_unique<QubitHamiltonian>(2));
    hamiltonian_mgr.get()->add_term(QubitHamiltonianTerm("XX", 2.0));
    hamiltonian_mgr.get()->add_term(QubitHamiltonianTerm("ZZ", 3.0));
    hamiltonian_mgr.get()->add_term(QubitHamiltonianTerm("YY", 4.0));

    hamiltonian_mgr.add(1, std::make_unique<QubitHamiltonian>(2));
    hamiltonian_mgr.get()->add_term(QubitHamiltonianTerm("XX", 2.0));
    hamiltonian_mgr.get()->add_term(QubitHamiltonianTerm("XZ", 3.0));
    hamiltonian_mgr.get()->add_term(QubitHamiltonianTerm("ZI", 4.0));
    hamiltonian_mgr.get()->add_term(QubitHamiltonianTerm("IZ", 5.0));
    hamiltonian_mgr.get()->add_term(QubitHamiltonianTerm("YX", 6.0));

    hamiltonian_mgr.checkout(0);
    return true;
}

}  // namespace qsyn::hamiltonian
