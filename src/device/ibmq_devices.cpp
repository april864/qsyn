/****************************************************************************
  PackageName  [ device ]
  Synopsis     [ Define class IBMQDevices and fetching functions ]
  Author       [ Mu-Te (Joshua) Lau]
  Copyright    [ Copyright(c) 2026 PARAG@N Lab, CS, Northwestern U, IL, USA ]
****************************************************************************/

#include "ibmq_devices.hpp"

#include <spdlog/spdlog.h>

#include <string>
#include <string_view>

#include "util/sysdep.hpp"
#include "util/tmp_files.hpp"

namespace qsyn::device {

auto read_ibmq_devices_jsons(std::filesystem::path const& device_path,
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
        spdlog::error("Cannot find home directory");
        return std::nullopt;
    }

    auto const query_backend_name = [&]() {
        auto const backend_name_without_ibm = backend_name.starts_with("ibm_") ? backend_name.substr(4) : backend_name;
        return fake
                   ? "fake_" + std::string(backend_name_without_ibm)
                   : "ibm_" + std::string(backend_name_without_ibm);
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
        spdlog::error("Failed to get IBM backend attributes for {}", query_backend_name);
        return std::nullopt;
    }

    return read_ibmq_devices_jsons(
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
            return read_ibmq_devices_jsons(
                device_path, props_path, IBMQDeviceJsonsSource::cached);
        }
    }

    auto tmp_dir                  = dvlab::utils::TmpDir();
    constexpr auto script_path    = "scripts/get_ibm_backend_attrs.py";
    std::vector<std::string> args = {real_backend_name, "-o",
                                     tmp_dir.path().string()};

    if (dvlab::utils::uv_run_script(script_path, args) == 0) {
        auto result = read_ibmq_devices_jsons(
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
            return read_ibmq_devices_jsons(
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
