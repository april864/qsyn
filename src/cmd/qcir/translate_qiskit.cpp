/****************************************************************************
  PackageName  [ qcir/translate_qiskit ]
  Synopsis     [ Run translate_and_optimize_qasm.py and load result into QCir ]
  Author       [ Design Verification Lab ]
****************************************************************************/

#include "./translate_qiskit.hpp"

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <fstream>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "qcir/qcir.hpp"
#include "qcir/qcir_io.hpp"
#include "util/data_structure_manager_common_cmd.hpp"
#include "util/sysdep.hpp"
#include "util/tmp_files.hpp"

namespace qsyn::qcir {

namespace {

bool is_non_unitary_gate(std::string_view name) {
    static std::set<std::string, std::less<>> const excluded{
        "measure", "reset", "barrier", "delay", "snapshot", "store", "load"};
    return excluded.contains(name);
}

std::vector<std::string> unitary_basis_from_device(device::Device const& device) {
    std::vector<std::string> basis;
    for (auto const& gate : device.get_gate_set()) {
        if (is_non_unitary_gate(gate)) {
            continue;
        }
        basis.push_back(gate);
    }
    return basis;
}

bool write_basis_gates_json(std::filesystem::path const& path, std::vector<std::string> const& basis) {
    std::ofstream out(path);
    if (!out) {
        spdlog::error("Failed to open {} for writing", path.string());
        return false;
    }
    out << nlohmann::json(basis).dump();
    return true;
}

}  // namespace

dvlab::CmdExecResult translate_qiskit(
    QCirMgr& qcir_mgr,
    device::DeviceMgr const& device_mgr,
    std::optional<std::string> backend,
    bool use_real_backend,
    bool no_optimize) {
    namespace fs = std::filesystem;
    namespace dv = dvlab::utils;

    if (!dv::mgr_has_data(qcir_mgr)) {
        return dvlab::CmdExecResult::error;
    }

    if (use_real_backend && (!backend.has_value() || backend->empty())) {
        spdlog::error("--use-real-backend requires --backend");
        return dvlab::CmdExecResult::error;
    }

    dv::TmpDir const tmp_dir;
    fs::path const tmp_qasm_input      = tmp_dir.path() / "input.qasm";
    fs::path const tmp_qasm_output     = tmp_dir.path() / "output.qasm";
    fs::path const tmp_basis_gates     = tmp_dir.path() / "basis_gates.json";
    bool const use_device_basis        = !backend.has_value() || backend->empty();
    std::string procedure_label;

    if (!qcir_mgr.get()->write_qasm(tmp_qasm_input)) {
        spdlog::error("Failed to write QCir to temporary QASM file");
        return dvlab::CmdExecResult::error;
    }

    auto const path_to_script = "scripts/translate_and_optimize_qasm.py";
    std::vector<std::string> args{
        tmp_qasm_input.string(),
        "-o",
        tmp_qasm_output.string(),
    };

    if (use_device_basis) {
        if (device_mgr.empty()) {
            spdlog::error(
                "No focused device loaded. Fetch/read a device or pass --backend for qcir translate --qiskit");
            return dvlab::CmdExecResult::error;
        }

        auto const* device = device_mgr.get();
        auto const basis     = unitary_basis_from_device(*device);
        if (basis.empty()) {
            spdlog::error(
                "Focused device '{}' has no unitary gates in its gate set",
                device->get_name());
            return dvlab::CmdExecResult::error;
        }

        if (!write_basis_gates_json(tmp_basis_gates, basis)) {
            return dvlab::CmdExecResult::error;
        }

        args.emplace_back("--basis-gates-file");
        args.emplace_back(tmp_basis_gates.string());
        procedure_label = fmt::format("device:{}", device->get_name());
        spdlog::info(
            "Translating QCir with Qiskit (device '{}' gate set: [{}], optimize={})...",
            device->get_name(),
            fmt::join(basis, ", "),
            !no_optimize);
    } else {
        args.emplace_back("--backend");
        args.emplace_back(*backend);
        if (use_real_backend) {
            args.emplace_back("--use-real-backend");
        }
        procedure_label = *backend;
        spdlog::info(
            "Translating QCir with Qiskit (backend '{}', real={}, optimize={})...",
            *backend,
            use_real_backend,
            !no_optimize);
    }

    if (no_optimize) {
        args.emplace_back("--no-optimize");
    }

    if (spdlog::get_level() <= spdlog::level::info) {
        args.emplace_back("--verbose");
    }

    auto const exit_code = dv::uv_run_script(path_to_script, args);
    if (exit_code != 0) {
        spdlog::error("Qiskit translation failed with exit code {}", exit_code);
        return dvlab::CmdExecResult::error;
    }

    if (!fs::exists(tmp_qasm_output)) {
        spdlog::error("Qiskit translation did not produce output at {}", tmp_qasm_output.string());
        return dvlab::CmdExecResult::error;
    }

    auto translated = from_qasm(tmp_qasm_output);
    if (!translated.has_value()) {
        spdlog::error("Failed to read translated QASM from {}", tmp_qasm_output.string());
        return dvlab::CmdExecResult::error;
    }

    auto const filename = qcir_mgr.get_filename();
    qcir_mgr.set(std::make_unique<QCir>(std::move(*translated)));
    qcir_mgr.set_filename(filename);
    qcir_mgr.add_procedure(fmt::format("qcir_translate_qiskit {}", procedure_label));

    spdlog::info("Successfully translated QCir with Qiskit (qubit indices preserved)");
    return dvlab::CmdExecResult::done;
}

}  // namespace qsyn::qcir
