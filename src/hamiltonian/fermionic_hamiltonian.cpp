/*
  PackageName  [ hamiltonian ]
  Synopsis     [ Implement fermionic Hamiltonian term class helpers ]
  Author       [ April Wang (april864) ]
*/

#include "hamiltonian/fermionic_hamiltonian.hpp"

#include <cctype>
#include <complex>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

#include "spdlog/spdlog.h"

namespace qsyn::hamiltonian {

}  // namespace qsyn::hamiltonian

std::optional<qsyn::hamiltonian::FermionHamiltonian> read_fermionic_hamiltonian(
    std::filesystem::path const& filepath) {
    using qsyn::hamiltonian::FermionHamiltonian;

    std::ifstream file(filepath);
    if (!file.is_open()) {
        spdlog::error("Cannot open file: {}", filepath.string());
        return std::nullopt;
    }

    struct ParsedTerm {
        std::complex<double> coeff;
        std::vector<std::pair<std::size_t, bool>> ops;
    };

    std::string line;
    std::vector<ParsedTerm> parsed_terms;
    std::size_t max_mode = 0;

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);

        // Default coefficient is 1 + 0j when no explicit "(re, im)" is given
        double real = 1.0;
        double imag = 0.0;

        // Try to parse an explicit "(re, im)" prefix if present
        char ch = 0;
        if (ss >> ch && ch == '(') {
            // We have an explicit coefficient, parse it
            real = 0.0;
            imag = 0.0;

            if (!(ss >> real)) {
                spdlog::error("Failed to parse real part of coefficient in line: '{}'", line);
                return std::nullopt;
            }

            if (!(ss >> ch) || ch != ',') {
                spdlog::error("Expected ',' after real part in line: '{}'", line);
                return std::nullopt;
            }

            if (!(ss >> imag)) {
                spdlog::error("Failed to parse imaginary part of coefficient in line: '{}'", line);
                return std::nullopt;
            }

            if (!(ss >> ch) || ch != ')') {
                spdlog::error("Expected ')' after imaginary part in line: '{}'", line);
                return std::nullopt;
            }
        } else {
            // No leading '(' → treat as implicit coefficient 1+0j.
            // If we successfully read a non-'(' character, put it back so
            // operator tokens are parsed correctly.
            if (ss && ch != 0) {
                ss.unget();
            } else {
                // If extraction failed entirely, fall back to erroring as before.
                spdlog::error("Invalid line format (expected operators) in line: '{}'", line);
                return std::nullopt;
            }
        }

        std::vector<std::pair<std::size_t, bool>> ops;
        std::string token;
        bool has_mode = false;

        while (ss >> token) {
            if (token.empty()) continue;

            bool is_creation = false;
            if (token.back() == '^') {
                is_creation = true;
                token.pop_back();
            }

            if (token.empty() || !std::isdigit(static_cast<unsigned char>(token.front()))) {
                spdlog::error("Invalid mode index token '{}' in line: '{}'", token, line);
                return std::nullopt;
            }

            std::size_t mode_index = 0;
            try {
                mode_index = std::stoul(token);
            } catch (std::exception const&) {
                spdlog::error("Failed to parse mode index '{}' in line: '{}'", token, line);
                return std::nullopt;
            }

            max_mode = std::max(max_mode, mode_index);
            ops.emplace_back(mode_index, is_creation);
            has_mode = true;
        }

        if (!has_mode) {
            spdlog::error("No fermionic operators found in line: '{}'", line);
            return std::nullopt;
        }

        parsed_terms.push_back(ParsedTerm{std::complex<double>(real, imag), std::move(ops)});
    }

    if (parsed_terms.empty()) {
        spdlog::error("File is empty or contains no valid terms.");
        return std::nullopt;
    }

    auto const n_modes = max_mode + 1;
    FermionHamiltonian hamilt(n_modes);

    for (auto const& term : parsed_terms) {
        hamilt.add_term(term.coeff, term.ops);
    }

    return hamilt;
}
