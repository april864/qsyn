/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define wrapper to system-dependent functions ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ Copyright(c) 2026 PARAG@N Lab, CS, Northwestern U, IL, USA ]
****************************************************************************/

#include "util/sysdep.hpp"

#include <limits.h>

#include <array>

#include "spdlog/spdlog.h"
#ifdef __linux__
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace dvlab {

namespace utils {

bool python_package_exists(std::string_view package_name) {
    if (!is_uv_available()) {
        spdlog::error("`uv` is required to for this command. Please install `uv` first.");
        return false;
    }
    return system(fmt::format("uv pip show {} > /dev/null 2>&1", package_name).c_str()) == 0;
}

std::filesystem::path get_qsyn_executable_dir() {
#ifdef __linux__
    std::array<char, PATH_MAX> path{};
    ssize_t len = readlink("/proc/self/exe", path.data(), path.size() - 1);
    if (len == -1) {
        return {};
    }
    path[static_cast<size_t>(len)] = '\0';
    return std::filesystem::path(path.data()).parent_path();
#elif defined(__APPLE__)
    std::array<char, PATH_MAX> path{};
    uint32_t size = static_cast<uint32_t>(path.size());
    if (_NSGetExecutablePath(path.data(), &size) != 0) {
        return {};
    }
    return std::filesystem::path(path.data()).parent_path();
#elif defined(_WIN32)
    std::array<char, MAX_PATH> path{};
    if (GetModuleFileNameA(nullptr, path.data(), static_cast<DWORD>(path.size())) == 0) {
        return {};
    }
    return std::filesystem::path(path.data()).parent_path();
#else
    return {};
#endif
}

std::optional<std::filesystem::path> get_qsyn_config_dir() {
    auto home_dir = get_home_directory();
    if (!home_dir) {
        return std::nullopt;
    }
    return home_dir.value() + "/.config/qsyn/";
}

bool is_uv_available() {
    return system("uv --version > /dev/null 2>&1") == 0;
}

int uv_run_script(std::string_view script_path, std::vector<std::string> args) {
    if (!is_uv_available()) {
        spdlog::error(
            "`uv` is required to for this command. Please install `uv` first."
            "See `https://docs.astral.sh/uv/getting-started/installation/` for installation instructions.");
        return 1;
    }

    auto const executable_dir = get_qsyn_executable_dir();

    if (!std::filesystem::exists(executable_dir / ".venv")) {
        spdlog::warn("No uv venv found. A new one will be created...");
        // NOTE: `uv run` will try to create a venv if it doesn't exist.
        // We don't need to create it manually. The warning is just to
        // inform the user.
    }

    return system(fmt::format("uv run --project {} {} {}", executable_dir.string(), script_path, fmt::join(args, " ")).c_str());
}
}  // namespace utils

}  // namespace dvlab
