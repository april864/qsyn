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
            parser.description("Transpile the current QCir using Qiskit");

            parser.add_argument<std::string>("output")
                .help("the output QASM file path");

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

            // Check if qiskit is installed
            if (!dv::python_package_exists("qiskit")) {
                spdlog::error("qiskit is not installed in the system!!");
                spdlog::error("Please install qiskit first or check if you have used the correct python environment!!");
                return CmdExecResult::error;
            }

            // Get arguments
            auto output_path = parser.get<std::string>("output");
            auto backend     = parser.get<std::string>("--backend");
            auto opt_level   = parser.get<int>("--optimization-level");

            // Check if output path is writable
            std::ofstream test_output(output_path);
            if (!test_output) {
                spdlog::error("Cannot write to output file: {}", output_path);
                return CmdExecResult::error;
            }
            test_output.close();
            fs::remove(output_path);  // Remove the test file

            // Create temporary directory and file for input QASM
            dv::TmpDir const tmp_dir;
            fs::path const tmp_qasm = tmp_dir.path() / "input.qasm";

            // Write current QCir to temp QASM file
            if (!qcir_mgr.get()->write_qasm(tmp_qasm)) {
                spdlog::error("Failed to write QCir to temporary QASM file");
                return CmdExecResult::error;
            }

            // Build command to call the Python script
            auto const path_to_script = "scripts/qiskit_transpile.py";
            std::string cmd;
            if (backend.empty()) {
                cmd = fmt::format("python3 {} -input {} -output {} -optimization_level {}",
                                  path_to_script,
                                  tmp_qasm.string(),
                                  output_path,
                                  opt_level);
                spdlog::info("Transpiling QCir using Qiskit without qubit routing (optimization level {})...", opt_level);
            } else {
                cmd = fmt::format("python3 {} -input {} -output {} -backend {} -optimization_level {}",
                                  path_to_script,
                                  tmp_qasm.string(),
                                  output_path,
                                  backend,
                                  opt_level);
                spdlog::info("Transpiling QCir using Qiskit with backend '{}' and optimization level {}...", backend, opt_level);
            }

            // Execute the Python script
            int result = system(cmd.c_str());

            if (result == 0) {
                spdlog::info("Successfully transpiled QCir to {}", output_path);
                return CmdExecResult::done;
            } else {
                spdlog::error("Transpilation failed with exit code {}", result);
                return CmdExecResult::error;
            }
        }};
}

}  // namespace qsyn::qcir
