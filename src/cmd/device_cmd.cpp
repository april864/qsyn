/****************************************************************************
  PackageName  [ device ]
  Synopsis     [ Define device package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./device_cmd.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <string>

#include "./device_mgr.hpp"
#include "device/device.hpp"
#include "device/ibmq_devices.hpp"
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
                auto filepath = parser.get<std::string>("filepath");
                auto replace  = parser.get<bool>("--replace");

                auto device = read_qsyn_device_file(filepath);

                if (!device.has_value()) {
                    spdlog::error("the format in \"{}\" has something wrong!!", filepath);
                    return CmdExecResult::error;
                }

                if (device_mgr.empty() || !replace) {
                    device_mgr.add(device_mgr.get_next_id(), std::make_unique<qsyn::device::Device>(std::move(device.value())));
                } else {
                    device_mgr.set(std::make_unique<qsyn::device::Device>(std::move(device.value())));
                }

                return CmdExecResult::done;
            }};
}

dvlab::Command device_fetch_cmd(qsyn::device::DeviceMgr& device_mgr) {
    return {
        "fetch", [](ArgumentParser& parser) {
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
                    "ignored if `--fake` is specified."); },
        [&device_mgr](ArgumentParser const& parser) {
            (void)device_mgr;  // reserved for device loading when hooked up
            auto const backend_name = parser.get<std::string>("backend");
            auto const fake         = parser.get<bool>("--fake");
            auto const cached       = parser.get<bool>("--cached");

            auto const home_dir = dvlab::utils::get_home_directory();
            if (!home_dir) {
                spdlog::error("Cannot find home directory");
                return CmdExecResult::error;
            }
            // set the cached directory. using make_optional to avoid copying
            auto const cached_dir = std::make_optional((
                std::filesystem::path(home_dir.value()) /
                ".config/qsyn/cached_backend_attrs/"));

            auto const result = fetch_ibmq_device_attrs_with_fallback(
                backend_name, fake, cached, cached_dir);

            if (!result) {
                spdlog::error("Failed to fetch IBM backend attributes for {}", backend_name);
                return CmdExecResult::error;
            }

            if (result->source == IBMQDeviceJsonsSource::cached && !cached) {
                spdlog::warn(
                    "Failed to fetch real backend attributes for {}. "
                    "Using cached backend attributes instead...",
                    backend_name);
            }

            if (result->source == IBMQDeviceJsonsSource::fake && !fake) {
                spdlog::warn(
                    "Failed to fetch real backend attributes for {}. "
                    "Using fake backend attributes instead...",
                    backend_name);
            }

            // TODO: parse and add the device to the device manager

            return CmdExecResult::done;
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

dvlab::Command device_cmd(qsyn::device::DeviceMgr& device_mgr) {
    auto cmd = dvlab::utils::mgr_root_cmd(device_mgr);
    // print functions
    cmd.add_subcommand("device-cmd-group", dvlab::utils::mgr_list_cmd(device_mgr));
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
