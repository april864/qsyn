/****************************************************************************
  PackageName  [ device ]
  Synopsis     [ Define spinner utility. ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ Copyright(c) 2026 PARAG@N Lab, CS, Northwestern U, IL, USA ]
****************************************************************************/

#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <utility>

namespace dvlab {
namespace utils {

class Spinner {
public:
    Spinner(std::string const& message = "", std::string const& done_message = "");
    ~Spinner();

private:
    std::string _message;
    std::string _done_message;
    std::atomic<bool> _stop{false};
    std::thread _thread;
};

template <typename F>
auto with_spinner(F&& call, std::string const& message = "",
                  std::string const& done_message = "") -> decltype(std::forward<F>(call)()) {
    Spinner spinner(message, done_message);
    return std::forward<F>(call)();
}

};  // namespace utils
}  // namespace dvlab
