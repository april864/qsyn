/****************************************************************************
  PackageName  [ device ]
  Synopsis     [ Define class IBMQDevices and fetching functions ]
  Author       [ Mu-Te (Joshua) Lau]
  Copyright    [ Copyright(c) 2026 PARAG@N Lab, CS, Northwestern U, IL, USA ]
****************************************************************************/

#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string_view>

#include "device/device.hpp"

namespace qsyn::device {

enum struct IBMQDeviceJsonsSource : std::uint8_t {
    real,
    fake,
    cached,
    unknown,
};

struct IBMQDeviceJsons {
    IBMQDeviceJsonsSource source;
    nlohmann::json device_json;
    nlohmann::json properties_json;
};

struct IBMQDevice : public Device {
    std::string backend_version;
    std::chrono::sys_seconds last_update_time;
    IBMQDeviceJsonsSource json_source;

    std::string info_string() const override;
};

auto read_ibmq_device(IBMQDeviceJsons const& jsons) -> std::optional<IBMQDevice>;

auto load_ibmq_devices_jsons(std::filesystem::path const& device_path,
                             std::filesystem::path const& properties_path,
                             IBMQDeviceJsonsSource source = IBMQDeviceJsonsSource::unknown)
    -> std::optional<IBMQDeviceJsons>;

auto fetch_ibmq_device_attrs(std::string_view backend_name, bool fake = false)
    -> std::optional<IBMQDeviceJsons>;

auto fetch_ibmq_device_attrs_with_fallback(
    std::string_view backend_name,
    bool fake                                             = false,
    bool cached                                           = false,
    std::optional<std::filesystem::path> const& cache_dir = std::nullopt)
    -> std::optional<IBMQDeviceJsons>;

}  // namespace qsyn::device
