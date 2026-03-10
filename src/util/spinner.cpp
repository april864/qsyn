/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Implement spinner utility for terminal loading indicator. ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ Copyright(c) 2026 PARAG@N Lab, CS, Northwestern U, IL, USA ]
****************************************************************************/

#include "util/spinner.hpp"

#include <fmt/core.h>

#include <chrono>
#include <cstdio>
#include <thread>

#include "util/terminal_attributes.hpp"

namespace dvlab {
namespace utils {

namespace {

constexpr char const* spinner_chars = "|/-\\";
constexpr auto spin_interval        = std::chrono::milliseconds(80);
}  // namespace

Spinner::Spinner(std::string const& message)
    : _message(message), _stop(false) {
    if (!is_terminal(stderr)) {
        fmt::print(stderr, "{}\n", _message);
        return;
    }
    _thread = std::thread([this]() {
        size_t frame = 0;
        while (!_stop.load(std::memory_order_relaxed)) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            char const c = spinner_chars[frame % 4];
            fmt::print(stderr, "{} {} \r", _message, c);
            std::fflush(stderr);
            frame++;
            for (auto deadline = std::chrono::steady_clock::now() + spin_interval;
                 !_stop.load(std::memory_order_relaxed) &&
                 std::chrono::steady_clock::now() < deadline;) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    });
}

Spinner::~Spinner() {
    _stop.store(true, std::memory_order_relaxed);
    if (_thread.joinable()) {
        _thread.join();
        // clear the line
        fmt::print(stderr, "\r{}\r", std::string(_message.size() + 2, ' '));
        std::fflush(stderr);
    }
}

}  // namespace utils
}  // namespace dvlab
