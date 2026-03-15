/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Placer structure ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <memory>
#include <vector>

#include "duostra/device_state.hpp"
#include "duostra/duostra_def.hpp"
#include "qsyn/qsyn_type.hpp"

namespace qsyn::duostra {

class BasePlacer {
public:
    using DeviceState = qsyn::duostra::DeviceState;
    BasePlacer() {}
    virtual ~BasePlacer() = default;

    std::vector<QubitIdType> place_and_assign(DeviceState& device);

protected:
    virtual std::vector<QubitIdType> _place(DeviceState&) const = 0;
};

class RandomPlacer : public BasePlacer {
public:
    using DeviceState = BasePlacer::DeviceState;

protected:
    std::vector<QubitIdType> _place(DeviceState& /*unused*/) const override;
};

class StaticPlacer : public BasePlacer {
public:
    using DeviceState = BasePlacer::DeviceState;

protected:
    std::vector<QubitIdType> _place(DeviceState& /*unused*/) const override;
};

class DFSPlacer : public BasePlacer {
public:
    using DeviceState = BasePlacer::DeviceState;

protected:
    std::vector<QubitIdType> _place(DeviceState& /*unused*/) const override;

private:
    void _dfs_device(QubitIdType current, DeviceState& device, std::vector<QubitIdType>& assign, std::vector<bool>& qubit_marks) const;
};

std::unique_ptr<BasePlacer> get_placer(PlacerType type);

}  // namespace qsyn::duostra
