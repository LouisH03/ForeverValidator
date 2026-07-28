#ifndef TMNF_REPLAY_CONTROL_PLAN_H
#define TMNF_REPLAY_CONTROL_PLAN_H

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "format/replay/replay_ghost_trajectory.h"
#include "format/replay/replay_input_timeline.h"
#include "simulation/control/replay_control_timeline.h"
enum class ReplayControlPlanBuildResult {
    Success,
    InvalidRequest,
    TargetAllocationFailed,
    TargetTimeOutOfRange,
    NonFiniteTarget,
    TickReservationFailed,
    TickAllocationFailed,
    MissingComparisonTarget,
    MissingOutput,
};

struct ReplayControlPlanRequest {
    explicit ReplayControlPlanRequest(const ReplayInputTimeline &timeline)
        : inputTimeline(timeline) {}

    std::uint32_t controlTickMs = 0u;
    std::uint32_t requestedSamples = 0u;
    std::int32_t validationDurationMs = 0;
    std::uint32_t validationPrestartMs = 0u;
    std::uint32_t inputTimeBaseMs = 0u;
    ReplayRaceTransitionActions baseActions{};
    std::optional<std::int32_t> enableRaceSimulationAfterMs;
    std::optional<std::int32_t> establishRaceSpawnAtMs;
    bool appendUnobservedTrailingTick = false;
    std::optional<std::reference_wrapper<const ReplayGhostTrajectory>> trajectory;
    const ReplayInputTimeline &inputTimeline;

    bool IsInputOnlyValidation() const {
        return !trajectory.has_value() &&
               requestedSamples == 0u &&
               validationDurationMs >= 0;
    }
};

struct ReplayControlPlan {
    std::vector<ReplayControlTick> ticks;
    std::uint32_t comparisonTargetCount = 0u;
};

ReplayControlPlanBuildResult BuildReplayControlPlan(
        const ReplayControlPlanRequest &request,
        ReplayControlPlan *out);

#endif
