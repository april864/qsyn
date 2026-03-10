/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Implement command to read hamiltonian from file ]
  Author       [ April Wang (april864) ]
*/

#include "./read.hpp"

#include <filesystem>
#include <memory>

#include "hamiltonian/qubit_hamiltonian.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::hamiltonian {

dvlab::Command qbham_read_cmd(QubitHamiltonianMgr& qbham_mgr) {
    return Command(
        "read",
        [](ArgumentParser& parser) {
            parser.description("Read a hamiltonian from a text file");

            parser.add_argument<std::string>("filepath")
                .help(
                    "The path to the input file. "
                    "File format must be: 'Coefficient PauliString' per line.");
        },
        [&](ArgumentParser const& parser) {
            auto const filepath = std::filesystem::path(parser.get<std::string>("filepath"));

            auto hamilt = read_qubit_hamiltonian(filepath);
            if (!hamilt.has_value()) {
                return CmdExecResult::error;
            }

            size_t new_id = qbham_mgr.get_next_id();

            auto hamilt_ptr = std::make_unique<QubitHamiltonian>(std::move(*hamilt));
            qbham_mgr.add(new_id, std::move(hamilt_ptr));

            qbham_mgr.checkout(new_id);

            qbham_mgr.set_filename(filepath.string());

            return CmdExecResult::done;
        });
}

}  // namespace qsyn::hamiltonian
