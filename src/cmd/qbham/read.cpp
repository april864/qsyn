/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Implement command to read hamiltonian from file ]
  Author       [ April Wang (april864) ]
*/

#include "./read.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

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
            std::string filepath = parser.get<std::string>("filepath");
            std::ifstream file(filepath);

            if (!file.is_open()) {
                spdlog::error("Cannot open file: {}", filepath);
                return CmdExecResult::error;
            }

            std::string line;
            std::vector<std::pair<double, std::string>> terms;
            size_t n_qubits = 0;

            while (std::getline(file, line)) {
                if (line.empty()) continue;

                std::stringstream ss(line);
                double coeff;
                std::string pauli_str;

                if (ss >> coeff >> pauli_str) {
                    if (n_qubits == 0) {
                        n_qubits = pauli_str.length();
                    } else if (pauli_str.length() != n_qubits) {
                        spdlog::error("Inconsistent qubit count in file. Expected {}, got '{}'", n_qubits, pauli_str);
                        return CmdExecResult::error;
                    }
                    terms.emplace_back(coeff, pauli_str);
                }
            }

            if (n_qubits == 0) {
                spdlog::error("File is empty or contains no valid terms.");
                return CmdExecResult::error;
            }

            // Create Hamiltonian
            auto hamilt = std::make_unique<QubitHamiltonian>(n_qubits);
            for (auto const& [coeff, pauli_str] : terms) {
                hamilt->add_term(QubitHamiltonianTerm(pauli_str, coeff)); 
            }

            size_t new_id = qbham_mgr.get_next_id();

            qbham_mgr.add(new_id, std::move(hamilt));

            qbham_mgr.checkout(new_id);

            return CmdExecResult::done;
        });
}

}  // namespace qsyn::hamiltonian
