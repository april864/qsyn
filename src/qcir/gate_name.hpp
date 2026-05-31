/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Gate name helpers ]
  Author       [ Design Verification Lab ]
****************************************************************************/

#pragma once

#include <fmt/core.h>

#include <string>
#include <string_view>

namespace qsyn::qcir {

/// Full gate name: ``type`` or ``type(args)`` when ``args`` is non-empty.
inline std::string gate_repr(std::string_view gate_type, std::string_view args = {}) {
    if (args.empty()) {
        return std::string(gate_type);
    }
    return fmt::format("{}({})", gate_type, args);
}

}  // namespace qsyn::qcir
