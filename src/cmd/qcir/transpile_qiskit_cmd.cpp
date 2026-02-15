/****************************************************************************
  PackageName  [ qcir/transpile_qiskit ]
  Synopsis     [ Define transpile_qiskit command ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./transpile_qiskit_cmd.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "../qcir_mgr.hpp"
#include "argparse/arg_parser.hpp"
#include "argparse/arg_type.hpp"
#include "cli/cli.hpp"
#include "qcir/qcir.hpp"
#include "qcir/qcir_io.hpp"
#include "util/data_structure_manager_common_cmd.hpp"
#include "util/sysdep.hpp"
#include "util/tmp_files.hpp"
#include "util/util.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::qcir {

Command qcir_transpile_qiskit_cmd(QCirMgr& qcir_mgr) {
    return {
        "transpile-qiskit",
        [](ArgumentParser& parser) {
            parser.description("Transpile the current QCir using Qiskit and checks the resulting circuit to a new QCir");

            parser.add_argument<std::string>("-b", "--backend")
                .default_value("")
                .help("the backend to use for transpilation (optional; if omitted, transpiles without qubit routing)");

            parser.add_argument<int>("-o", "--optimization-level")
                .choices({0, 1, 2, 3})
                .default_value(2)
                .help("optimization level (0-3, default: 2)");
        },
        [&](ArgumentParser const& parser) -> CmdExecResult {
            namespace fs = std::filesystem;
            namespace dv = dvlab::utils;

            // Check if QCir is loaded
            if (!dvlab::utils::mgr_has_data(qcir_mgr)) {
                return CmdExecResult::error;
            }

            // Get arguments
            auto const backend   = parser.get<std::string>("--backend");
            auto const opt_level = parser.get<int>("--optimization-level");

            // Create temporary directory and file for temporary QASMs
            dv::TmpDir const tmp_dir;
            fs::path const tmp_qasm_input  = tmp_dir.path() / "input.qasm";
            fs::path const tmp_qasm_output = tmp_dir.path() / "output.qasm";

            // Write current QCir to temp QASM file
            if (!qcir_mgr.get()->write_qasm(tmp_qasm_input)) {
                spdlog::error("Failed to write QCir to temporary QASM file");
                return CmdExecResult::error;
            }

            // Build command to call the Python script
            auto const path_to_script = "scripts/qiskit_transpile.py";
            auto args                 = std::vector<std::string>{
                "-input",
                tmp_qasm_input.string(),
                "-output",
                tmp_qasm_output.string(),
                "-optimization_level",
                std::to_string(opt_level),
            };
            if (backend.empty()) {
                spdlog::info("Transpiling QCir using Qiskit without qubit routing (optimization level {})...", opt_level);
            } else {
                args.push_back("-backend");
                args.push_back(backend);
                spdlog::info("Transpiling QCir using Qiskit with backend '{}' and optimization level {}...", backend, opt_level);
            }

            if (auto result = dvlab::utils::uv_run_script(path_to_script, args) == 0) {
                auto new_qcir = qcir::from_qasm(tmp_qasm_output.string());
                if (!new_qcir.has_value()) {
                    spdlog::error("Failed to read the output QASM file");
                    return CmdExecResult::error;
                }
                auto new_id = qcir_mgr.get_next_id();
                qcir_mgr.add(new_id, std::make_unique<QCir>(std::move(*new_qcir)));
                spdlog::info("Successfully transpiled QCir and checked the resulting circuit to QCir {}", new_id);
                qcir_mgr.checkout(new_id);
                return CmdExecResult::done;
            } else {
                spdlog::error("Transpilation failed with exit code {}", result);
                return CmdExecResult::error;
            }
        }};
}

}  // namespace qsyn::qcir
