/****************************************************************************
  PackageName  [ hamiltonian ]
  Synopsis     [ Define fermionic Hamiltonian commands ]
  Author       [ Design Verification Lab ]
****************************************************************************/

#include "./fham_cmd.hpp"

#include <filesystem>
#include <string>

#include "argparse/arg_parser.hpp"
#include "cli/cli.hpp"
#include "cmd/device_mgr.hpp"
#include "cmd/fham_mgr.hpp"
#include "device/device_analysis.hpp"
#include "hamiltonian/bonsai.hpp"
#include "hamiltonian/f2q_mappings.hpp"
#include "hamiltonian/fermionic_hamiltonian.hpp"
#include "hamiltonian/qubit_hamiltonian.hpp"
#include "hamiltonian/ternary_tree.hpp"
#include "hamiltonian/treespile.hpp"
#include "qcir/qcir.hpp"
#include "util/data_structure_manager_common_cmd.hpp"

using namespace dvlab::argparse;

namespace qsyn::hamiltonian {

namespace {

dvlab::Command fham_read_cmd(FermionHamiltonianMgr& fham_mgr) {
    return dvlab::Command(
        "read",
        [](ArgumentParser& parser) {
            parser.description("Read a fermionic Hamiltonian from a text file");

            parser.add_argument<std::string>("filepath")
                .help(
                    "The path to the input file. "
                    "File format: '(re, im) ops' per line, e.g. '(0.5, 0.5) 0^ 1'");
        },
        [&](ArgumentParser const& parser) {
            auto const filepath = std::filesystem::path(parser.get<std::string>("filepath"));

            auto ferm_opt = read_fermionic_hamiltonian(filepath);
            if (!ferm_opt.has_value()) {
                return dvlab::CmdExecResult::error;
            }

            size_t new_id = fham_mgr.get_next_id();
            fham_mgr.add(new_id, std::make_unique<FermionHamiltonian>(std::move(*ferm_opt)));
            fham_mgr.set_filename(std::filesystem::path{filepath}.stem().string());

            return dvlab::CmdExecResult::done;
        });
}

dvlab::Command fham_qubitize_cmd(FermionHamiltonianMgr& fham_mgr, QubitHamiltonianMgr& qbham_mgr, device::DeviceMgr& device_mgr) {
    return dvlab::Command(
        "qubitize",
        [](ArgumentParser& parser) {
            parser.description(
                "Transform the focused fermionic Hamiltonian to a qubit Hamiltonian");

            parser.add_argument<std::string>("-s", "--strategy")
                .default_value("jw")
                .constraint(choices_allow_prefix({"jw", "ternary_tree"}))
                .help("Fermion-to-qubit mapping strategy: 'jw' (Jordan-Wigner, default) or 'ternary_tree'");

            parser.add_argument<bool>("-o", "--optimize")
                .action(store_true)
                .help("Run simulated annealing to minimize Pauli weight (only applies to ternary_tree strategy)");
        },
        [&](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(fham_mgr)) {
                return dvlab::CmdExecResult::error;
            }

            auto const* f_ham = fham_mgr.get();

            auto const strategy = parser.get<std::string>("--strategy");
            bool optimize       = parser.get<bool>("--optimize");

            QubitHamiltonian q_ham = [&]() {
                if (strategy == "jw") {
                    return qubitize(*f_ham, JordanWignerMapping{f_ham->n_modes()});
                }

                // strategy == "ternary_tree"
                std::optional<TernaryTree> initial_tree = std::nullopt;
                device::Device const* dev_ptr           = nullptr;

                if (!device_mgr.empty()) {
                    dev_ptr = device_mgr.get();
                    if (dev_ptr->get_num_qubits() < f_ham->n_modes()) {
                        fmt::println("Warning: Device ({} qubits) is too small for Hamiltonian ({} modes).", dev_ptr->get_num_qubits(), f_ham->n_modes());
                        fmt::println("Using standard ternary tree...");
                        dev_ptr = nullptr;
                    } else {
                        fmt::println("Using device topology to build ternary tree.");

                        auto apsp        = device::floyd_warshall(*dev_ptr, device::default_floyd_warshall_cost);
                        auto tree_result = build_bonsai_ternary_tree(*dev_ptr, apsp, f_ham->n_modes());

                        if (tree_result.has_value()) {
                            initial_tree = std::move(tree_result.value());
                        } else {
                            spdlog::warn("Failed to build Bonsai tree (device too small/disconnected?). Using standard ternary tree.");
                        }
                    }
                }

                if (!initial_tree.has_value()) {
                    if (device_mgr.empty()) {
                        fmt::println("No device. Using standard ternary tree.");
                    }
                    initial_tree = TernaryTree(f_ham->n_modes());
                }
                TernaryTree tree = std::move(initial_tree.value());

                if (optimize) {
                    fmt::println("Optimizing ternary tree mapping to minimize Pauli weight...");
                    device::Device const* dev_ptr = device_mgr.empty() ? nullptr : device_mgr.get();

                    tree = optimize_mapping(tree, *f_ham, dev_ptr);
                }
                return qubitize(*f_ham, TernaryTreeMapping{std::move(tree)});
            }();

            size_t id = qbham_mgr.get_next_id();
            qbham_mgr.add(id, std::make_unique<QubitHamiltonian>(std::move(q_ham)));
            qbham_mgr.set_filename(fham_mgr.get_filename());
            qbham_mgr.add_procedures(fham_mgr.get_procedures());

            std::string proc_name = strategy == "ternary_tree" ? "fham_qubitize_ternary_tree" : "fham_qubitize_jw";
            if (strategy == "ternary_tree" && optimize) proc_name += "_optimized";
            qbham_mgr.add_procedure(proc_name);

            fmt::println("Transformed focused fermionic Hamiltonian to QubitHamiltonian with ID: {}", id);

            return dvlab::CmdExecResult::done;
        });
}

}  // namespace

namespace {

dvlab::Command fham_print_cmd(FermionHamiltonianMgr const& fham_mgr) {
    return dvlab::Command{
        "print",
        [](ArgumentParser& parser) {
            parser.description("Print the focused fermionic Hamiltonian");
        },
        [&](ArgumentParser const& /*parser*/) {
            if (!dvlab::utils::mgr_has_data(fham_mgr)) {
                return dvlab::CmdExecResult::error;
            }

            auto const* f_ham = fham_mgr.get();
            fmt::println("Fermionic Hamiltonian ({} modes, {} terms)",
                         f_ham->n_modes(),
                         f_ham->get_terms().size());

            auto const& terms = f_ham->get_terms();
            for (std::size_t i = 0; i < terms.size(); ++i) {
                auto const& [coeff, ops] = terms[i];
                fmt::print("  Term {}: ({}, {})  ", i, coeff.real(), coeff.imag());
                for (auto const& [mode, is_creation] : ops) {
                    fmt::print("{}{}", mode, is_creation ? "^ " : " ");
                }
                fmt::println("");
            }

            return dvlab::CmdExecResult::done;
        }};
}

dvlab::Command fham_treespile_cmd(
    device::DeviceMgr& device_mgr,
    FermionHamiltonianMgr& fham_mgr,
    qcir::QCirMgr& qcir_mgr) {
    return dvlab::Command(
        "treespile",
        [](ArgumentParser& parser) {
            parser.description(
                "Apply treespile mapping from the focused fermionic Hamiltonian on the currently loaded device");
            parser.add_argument<double>("time")
                .help("Time for the Hamiltonian to evolve for");
            parser.add_argument<size_t>("n-steps")
                .help("Number of trotterization steps to apply");
            parser.add_argument<std::string>("--cost-fn")
                .constraint(choices_allow_prefix({"log_success_rate", "default"}))
                .default_value("default")
                .help("cost function for Floyd-Warshall used inside treespile");
            parser.add_argument<bool>("-o", "--optimize")
                .action(store_true)
                .help("Run simulated annealing to minimize Pauli weight");
        },
        [&](ArgumentParser const& parser) {
            if (device_mgr.empty()) {
                spdlog::error("No device loaded. Read or fetch a device first.");
                return dvlab::CmdExecResult::error;
            }

            if (!dvlab::utils::mgr_has_data(fham_mgr)) {
                spdlog::error("No fermionic Hamiltonian loaded. Read or create one first.");
                return dvlab::CmdExecResult::error;
            }

            auto const& device = *device_mgr.get();
            auto const* f_ham  = fham_mgr.get();

            auto const n_steps = parser.get<size_t>("n-steps");
            if (n_steps == 0) {
                spdlog::error("Number of trotterization steps must be greater than 0");
                return dvlab::CmdExecResult::error;
            }

            auto const time        = parser.get<double>("time");
            auto const cost_fn_str = parser.get<std::string>("--cost-fn");
            bool optimize          = parser.get<bool>("--optimize");
            auto cost_fn           = device::default_floyd_warshall_cost;
            if (dvlab::str::is_prefix_of(dvlab::str::tolower_string(cost_fn_str), "log_success_rate")) {
                cost_fn = device::log_success_rate_floyd_warshall_cost;
            }

            auto const result = treespile(*f_ham, device, time, n_steps, cost_fn, optimize);

            if (!result.has_value()) {
                auto const reason = result.error();
                switch (reason) {
                    case TreespileFailReason::empty_hamiltonian:
                        spdlog::error("treespile failed: empty Hamiltonian");
                        break;
                    case TreespileFailReason::device_too_small:
                        spdlog::error("treespile failed: device too small");
                        break;
                    case TreespileFailReason::tt_build_failed_not_enough_qubits:
                        spdlog::error("treespile failed: could not build ternary tree (not enough qubits / disconnected)");
                        break;
                    case TreespileFailReason::invalid_n_trotterization_steps:
                        spdlog::error("treespile failed: invalid number of trotterization steps");
                        break;
                }
                return dvlab::CmdExecResult::error;
            }

            auto circuit      = result.value();
            auto const new_id = qcir_mgr.get_next_id();

            qcir_mgr.add(new_id, std::make_unique<qcir::QCir>(std::move(circuit)));
            qcir_mgr.set_filename(fham_mgr.get_filename());
            qcir_mgr.add_procedures(fham_mgr.get_procedures());
            qcir_mgr.add_procedure("fham_treespile");

            return dvlab::CmdExecResult::done;
        });
}

}  // namespace

dvlab::Command fham_cmd(
    FermionHamiltonianMgr& fham_mgr,
    QubitHamiltonianMgr& qbham_mgr,
    qcir::QCirMgr& qcir_mgr,
    device::DeviceMgr& device_mgr) {
    auto cmd = dvlab::utils::mgr_root_cmd(fham_mgr);

    cmd.add_subcommand("fham-cmd-group", dvlab::utils::mgr_list_cmd(fham_mgr));
    cmd.add_subcommand("fham-cmd-group", dvlab::utils::mgr_delete_cmd(fham_mgr));
    cmd.add_subcommand("fham-cmd-group", dvlab::utils::mgr_checkout_cmd(fham_mgr));
    cmd.add_subcommand("fham-cmd-group", dvlab::utils::mgr_copy_cmd(fham_mgr));
    cmd.add_subcommand("fham-cmd-group", fham_read_cmd(fham_mgr));
    cmd.add_subcommand("fham-cmd-group", fham_qubitize_cmd(fham_mgr, qbham_mgr, device_mgr));
    cmd.add_subcommand("fham-cmd-group", fham_treespile_cmd(device_mgr, fham_mgr, qcir_mgr));
    cmd.add_subcommand("fham-cmd-group", fham_print_cmd(fham_mgr));

    return cmd;
}

bool add_fham_cmds(
    dvlab::CommandLineInterface& cli,
    FermionHamiltonianMgr& fham_mgr,
    QubitHamiltonianMgr& qbham_mgr,
    qcir::QCirMgr& qcir_mgr,
    device::DeviceMgr& device_mgr) {
    if (!cli.add_command(fham_cmd(fham_mgr, qbham_mgr, qcir_mgr, device_mgr))) {
        spdlog::error("Registering \"fham\" commands fails... exiting");
        return false;
    }

    return true;
}

}  // namespace qsyn::hamiltonian
