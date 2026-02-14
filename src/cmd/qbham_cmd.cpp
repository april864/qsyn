/****************************************************************************
  PackageName  [ hamiltonian ]
  Synopsis     [ Define hamiltonian commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qbham_cmd.hpp"

#include <string>

#include "argparse/arg_parser.hpp"
#include "cli/cli.hpp"
#include "cmd/qbham/ferm_to_qubit.hpp"
#include "cmd/qbham/sort.hpp"
#include "cmd/qbham/trotterize.hpp"
#include "cmd/qbham/read.hpp"
#include "cmd/qbham_mgr.hpp"
#include "cmd/tableau_mgr.hpp"
#include "hamiltonian/qubit_hamiltonian.hpp"
#include "util/data_structure_manager_common_cmd.hpp"

using namespace dvlab::argparse;

namespace qsyn::hamiltonian {

dvlab::Command qbham_print_cmd(QubitHamiltonianMgr const& qbham_mgr) {
    return dvlab::Command{
        "print",
        [](ArgumentParser& parser) {
            parser.description("Print the hamiltonian");
        },
        [&](ArgumentParser const& /* parser */) {
            if (!dvlab::utils::mgr_has_data(qbham_mgr)) {
                return dvlab::CmdExecResult::error;
            }

            fmt::println("Qubit Hamiltonian ({} qubits, {} terms)",
                         qbham_mgr.get()->n_qubits(),
                         qbham_mgr.get()->n_terms());
            fmt::println("{}", qbham_mgr.get()->to_string());

            return dvlab::CmdExecResult::done;
        }};
}

dvlab::Command qbham_cmd(QubitHamiltonianMgr& qbham_mgr, tableau::TableauMgr& tableau_mgr) {
    auto cmd = dvlab::utils::mgr_root_cmd(qbham_mgr);

    cmd.add_subcommand("qbham-cmd-group", dvlab::utils::mgr_list_cmd(qbham_mgr));
    // cmd.add_subcommand("qbham-cmd-group", qbham_new_cmd(qbham_mgr));
    cmd.add_subcommand("qbham-cmd-group", dvlab::utils::mgr_delete_cmd(qbham_mgr));
    cmd.add_subcommand("qbham-cmd-group", dvlab::utils::mgr_checkout_cmd(qbham_mgr));
    cmd.add_subcommand("qbham-cmd-group", dvlab::utils::mgr_copy_cmd(qbham_mgr));
    cmd.add_subcommand("qbham-cmd-group", qbham_ferm_to_qubit_cmd(qbham_mgr));
    cmd.add_subcommand("qbham-cmd-group", qbham_read_cmd(qbham_mgr));
    // cmd.add_subcommand("qbham-cmd-group", qbham_write_cmd(qbham_mgr));
    cmd.add_subcommand("qbham-cmd-group", qbham_print_cmd(qbham_mgr));
    cmd.add_subcommand("qbham-cmd-group", qbham_trotterize_cmd(qbham_mgr, tableau_mgr));
    cmd.add_subcommand("qbham-cmd-group", qbham_sort_cmd(qbham_mgr));

    return cmd;
}

bool add_qbham_cmds(dvlab::CommandLineInterface& cli, QubitHamiltonianMgr& qbham_mgr, tableau::TableauMgr& tableau_mgr) {
    if (!cli.add_command(qbham_cmd(qbham_mgr, tableau_mgr))) {
        spdlog::error("Registering \"qbham\" commands fails... exiting");
        return false;
    }
    
    return true;
}

}  // namespace qsyn::hamiltonian
