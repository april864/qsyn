/****************************************************************************
  PackageName  [ device ]
  Synopsis     [ Define class IBMQDevices and fetching functions ]
  Author       [ Mu-Te (Joshua) Lau]
  Copyright    [ Copyright(c) 2026 PARAG@N Lab, CS, Northwestern U, IL, USA ]
****************************************************************************/

#include "ibmq_devices.hpp"

#include <date/date.h>
#include <spdlog/spdlog.h>

#include <array>
#include <chrono>
#include <ctime>
#include <string>
#include <string_view>

#include "util/sysdep.hpp"
#include "util/tmp_files.hpp"

namespace qsyn::device {

namespace {

// Parse ISO8601 datetime (e.g. "2026-02-15T14:29:39-06:00" or "2026-02-15T20:29:39Z") into UTC.
auto parse_iso8601(std::string s) -> std::chrono::sys_seconds {
    if (!s.empty() && s.back() == 'Z') {
        s.pop_back();
        s += "+0000";
    }
    // date::parse %z expects offset without colon (e.g. -0600 not -06:00)
    if (s.size() >= 6 && s[s.size() - 3] == ':')
        s.erase(s.size() - 3, 1);

    std::istringstream in{s};
    std::chrono::sys_seconds tp;

    in >> date::parse("%FT%T%z", tp);

    if (in.fail())
        throw std::runtime_error("Invalid ISO8601 datetime");

    return tp;
}

auto format_iso8601_utc(std::chrono::sys_seconds tp) -> std::string {
    return date::format("%Y-%m-%d %H:%M:%S", tp) + " UTC";
}

// Format UTC time for display in the user's local timezone.
auto format_iso8601_local(std::chrono::sys_seconds tp) -> std::string {
    auto const secs = tp.time_since_epoch().count();
    auto t          = static_cast<std::time_t>(secs);
    if (static_cast<std::chrono::sys_seconds::rep>(t) != secs)
        return format_iso8601_utc(tp);  // overflow, fallback to UTC
    std::tm local{};

    // try to retrieve the local time zone
#if defined(_WIN32)
    if (localtime_s(&local, &t) != 0)
        return format_iso8601_utc(tp);
#else
    if (localtime_r(&t, &local) == nullptr)
        return format_iso8601_utc(tp);
#endif
    std::array<char, 32> buf{};
    // try to format the local time
    // if this returns 0, it either means the local time formatting is invalid
    // or there is no specialized printing for the time zone
    if (std::strftime(buf.data(), buf.size(), "%Y-%m-%d %H:%M:%S", &local) == 0)
        return format_iso8601_utc(tp);
    return std::string{buf.data()};
}

auto find_gate_parameter(nlohmann::json const& gate, std::string const& name)
    -> std::optional<float> {
    for (auto const& param : gate["parameters"]) {
        if (param["name"].get<std::string>() == name) {
            return param["value"].get<float>();
        }
    }
    return std::nullopt;
}

void parse_basic_info(IBMQDeviceJsons const& jsons, IBMQDevice& device) {
    auto const& [source, device_json, properties_json] = jsons;

    device.json_source = source;

    device.set_name(device_json["backend_name"].get<std::string>());
    device.backend_version  = device_json["backend_version"].get<std::string>();
    device.last_update_time = parse_iso8601(properties_json["last_update_date"]["__value__"].get<std::string>());
}

void parse_gates(IBMQDeviceJsons const& jsons, IBMQDevice& device) {
    auto const& [source, device_json, properties_json] = jsons;

    // parse gate set
    std::unordered_map<std::string, size_t> gate_name_to_idx;
    size_t gate_idx = 0;

    // parse per gate delay and error
    for (auto const& gate : properties_json["gates"]) {
        // NOTE: the device_json basis gate list is not reliable at all
        // We just parse the gate types by walking through the properties_json
        auto const gate_name = gate["gate"].get<std::string>();
        if (!gate_name_to_idx.contains(gate_name)) {
            device.add_gate_type(gate_name);
            gate_name_to_idx.emplace(gate_name, gate_idx);
            gate_idx++;
        }
        auto const gate_idx = gate_name_to_idx.at(gate_name);
        GateInfo gate_info{
            .gate_idx = gate_idx,
            .time     = GateDelayNanoSec{0.0f},
            .error    = 0.0f};

        if (auto gate_error = find_gate_parameter(gate, "gate_error")) {
            gate_info.error = *gate_error;
        }
        if (auto gate_delay = find_gate_parameter(gate, "gate_delay")) {
            gate_info.time = GateDelayNanoSec{*gate_delay};
        }

        auto const qubits = gate["qubits"].get<std::vector<size_t>>();
        if (qubits.size() == 1) {
            device.add_gate_info(qubits[0], gate_info);
        } else if (qubits.size() == 2) {
            device.add_gate_info(std::make_pair(qubits[0], qubits[1]), gate_info);
        }
    }
}

}  // namespace

auto read_ibmq_device(IBMQDeviceJsons const& jsons)
    -> std::optional<IBMQDevice> {
    IBMQDevice device;

    try {
        parse_basic_info(jsons, device);
        parse_gates(jsons, device);
    } catch (const nlohmann::json::exception& e) {
        spdlog::error("Failed to parse IBMQ device: {}", e.what());
        return std::nullopt;
    }

    return device;
}

auto IBMQDevice::info_string() const -> std::string {
    std::string source_str;
    switch (json_source) {
        case IBMQDeviceJsonsSource::real:
            source_str = "realtime";
            break;
        case IBMQDeviceJsonsSource::cached:
            source_str = "cache";
            break;
        case IBMQDeviceJsonsSource::fake:
            source_str = "fake backend";
            break;
        case IBMQDeviceJsonsSource::unknown:
        default:
            source_str = "unknown";
            break;
    }
    auto result = fmt::format("IBMQ Device: {} ({} qubits)\n",
                              get_name(),
                              get_num_qubits());
    result += fmt::format("- Backend version: {}\n", backend_version);
    result += fmt::format("- Last updated: {}\n", format_iso8601_local(last_update_time));
    result += fmt::format("- Source: {}\n", source_str);
    result += fmt::format("- Gate set: [{}]\n", fmt::join(get_gate_set(), ", "));

    return result;
}

auto load_ibmq_devices_jsons(std::filesystem::path const& device_path,
                             std::filesystem::path const& properties_path,
                             IBMQDeviceJsonsSource source)
    -> std::optional<IBMQDeviceJsons> {
    if (!std::filesystem::exists(device_path)) {
        spdlog::error("Device file {} does not exist", device_path.string());
        return std::nullopt;
    }
    if (!std::filesystem::exists(properties_path)) {
        spdlog::error("Properties file {} does not exist", properties_path.string());
        return std::nullopt;
    }
    try {
        return IBMQDeviceJsons{
            .source          = source,
            .device_json     = nlohmann::json::parse(std::ifstream(device_path)),
            .properties_json = nlohmann::json::parse(std::ifstream(properties_path)),
        };
    } catch (const nlohmann::json::exception& e) {
        spdlog::error("Failed to parse JSON file {}: {}", device_path.string(), e.what());
        return std::nullopt;
    }
}

/**
 * Fetch the attributes of an IBM backend and save them to nlohmann::json objects
 * @param backend_name the name of the IBM backend
 * @param fake whether to use a fake backend
 * @return the attributes of the IBM backend
 */
auto fetch_ibmq_device_attrs(std::string_view backend_name, bool fake)
    -> std::optional<IBMQDeviceJsons> {
    auto const tmp_dir = dvlab::utils::TmpDir();

    auto const home_dir = dvlab::utils::get_home_directory();
    if (!home_dir) {
        spdlog::critical("Cannot find home directory");
        return std::nullopt;
    }

    auto const query_backend_name = [&]() {
        auto base = backend_name.starts_with("ibm_") ? backend_name.substr(4) : backend_name;
        if (fake && base.starts_with("fake_"))
            base = base.substr(5);
        return fake ? "fake_" + std::string(base) : "ibm_" + std::string(base);
    }();

    auto const device_file_path       = query_backend_name + ".json";
    auto const device_file_props_path = query_backend_name + "_properties.json";

    auto const script_path = "scripts/get_ibm_backend_attrs.py";
    auto args              = std::vector<std::string>{
        query_backend_name, "-o", tmp_dir.path().string()};
    if (fake) {
        args.push_back("-f");
    }
    auto const result = dvlab::utils::uv_run_script(script_path, args);

    if (result != 0) {
        return std::nullopt;
    }

    return load_ibmq_devices_jsons(
        tmp_dir.path() / device_file_path,
        tmp_dir.path() / device_file_props_path,
        fake
            ? IBMQDeviceJsonsSource::fake
            : IBMQDeviceJsonsSource::real);
}

auto fetch_ibmq_device_attrs_with_fallback(
    std::string_view backend_name,
    bool fake,
    bool cached,
    std::optional<std::filesystem::path> const& cache_dir)
    -> std::optional<IBMQDeviceJsons> {
    auto const backend_name_without_ibm =
        backend_name.starts_with("ibm_") ? backend_name.substr(4) : backend_name;
    auto const real_backend_name = "ibm_" + std::string(backend_name_without_ibm);
    auto const fake_backend_name = "fake_" + std::string(backend_name_without_ibm);

    if (fake) {
        return fetch_ibmq_device_attrs(fake_backend_name, true);
    }

    if (cache_dir.has_value()) {
        if (cache_dir->empty()) {
            spdlog::error("Cache dir path is empty");
            return std::nullopt;
        }
        if (std::filesystem::exists(*cache_dir) &&
            !std::filesystem::is_directory(*cache_dir)) {
            spdlog::error("Cache dir is not a directory: {}",
                          cache_dir->string());
            return std::nullopt;
        }
    }

    auto const device_file_path       = real_backend_name + ".json";
    auto const device_file_props_path = real_backend_name + "_properties.json";

    if (cached && cache_dir.has_value()) {
        auto const device_path = *cache_dir / device_file_path;
        auto const props_path  = *cache_dir / device_file_props_path;
        if (std::filesystem::exists(device_path) &&
            std::filesystem::exists(props_path)) {
            return load_ibmq_devices_jsons(
                device_path, props_path, IBMQDeviceJsonsSource::cached);
        }
    }

    auto tmp_dir                  = dvlab::utils::TmpDir();
    constexpr auto script_path    = "scripts/get_ibm_backend_attrs.py";
    std::vector<std::string> args = {real_backend_name, "-o",
                                     tmp_dir.path().string()};

    if (dvlab::utils::uv_run_script(script_path, args) == 0) {
        auto result = load_ibmq_devices_jsons(
            tmp_dir.path() / device_file_path,
            tmp_dir.path() / device_file_props_path,
            IBMQDeviceJsonsSource::real);
        if (result && cache_dir.has_value()) {
            std::filesystem::create_directories(*cache_dir);
            std::filesystem::copy(
                tmp_dir.path() / device_file_path,
                *cache_dir / device_file_path,
                std::filesystem::copy_options::overwrite_existing);
            std::filesystem::copy(
                tmp_dir.path() / device_file_props_path,
                *cache_dir / device_file_props_path,
                std::filesystem::copy_options::overwrite_existing);
        }
        return result;
    }

    if (cached) {
        // if cached is true, we have already checked
        return std::nullopt;
    }

    if (cache_dir.has_value()) {
        auto const cached_device_path = *cache_dir / device_file_path;
        auto const cached_props_path  = *cache_dir / device_file_props_path;
        if (std::filesystem::exists(cached_device_path) &&
            std::filesystem::exists(cached_props_path)) {
            return load_ibmq_devices_jsons(
                cached_device_path, cached_props_path,
                IBMQDeviceJsonsSource::cached);
        }
    }

    auto fake_result = fetch_ibmq_device_attrs(fake_backend_name, true);
    if (fake_result) {
        return fake_result;
    }

    return std::nullopt;
}

}  // namespace qsyn::device
