/****************************************************************************
  PackageName  [ device ]
  Synopsis     [ Floyd-Warshall and device analysis utilities ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <functional>
#include <limits>
#include <optional>
#include <vector>

#include "device/device.hpp"
#include "qsyn/qsyn_type.hpp"

namespace qsyn::device {

struct APSPResult {
    std::vector<std::vector<std::optional<QubitIdType>>> predecessor;
    std::vector<std::vector<std::optional<float>>> distance;
};

using APSPCostFnType = std::function<float(Device::QubitPair const&, Device const&)>;

float default_floyd_warshall_cost(Device::QubitPair const& /*adj*/, Device const& /*device*/);
float log_success_rate_floyd_warshall_cost(Device::QubitPair const& adj, Device const& device);

APSPResult floyd_warshall(
    Device const& device,
    APSPCostFnType const& cost_fn = default_floyd_warshall_cost);

std::vector<float> get_eccentricities(APSPResult const& apsp, Device const& device);

std::vector<QubitIdType> get_centers(std::vector<float> const& eccentricities);

std::vector<QubitIdType> get_centers(APSPResult const& apsp, Device const& device);

std::optional<std::vector<QubitIdType>> get_shortest_path(APSPResult const& apsp, QubitIdType src, QubitIdType dest);

}  // namespace qsyn::device
