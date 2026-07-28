#include "simulation/backends/cuda/cuda_timeline_executor.h"

#include <cmath>

namespace forevervalidator::simulation {

CudaControlTick FlattenCudaControlTick(
        const ReplayControlTick &source) noexcept {
    CudaControlTick result;
    result.periodMs = source.periodMs;
    result.timeMs = source.timeMs;
    result.stuntsTimeLimitMs = source.actions.stuntsTimeLimitMs;
    result.respawnAtCheckpointCount =
            source.actions.respawnAtCheckpointCount;
    result.controls = source.controls;
    result.stuntsInput = source.stuntsInput;
    result.observe = source.observe;
    result.hasComparisonTarget =
            source.comparisonTarget.has_value();
    if (source.comparisonTarget.has_value()) {
        result.comparisonTarget = *source.comparisonTarget;
    }
#define SET_ACTION(field, flag)                                                \
    if (source.actions.field) result.actionFlags |= flag
    SET_ACTION(establishRaceSpawn,
               CudaControlActionEstablishRaceSpawn);
    SET_ACTION(suppressVehicleForceCallbacks,
               CudaControlActionSuppressVehicleForceCallbacks);
    SET_ACTION(enableRaceSimulation,
               CudaControlActionEnableRaceSimulation);
    SET_ACTION(resetAtRaceStart,
               CudaControlActionResetAtRaceStart);
    SET_ACTION(enableStuntsSimulation,
               CudaControlActionEnableStuntsSimulation);
    SET_ACTION(finishRace, CudaControlActionFinishRace);
#undef SET_ACTION
    return result;
}

const char *CudaTimelineStatusName(CudaTimelineStatus status) noexcept {
    switch (status) {
    case CudaTimelineStatus::Success: return "success";
    case CudaTimelineStatus::InvalidArgument:
        return "invalid_argument";
    case CudaTimelineStatus::SchemaMismatch:
        return "schema_mismatch";
    case CudaTimelineStatus::CapacityExceeded:
        return "capacity_exceeded";
    case CudaTimelineStatus::Cancelled: return "cancelled";
    case CudaTimelineStatus::DeviceFailure: return "device_failure";
    case CudaTimelineStatus::UnsupportedPhysicsTransition:
        return "unsupported_physics_transition";
    }
    return "unknown";
}

std::optional<std::size_t> SelectCudaTimelineWinner(
        const std::vector<CudaCandidateTimelineOutput> &candidates) noexcept {
    const auto finalDistance = [](const CudaCandidateTimelineOutput &candidate)
            -> std::optional<float> {
        for (std::size_t index = candidate.observations.size();
             index != 0u; --index) {
            const CudaTimelineObservation &observation =
                    candidate.observations[index - 1u];
            if (observation.hasComparison &&
                std::isfinite(observation.comparisonDistance)) {
                return observation.comparisonDistance;
            }
        }
        return std::nullopt;
    };
    const auto isBetter = [&](const CudaCandidateTimelineOutput &left,
                              const CudaCandidateTimelineOutput &right) {
        const bool stunts =
                left.finalState.race.replayPlayMode ==
                static_cast<std::uint32_t>(
                        EChallengePlayMode::Stunts);
        if (stunts &&
            left.finalState.stunts.stuntsScore !=
                    right.finalState.stunts.stuntsScore) {
            return left.finalState.stunts.stuntsScore >
                   right.finalState.stunts.stuntsScore;
        }
        const ReplayRaceProgress &leftRace =
                left.finalState.race.progress;
        const ReplayRaceProgress &rightRace =
                right.finalState.race.progress;
        if (leftRace.raceCompleted != rightRace.raceCompleted) {
            return leftRace.raceCompleted;
        }
        if (leftRace.completedLapCount != rightRace.completedLapCount) {
            return leftRace.completedLapCount >
                   rightRace.completedLapCount;
        }
        if (leftRace.totalCheckpointEventCount !=
            rightRace.totalCheckpointEventCount) {
            return leftRace.totalCheckpointEventCount >
                   rightRace.totalCheckpointEventCount;
        }
        if (leftRace.raceCompleted &&
            leftRace.lastPrepareTimeMs != rightRace.lastPrepareTimeMs) {
            return leftRace.lastPrepareTimeMs <
                   rightRace.lastPrepareTimeMs;
        }
        const std::optional<float> leftDistance = finalDistance(left);
        const std::optional<float> rightDistance = finalDistance(right);
        if (leftDistance.has_value() != rightDistance.has_value()) {
            return leftDistance.has_value();
        }
        if (leftDistance.has_value() &&
            *leftDistance != *rightDistance) {
            return *leftDistance < *rightDistance;
        }
        if (left.executedRespawnCount != right.executedRespawnCount) {
            return left.executedRespawnCount <
                   right.executedRespawnCount;
        }
        return left.finalState.candidateId <
               right.finalState.candidateId;
    };

    std::optional<std::size_t> winner;
    for (std::size_t index = 0u; index < candidates.size(); ++index) {
        if (candidates[index].status != CudaTimelineStatus::Success) {
            continue;
        }
        if (!winner.has_value() ||
            isBetter(candidates[index], candidates[*winner])) {
            winner = index;
        }
    }
    return winner;
}

#if !FOREVERVALIDATOR_HAS_CUDA
CudaTimelineBatchResult ExecuteCudaTimelineBatch(
        const void *,
        const void *,
        const std::vector<CudaCandidateTimelineInput> &,
        bool) noexcept {
    CudaTimelineBatchResult result;
    result.status = CudaTimelineStatus::DeviceFailure;
    result.diagnostic =
            "CUDA timeline execution unavailable in a CPU-only build";
    return result;
}
#endif

}  // namespace forevervalidator::simulation
