/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Implement hamiltonian test commands ]
  Author       [ Mu-Te Lau (joshmtlau) ]
*/

#include "./sort.hpp"

#include "hamiltonian/qbham_transformations.hpp"
#include "hamiltonian/qubit_hamiltonian.hpp"
#include "util/data_structure_manager_common_cmd.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::hamiltonian {

dvlab::Command qbham_sort_cmd(QubitHamiltonianMgr const& qbham_mgr) {
    return Command(
        "sort",
        [](ArgumentParser& parser) {
            parser.description("Sort the hamiltonian");

            parser.add_argument<std::string>("strategy")
                .help(
                    "The sorting strategy to use. "
                    "Currently, only 'lex' and 'magnitude' are supported.")
                .constraint(choices_allow_prefix({"lex", "magnitude"}));

            parser.add_argument<std::string>("--order")
                .help(
                    "The order of the Pauli terms for the `lex` strategy. "
                    "Case-sensitive. Must contains all characters in 'xyzi' exactly once."
                    "Default is 'xyzi'. This option is ignored for other strategies.")
                .default_value("xyzi")
                .constraint(is_valid_pauli_letter_order);
        },
        [&](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(qbham_mgr)) {
                return CmdExecResult::error;
            }

            fmt::println("Hamiltonian before sorting:\n{}",
                         qbham_mgr.get()->to_string());

            auto const strategy  = parser.get<std::string>("strategy");
            auto const order_str = parser.get<std::string>("--order");

            if (strategy == "lex") {
                lexicographic_sort(*qbham_mgr.get(), order_str);
            } else if (strategy == "magnitude") {
                magnitude_sort(*qbham_mgr.get());
            }

            fmt::println("Hamiltonian after sorting:\n{}",
                         qbham_mgr.get()->to_string());

            return CmdExecResult::done;
        });
}

}  // namespace qsyn::hamiltonian
