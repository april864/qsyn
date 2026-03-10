/****************************************************************************
  PackageName  [ device ]
  Synopsis     [ Implement reading of Qsyn's native device file ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>
#include <fstream>
#include <gsl/narrow>
#include <ranges>
#include <string>
#include <tl/enumerate.hpp>
#include <tl/to.hpp>
#include <utility>

#include "device/device.hpp"
#include "qcir/basic_gate_type.hpp"  // IWYU pragma: keep
#include "qcir/qcir_gate.hpp"        // IWYU pragma: keep
#include "qsyn/qsyn_type.hpp"
#include "util/dvlab_string.hpp"
#include "util/util.hpp"  // IWYU pragma: keep

namespace qsyn::device {

// details for reading qsyn device file
namespace {

// Gate indices for one-qubit and two-qubit gates
struct GateIndices {
    std::vector<size_t> one_qubit_gate_idxs;
    std::vector<size_t> two_qubit_gate_idxs;
};

// Parse gate set from device file
std::optional<GateIndices>
parse_gate_set(std::string const& gate_set_str, Device& device) {
    std::string _;
    auto const token_end = dvlab::str::str_get_token(gate_set_str, _, 0, ": ");
    auto data            = gate_set_str.substr(token_end + 1);
    data                 = dvlab::str::trim_spaces(data);
    data                 = dvlab::str::remove_brackets(data, '{', '}');

    GateIndices gate_idxs;

    auto gate_type_view =
        dvlab::str::views::tokenize(data, ',') |
        std::views::transform([](auto const& str) { return dvlab::str::tolower_string(str); });

    size_t gate_idx       = 0;
    auto const process_op = [&](auto const& op_opt, std::string const& gate_type) -> bool {
        if (!op_opt.has_value()) {
            return false;
        }

        auto const& op = *op_opt;
        if (op.get_num_qubits() == 1) {
            gate_idxs.one_qubit_gate_idxs.emplace_back(gate_idx);
        } else if (op.get_num_qubits() == 2) {
            gate_idxs.two_qubit_gate_idxs.emplace_back(gate_idx);
        } else {
            spdlog::error("Unsupported gate type ({})!!", gate_type);
            spdlog::error("Only one-qubit and two-qubit gates are supported for device");
            return false;
        }

        device.add_gate_type(op.get_repr().substr(0, op.get_repr().find_first_of('(')));
        return true;
    };

    for (auto const& gate_type : gate_type_view) {
        if (!process_op(qcir::str_to_operation(gate_type), gate_type) &&
            !process_op(qcir::str_to_operation(gate_type, {dvlab::Phase()}), gate_type)) {
            return std::nullopt;
        }
        gate_idx++;
    }

    return gate_idxs;
}

/**
 * @brief Parse device qubits information
 *
 * @param data
 * @param container
 * @return true
 * @return false
 */
bool parse_singles(std::string const& data, std::vector<float>& container) {
    std::string const buffer = dvlab::str::remove_brackets(data, '[', ']');

    for (auto const& token : dvlab::str::views::tokenize(buffer, ',')) {
        auto fl = dvlab::str::from_string<float>(dvlab::str::trim_spaces(token));
        if (!fl.has_value()) {
            spdlog::error("The number `{}` is not a float!!", token);
            return false;
        }
        container.emplace_back(fl.value());
    }
    return true;
}

/**
 * @brief Parse device edges information with type is float
 *
 * @param data
 * @param containers
 * @return true
 * @return false
 */
bool parse_float_pairs(std::string const& data, std::vector<std::vector<float>>& containers) {
    for (auto const& outer_token : dvlab::str::views::tokenize(data, '[')) {
        std::string const buffer{outer_token.substr(0, outer_token.find_first_of(']'))};
        auto floats =
            dvlab::str::views::tokenize(buffer, ',') |
            std::views::transform([](auto const& str) {
                auto result = dvlab::str::from_string<float>(str);
                if (!result.has_value()) {
                    spdlog::error("The number `{}` is not a float!!", str);
                    return std::optional<float>{};
                }
                return result;
            });

        if (std::ranges::any_of(floats, [](auto const& fl) { return !fl.has_value(); })) {
            return false;
        }

        containers.emplace_back(floats | std::views::transform([](auto const& fl) { return fl.value(); }) | tl::to<std::vector>());
    }
    return true;
}

/**
 * @brief Parse device edges information with type size_t
 *
 * @param data
 * @param containers
 * @return true
 * @return false
 */
bool parse_size_t_pairs(std::string const& data, std::vector<std::vector<size_t>>& containers) {
    for (auto const& outer_token : dvlab::str::views::tokenize(data, '[')) {
        std::string const buffer{outer_token.substr(0, outer_token.find_first_of(']'))};
        auto qubit_ids =
            dvlab::str::views::tokenize(buffer, ',') |
            std::views::transform([](auto const& str) {
                auto result = dvlab::str::from_string<size_t>(str);
                if (!result.has_value()) {
                    spdlog::error("The number `{}` is not a positive integer!!", str);
                    return std::optional<size_t>{};
                }
                return result;
            });

        if (std::ranges::any_of(qubit_ids, [](auto const& fl) { return !fl.has_value(); })) {
            return false;
        }

        containers.emplace_back(qubit_ids | std::views::transform([](auto const& fl) { return fl.value(); }) | tl::to<std::vector>());
    }

    return true;
}

/**
 * @brief Parse device information including SGERROR, SGTIME, CNOTERROR, and CNOTTIME
 *
 * @param f
 * @param cx_error
 * @param cx_delay
 * @param single_error
 * @param single_delay
 * @return true
 * @return false
 */
bool parse_info(std::ifstream& f, std::vector<std::vector<float>>& cx_error, std::vector<std::vector<float>>& cx_delay, std::vector<float>& single_error, std::vector<float>& single_delay) {
    std::string str = "", token = "";
    while (true) {
        while (str.empty()) {
            if (f.eof()) break;
            std::getline(f, str);
            str = dvlab::str::trim_spaces(dvlab::str::trim_comments(str));
        }
        auto const token_end = dvlab::str::str_get_token(str, token, 0, ": ");
        auto const data      = dvlab::str::trim_spaces(str.substr(token_end + 1));

        if (token == "SGERROR") {
            if (!parse_singles(std::string{data}, single_error)) return false;
        } else if (token == "SGTIME") {
            if (!parse_singles(std::string{data}, single_delay)) return false;
        } else if (token == "CNOTERROR") {
            if (!parse_float_pairs(std::string{data}, cx_error)) return false;
        } else if (token == "CNOTTIME") {
            if (!parse_float_pairs(std::string{data}, cx_delay)) return false;
        }
        if (f.eof()) {
            break;
        }
        std::getline(f, str);
        str = dvlab::str::trim_spaces(dvlab::str::trim_comments(str));
    }

    return true;
}
}  // namespace

/**
 * @brief Read Qsyn device file and return a Device.
 *
 * @param filename
 * @return std::optional<Device> The parsed device, or std::nullopt on failure.
 */
std::optional<Device> read_qsyn_device_file(std::string const& filename) {
    std::ifstream topo_file(filename);
    if (!topo_file.is_open()) {
        spdlog::error("Cannot open the file \"{}\"!!", filename);
        return std::nullopt;
    }
    std::string str = "", token = "", data = "";

    // NOTE - Device name
    while (str.empty()) {
        std::getline(topo_file, str);
        str = dvlab::str::trim_spaces(dvlab::str::trim_comments(str));
    }
    size_t token_end = dvlab::str::str_get_token(str, token, 0, ": ");
    data             = str.substr(token_end + 1);

    Device device;
    device.set_name(std::string{dvlab::str::trim_spaces(data)});

    // NOTE - Qubit num
    str = "", token = "", data = "";
    while (str.empty()) {
        std::getline(topo_file, str);
        str = dvlab::str::trim_spaces(dvlab::str::trim_comments(str));
    }
    token_end = dvlab::str::str_get_token(str, token, 0, ": ");
    data      = str.substr(token_end + 1);
    data      = dvlab::str::trim_spaces(data);
    auto qbn  = dvlab::str::from_string<unsigned>(data);
    if (!qbn.has_value()) {
        spdlog::error("The number of qubit is not a positive integer!!");
        return std::nullopt;
    }
    // NOTE - Gate set
    str = "", token = "", data = "";
    while (str.empty()) {
        std::getline(topo_file, str);
        str = dvlab::str::trim_spaces(dvlab::str::trim_comments(str));
    }

    auto gate_idxs = parse_gate_set(str, device);
    if (!gate_idxs.has_value()) return std::nullopt;
    auto const& [one_qubit_gate_idxs, two_qubit_gate_idxs] = gate_idxs.value();

    // NOTE - Coupling map
    str = "", token = "", data = "";
    while (str.empty()) {
        std::getline(topo_file, str);
        str = dvlab::str::trim_spaces(dvlab::str::trim_comments(str));
    }

    token_end = dvlab::str::str_get_token(str, token, 0, ": ");
    data      = str.substr(token_end + 1);
    data      = dvlab::str::trim_spaces(data);
    data      = dvlab::str::remove_brackets(data, '[', ']');
    std::vector<std::vector<float>> cx_err, cx_delay;
    std::vector<std::vector<size_t>> adj_list;
    std::vector<float> sg_err, sg_delay;
    if (!parse_size_t_pairs(data, adj_list))
        return std::nullopt;

    // NOTE - Parse Information
    if (!parse_info(topo_file, cx_err, cx_delay, sg_err, sg_delay)) return std::nullopt;

    for (size_t i = 0; i < adj_list.size(); i++) {
        for (size_t j = 0; j < adj_list[i].size(); j++) {
            if (adj_list[i][j] > i) {
                // NOTE - Qsyn's device file format does not specify per gate type delays and errors.
                // Therefore, we assume all two-qubit gates have the same delays and errors.
                for (auto const& gate_idx : two_qubit_gate_idxs) {
                    // NOTE - Qsyn's device file format does not take into account the fact that G(a, b) and G(b, a)
                    // might have different delays and errors.
                    // We will pretend they do for backward compatibility.
                    device.add_gate_info(
                        Device::QubitPair{i, adj_list[i][j]}, GateInfo{.gate_idx = gate_idx, .time = GateDelayNanoSec{cx_delay[i][j]}, .error = cx_err[i][j]});
                    device.add_gate_info(
                        Device::QubitPair{adj_list[i][j], i}, GateInfo{.gate_idx = gate_idx, .time = GateDelayNanoSec{cx_delay[i][j]}, .error = cx_err[i][j]});
                }
            }
        }
    }

    assert(sg_err.size() == sg_delay.size());
    for (size_t i = 0; i < sg_err.size(); i++) {
        // NOTE - Qsyn's device file format does not specify per gate type delays and errors.
        // Therefore, we assume all one-qubit gates have the same delays and errors.
        for (auto const& gate_idx : one_qubit_gate_idxs) {
            device.add_gate_info(i, GateInfo{.gate_idx = gate_idx,
                                             .time     = GateDelayNanoSec{sg_delay[i]},
                                             .error    = sg_err[i]});
        }
    }

    return device;
}

}  // namespace qsyn::device
