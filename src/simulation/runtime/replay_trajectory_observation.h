#ifndef TMNF_REPLAY_TRAJECTORY_OBSERVATION_H
#define TMNF_REPLAY_TRAJECTORY_OBSERVATION_H

#include <cstdint>
#include <optional>

#include "engine/core/gm_types.h"
struct ReplayTrajectoryDeviation {
    GmVec3 targetPosition{};
    GmVec3 delta{};
    float distance = 0.0f;
};

struct ReplayTrajectoryObservation {
    GmVec3 simulatedPosition{};
    GmVec3 writePosition{};
    std::optional<ReplayTrajectoryDeviation> comparison;
    std::optional<std::uint32_t> finishTickMs;
};

#endif
