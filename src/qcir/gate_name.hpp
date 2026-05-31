/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Gate name helpers (base name without phase arguments) ]
  Author       [ Design Verification Lab ]
****************************************************************************/

#pragma once

#include <string>
#include <string_view>

namespace qsyn::qcir {

/// Gate name without ``(...)`` phase arguments (e.g. ``rz(π/2)`` → ``rz``).
inline std::string gate_base_name(std::string_view gate_name) {
    auto const pos = gate_name.find('(');
    if (pos == std::string_view::npos) {
        return std::string(gate_name);
    }
    return std::string(gate_name.substr(0, pos));
}

}  // namespace qsyn::qcir
