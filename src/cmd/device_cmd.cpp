/****************************************************************************
  PackageName  [ device ]
  Synopsis     [ Define device package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./device_cmd.hpp"

#include <spdlog/spdlog.h>

#include <memory>
#include <string>

#include "./device_mgr.hpp"
#include "device/device.hpp"
#include "qsyn/qsyn_type.hpp"
#include "util/data_structure_manager_common_cmd.hpp"
#include "util/sysdep.hpp"
#include "util/tmp_files.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;

namespace qsyn::device {

std::function<bool(size_t const&)> valid_device_id(qsyn::device::DeviceMgr const& device_mgr) {
    return [&device_mgr](size_t const& id) {
        if (device_mgr.is_id(id)) return true;
        spdlog::error("Device {} does not exist!!", id);
        return false;
    };
};

dvlab::Command device_checkout_cmd(qsyn::device::DeviceMgr& device_mgr) {
    return {"checkout",
            [&device_mgr](ArgumentParser& parser) {
                parser.description("checkout to Device <id> in DeviceMgr");

                parser.add_argument<size_t>("id")
                    .constraint(valid_device_id(device_mgr))
                    .help("the ID of the device");
            },
            [&device_mgr](ArgumentParser const& parser) {
                device_mgr.checkout(parser.get<size_t>("id"));
                return CmdExecResult::done;
            }};
}

dvlab::Command device_clear_cmd(qsyn::device::DeviceMgr& device_mgr) {
    return {"clear",
            [](ArgumentParser& parser) {
                parser.description("clear DeviceMgr");
            },
            [&device_mgr](ArgumentParser const& /*parser*/) {
                device_mgr.clear();
                return CmdExecResult::done;
            }};
}

dvlab::Command device_delete_cmd(qsyn::device::DeviceMgr& device_mgr) {
    return {"delete",
            [&device_mgr](ArgumentParser& parser) {
                parser.description("remove a Device from DeviceMgr");

                parser.add_argument<size_t>("id")
                    .constraint(valid_device_id(device_mgr))
                    .help("the ID of the device");
            },
            [&device_mgr](ArgumentParser const& parser) {
                device_mgr.remove(parser.get<size_t>("id"));
                return CmdExecResult::done;
            }};
}

dvlab::Command device_read_cmd(qsyn::device::DeviceMgr& device_mgr) {
    return {"read",
            [](ArgumentParser& parser) {
                parser.description("read a device topology");

                parser.add_argument<std::string>("filepath")
                    .help("the filepath to device file");

                parser.add_argument<bool>("-r", "--replace")
                    .action(store_true)
                    .help("if specified, replace the current device; otherwise store to a new one");
            },
            [&device_mgr](ArgumentParser const& parser) {
                qsyn::device::Device buffer_device;
                auto filepath = parser.get<std::string>("filepath");
                auto replace  = parser.get<bool>("--replace");

                if (!buffer_device.read_device(filepath)) {
                    spdlog::error("the format in \"{}\" has something wrong!!", filepath);
                    return CmdExecResult::error;
                }

                if (device_mgr.empty() || !replace) {
                    device_mgr.add(device_mgr.get_next_id(), std::make_unique<qsyn::device::Device>(std::move(buffer_device)));
                } else {
                    device_mgr.set(std::make_unique<qsyn::device::Device>(std::move(buffer_device)));
                }

                return CmdExecResult::done;
            }};
}

dvlab::Command device_fetch_cmd(qsyn::device::DeviceMgr& device_mgr) {
    return {
        "fetch",
        [](ArgumentParser& parser) {
            parser.description(
                "fetch and create a device. Currently only supports "
                "IBM backends. This command tries to retrieve the device "
                "by connecting to the IBM Quantum Platform. An "
                "`IBMQ_API_KEY` needs to be set in the `.env` file. If the "
                "backend is not found, this command will try to retrieve "
                "cached attributes from "
                "`~/.config/qsyn/cached_backend_attrs/`, saved by previous "
                "calls to this command. If that fails, the command will try "
                "to use a fake backend. If that still fails, the command "
                "gives up and returns an error.");

            parser.add_argument<std::string>("backend")
                .help(
                    "the name of the IBM backend to fetch. This function "
                    "tries to normalize the backend to the format expected by "
                    "the IBM Quantum Platform/Qiskit. For example, `fez` will "
                    "be normalized to `ibm_fez` or `fake_fez`.");

            parser.add_argument<bool>("-f", "--fake")
                .action(store_true)
                .help(
                    "Only use fake backend. Note that fake backends will "
                    "not be cached.");

            parser.add_argument<bool>("-c", "--cached")
                .action(store_true)
                .help(
                    "Use cached backend if available; only fetch if "
                    "a cached backend is not available. This flag is "
                    "ignored if `--fake` is specified.");
        },
        [&device_mgr](ArgumentParser const& parser) {
            (void)device_mgr;  // reserved for device loading when hooked up
            auto const fake   = parser.get<bool>("--fake");
            auto const cached = parser.get<bool>("--cached");

            auto const [real_backend_name, fake_backend_name] = [&]() {
                // wrapping the logic in a lambda to avoid accidental use of `backend_name`
                auto const backend_name             = parser.get<std::string>("backend");
                auto const backend_name_without_ibm = backend_name.starts_with("ibm_") ? backend_name.substr(4) : backend_name;
                return std::make_tuple("ibm_" + backend_name_without_ibm, "fake_" + backend_name_without_ibm);
            }();
            auto tmp_dir = dvlab::utils::TmpDir();

            constexpr auto script_path = "scripts/get_ibm_backend_attrs.py";
            auto const home_dir        = dvlab::utils::get_home_directory();
            if (!home_dir) {
                spdlog::error("Cannot find home directory");
                return CmdExecResult::error;
            }

            if (fake) {
                auto const result =
                    dvlab::utils::uv_run_script(
                        script_path,
                        {fake_backend_name,
                         "-f",
                         "-o",
                         tmp_dir.path().string()});

                if (result != 0) {
                    spdlog::error("Failed to get fake backend attributes for {}", fake_backend_name);
                    return CmdExecResult::error;
                } else {
                    return CmdExecResult::done;
                }
            }

            auto const cached_dir = std::filesystem::path(home_dir.value()) / ".config/qsyn/cached_backend_attrs/";

            auto const device_file_path       = real_backend_name + ".json";
            auto const device_file_props_path = real_backend_name + "_properties.json";

            if (cached) {
                if (std::filesystem::exists(cached_dir / device_file_path) && std::filesystem::exists(cached_dir / device_file_props_path)) {
                    fmt::println("Using cached backend attributes for {}", real_backend_name);
                    return CmdExecResult::done;
                } else {
                    spdlog::warn(
                        "Cached backend attributes for {} not found. "
                        "Fetching backend attributes from IBM Quantum Platform...",
                        real_backend_name);
                }
            }

            if (dvlab::utils::uv_run_script(
                    script_path,
                    {real_backend_name,
                     "-o",
                     tmp_dir.path().string()}) == 0) {
                fmt::println("Successfully got backend attributes for {}", real_backend_name);

                std::filesystem::create_directories(cached_dir);
                std::filesystem::copy(tmp_dir.path() / device_file_path, cached_dir / device_file_path);
                std::filesystem::copy(tmp_dir.path() / device_file_props_path, cached_dir / device_file_props_path);

                fmt::println("Saved backend attributes for {} to {}", real_backend_name, cached_dir.string());
                return CmdExecResult::done;
            }

            if (cached) {
                spdlog::error("Failed to get backend attributes for {}!!", real_backend_name);
                return CmdExecResult::error;
            }

            spdlog::warn("Failed to get backend attributes for {}. Trying to use cached backend attributes instead...", real_backend_name);

            if (std::filesystem::exists(cached_dir / device_file_path) && std::filesystem::exists(cached_dir / device_file_props_path)) {
                spdlog::warn("Using cached backend attributes for {}", real_backend_name);
                return CmdExecResult::done;
            }

            spdlog::warn("No cached backend attributes for {} found. Trying to use fake backend instead...", real_backend_name);

            if (dvlab::utils::uv_run_script(
                    script_path,
                    {fake_backend_name,
                     "-f",
                     "-o",
                     tmp_dir.path().string()}) == 0) {
                spdlog::warn("Using fake backend attributes for {}", fake_backend_name);
                return CmdExecResult::done;
            } else {
                spdlog::error("Failed to get fake backend attributes for {}", fake_backend_name);
                return CmdExecResult::error;
            }

            return CmdExecResult::error;
        }};
}

dvlab::Command
device_list_cmd(qsyn::device::DeviceMgr& device_mgr) {
    return {"list",
            [](ArgumentParser& parser) {
                parser.description("list info about Devices");
            },
            [&device_mgr](ArgumentParser const& /* parser */) {
                device_mgr.print_list();

                return CmdExecResult::done;
            }};
}

dvlab::Command device_print_cmd(qsyn::device::DeviceMgr& device_mgr) {
    return {"print",
            [](ArgumentParser& parser) {
                parser.description("print info of device topology");

                auto mutex = parser.add_mutually_exclusive_group().required(false);

                mutex.add_argument<size_t>("-e", "--edges")
                    .nargs(0, 2)
                    .help(
                        "print information of edges. "
                        "If no qubit ID is specified, print for all edges; "
                        "if one qubit ID specified, list the adjacent edges to the qubit; "
                        "if two qubit IDs are specified, list the edge between them");

                mutex.add_argument<size_t>("-q", "--qubits")
                    .nargs(NArgsOption::zero_or_more)
                    .help(
                        "print information of qubits. "
                        "If no qubit ID is specified, print for all qubits;"
                        "otherwise, print information of the specified qubit IDs");

                mutex.add_argument<QubitIdType>("-p", "--path")
                    .nargs(2)
                    .metavar("(q1, q2)")
                    .help(
                        "print routing paths between q1 and q2");
            },
            [&device_mgr](ArgumentParser const& parser) {
                if (!dvlab::utils::mgr_has_data(device_mgr)) return CmdExecResult::error;

                if (parser.parsed("--edges")) {
                    device_mgr.get()->print_edges(parser.get<std::vector<size_t>>("--edges"));
                    return CmdExecResult::done;
                }
                if (parser.parsed("--qubits")) {
                    device_mgr.get()->print_qubits(parser.get<std::vector<size_t>>("--qubits"));
                    return CmdExecResult::done;
                }
                if (parser.parsed("--path")) {
                    auto qids = parser.get<std::vector<QubitIdType>>("--path");
                    device_mgr.get()->print_path(qids[0], qids[1]);
                    return CmdExecResult::done;
                }

                device_mgr.get()->print_topology();
                return CmdExecResult::done;
            }};
}

dvlab::Command device_cmd(qsyn::device::DeviceMgr& device_mgr) {
    auto cmd = dvlab::utils::mgr_root_cmd(device_mgr);
    // print functions
    cmd.add_subcommand("device-cmd-group", dvlab::utils::mgr_list_cmd(device_mgr));
    cmd.add_subcommand("device-cmd-group", device_print_cmd(device_mgr));
    cmd.add_subcommand("device-cmd-group", device_checkout_cmd(device_mgr));
    cmd.add_subcommand("device-cmd-group", device_read_cmd(device_mgr));
    cmd.add_subcommand("device-cmd-group", device_fetch_cmd(device_mgr));
    cmd.add_subcommand("device-cmd-group", dvlab::utils::mgr_delete_cmd(device_mgr));
    return cmd;
}

bool add_device_cmds(dvlab::CommandLineInterface& cli, qsyn::device::DeviceMgr& device_mgr) {
    if (!cli.add_command(device_cmd(device_mgr))) {
        spdlog::critical("Registering \"device\" commands fails... exiting");
        return false;
    }
    return true;
}

}  // namespace qsyn::device
