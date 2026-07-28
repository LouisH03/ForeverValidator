#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "simulation/control/replay_control_plan.h"
#include "engine/core/finite_values.h"
#include <new>
namespace {

using u32 = std::uint32_t;
using s32 = std::int32_t;

struct ReplayControlTimeline {
    u32 controlTickMs = 0u;
    u32 targetCount = 0u;
    const ReplayInputTimeline *inputTimeline = nullptr;
    s32 validationDurationMs = 0;
    u32 validationPrestartMs = 0u;
    u32 inputTimeBaseMs = 0u;
    ReplayRaceTransitionActions baseActions{};
    std::optional<s32> enableRaceSimulationAfterMs;
    std::optional<s32> establishRaceSpawnAtMs;
    bool appendUnobservedTrailingTick = false;
};

struct ReplayControlTarget {
    s32 timeMs = 0;
    std::optional<GmVec3> comparisonTarget;
};

struct ReplayControlState {
    bool raceRunning = false;
    bool accelerate = false;
    bool brake = false;
    bool steerRight = false;
    bool steerLeft = false;
    s32 steerLeftTime = 0;
    s32 steerRightTime = 0;
    s32 steerTime = 0;
    forevervalidator::AnalogInputState steerValue = 0;
    s32 accelerateTime = 0;
    s32 brakeTime = 0;
    s32 gasTime = 0;
    forevervalidator::AnalogInputState gasValue = 0;
    std::array<u32,
               static_cast<std::size_t>(ReplayStuntInputAction::Count)>
            stuntInputLastChangeTimeMs{};
};

std::optional<std::size_t> StuntInputIndex(
        ReplayInputActionKind action) {
    switch (action) {
    case ReplayInputActionKind::SteerLeft:
        return static_cast<std::size_t>(
                ReplayStuntInputAction::SteerLeft);
    case ReplayInputActionKind::SteerRight:
        return static_cast<std::size_t>(
                ReplayStuntInputAction::SteerRight);
    case ReplayInputActionKind::Steer:
        return static_cast<std::size_t>(ReplayStuntInputAction::Steer);
    case ReplayInputActionKind::Accelerate:
        return static_cast<std::size_t>(
                ReplayStuntInputAction::Accelerate);
    case ReplayInputActionKind::Brake:
        return static_cast<std::size_t>(ReplayStuntInputAction::Brake);
    case ReplayInputActionKind::Gas:
        return static_cast<std::size_t>(ReplayStuntInputAction::Gas);
    default:
        return std::nullopt;
    }
}

void ApplyEvent(ReplayControlState &state, const ReplayInputEvent &event) {
    const bool active = event.value.IsActive();
    const s32 time = static_cast<s32>(event.timeMs);
    const std::optional<std::size_t> stuntInputIndex =
            StuntInputIndex(event.action);
    if (stuntInputIndex.has_value()) {
        state.stuntInputLastChangeTimeMs[*stuntInputIndex] = event.timeMs;
    }
    switch (event.action) {
    case ReplayInputActionKind::Accelerate:
        state.accelerate = active;
        state.accelerateTime = time;
        break;
    case ReplayInputActionKind::Gas:
        state.gasValue = event.value.AnalogValue();
        state.gasTime = time;
        break;
    case ReplayInputActionKind::Brake:
        state.brake = active;
        state.brakeTime = time;
        break;
    case ReplayInputActionKind::Steer:
        state.steerValue = event.value.AnalogValue();
        state.steerTime = time;
        break;
    case ReplayInputActionKind::SteerLeft:
        state.steerLeft = active;
        state.steerLeftTime = time;
        break;
    case ReplayInputActionKind::SteerRight:
        state.steerRight = active;
        state.steerRightTime = time;
        break;
    case ReplayInputActionKind::RaceRunning:
        state.raceRunning = active;
        break;
    case ReplayInputActionKind::Respawn:
        break;
    case ReplayInputActionKind::FinishLine:
    case ReplayInputActionKind::Unmapped:
    default:
        break;
    }
}

ReplayStuntInputState StuntInputStateFromControlState(
        const ReplayControlState &state,
        const ReplayControlTimeline &timeline) {
    ReplayStuntInputState result;
    for (std::size_t index = 0u;
         index < result.lastChangeTimeMs.size();
         ++index) {
        const u32 inputTime = state.stuntInputLastChangeTimeMs[index];
        const std::uint64_t translated =
                static_cast<std::uint64_t>(timeline.validationPrestartMs) +
                inputTime;
        if (translated <= timeline.inputTimeBaseMs) {
            result.lastChangeTimeMs[index] = 0u;
            continue;
        }
        const std::uint64_t simulationTime =
                translated - timeline.inputTimeBaseMs;
        result.lastChangeTimeMs[index] = simulationTime <= UINT32_MAX
                ? static_cast<u32>(simulationTime)
                : UINT32_MAX;
    }
    return result;
}

ReplayVehicleControlState ControlsFromState(const ReplayControlState &state) {
    float steering = 0.0f;
    const s32 digitalSteerTime = ((state.steerLeftTime) > (state.steerRightTime) ? (state.steerLeftTime) : (state.steerRightTime));
    const bool analogSteeringWins =
            state.steerTime > digitalSteerTime ||
            (state.steerTime == digitalSteerTime &&
             !state.steerLeft && !state.steerRight &&
             std::abs(state.steerValue) > 655);
    if (analogSteeringWins) {
        steering = static_cast<float>(state.steerValue) /
                static_cast<float>(forevervalidator::kAnalogInputScale);
    } else if (state.steerLeft) {
        steering = -1.0f;
    } else if (state.steerRight) {
        steering = 1.0f;
    }

    float lowSpeedGateA = 0.0f;
    float lowSpeedGateB = 0.0f;
    const s32 digitalGateTime = ((state.accelerateTime) > (state.brakeTime) ? (state.accelerateTime) : (state.brakeTime));
    const bool analogGasWins =
            state.gasTime > digitalGateTime ||
            (state.gasTime == digitalGateTime &&
             !state.accelerate && !state.brake);
    if (analogGasWins) {
        if (state.gasValue <= -19661) {
            lowSpeedGateA = 1.0f;
        } else if (state.gasValue >= 19661) {
            lowSpeedGateB = 1.0f;
        }
    } else {
        lowSpeedGateA = state.accelerate ? 1.0f : 0.0f;
        lowSpeedGateB = state.brake ? 1.0f : 0.0f;
    }

    return {lowSpeedGateA, lowSpeedGateB, steering};
}

bool AtOrAfter(s32 timeMs, const std::optional<s32> &thresholdMs) {
    return thresholdMs.has_value() && timeMs >= *thresholdMs;
}

bool TargetsAreOrdered(const std::vector<ReplayControlTarget> &targets) {
    for (std::size_t i = 1u; i < targets.size(); ++i) {
        if (targets[i].timeMs < targets[i - 1u].timeMs) {
            return false;
        }
    }
    return true;
}

bool TargetIsFinite(const ReplayControlTarget &target) {
    return !target.comparisonTarget.has_value() ||
           FiniteValues::IsFinite(*target.comparisonTarget);
}

ReplayControlPlanBuildResult BuildTimelinePlan(
        const ReplayControlTimeline &timeline,
        const std::vector<ReplayControlTarget> &targets,
        ReplayControlPlan *out) {
    if (out == nullptr || targets.empty()) {
        return ReplayControlPlanBuildResult::MissingOutput;
    }
    out->ticks.clear();
    out->comparisonTargetCount = 0u;

    ReplayControlState state;
    std::size_t eventCursor = 0u;
    std::size_t endpointCursor = 0u;
    bool previousRaceRunning = false;
    bool raceStartResetDone = false;
    bool raceSpawnEstablished = false;
    s32 finalTarget = targets.back().timeMs;
    std::optional<s32> finalObservationTickMs;
    if (timeline.validationDurationMs >= 0) {
        finalObservationTickMs =
                static_cast<s32>(timeline.validationPrestartMs) +
                timeline.validationDurationMs;
        if (finalTarget < *finalObservationTickMs) {
            finalTarget = *finalObservationTickMs;
        }
    }

    const std::optional<s32> raceSpawnMs =
            timeline.establishRaceSpawnAtMs.has_value()
                    ? timeline.establishRaceSpawnAtMs
                    : timeline.enableRaceSimulationAfterMs;
    const u32 capacity =
            static_cast<u32>(finalTarget /
                             static_cast<s32>(timeline.controlTickMs)) +
            1u;
    try {
        out->ticks.reserve(capacity);
    } catch (const std::bad_alloc &) {
        return ReplayControlPlanBuildResult::TickReservationFailed;
    }

    for (s32 timeMs = static_cast<s32>(timeline.controlTickMs);
         timeMs <= finalTarget;
         timeMs += static_cast<s32>(timeline.controlTickMs)) {
        const u32 inputSampleTime =
                timeline.inputTimeBaseMs - timeline.validationPrestartMs +
                static_cast<u32>(timeMs);
        const std::vector<ReplayInputEvent> *events =
                timeline.inputTimeline != nullptr
                        ? &timeline.inputTimeline->Events()
                        : nullptr;
        u32 respawnEventCount = 0u;
        bool finishRace = false;
        while (events != nullptr &&
               eventCursor < events->size() &&
               (*events)[eventCursor].timeMs <= inputSampleTime) {
            const ReplayInputEvent &event = (*events)[eventCursor];
            if (event.action == ReplayInputActionKind::Respawn &&
                event.value.IsActive()) {
                ++respawnEventCount;
            }
            if (event.action == ReplayInputActionKind::FinishLine &&
                event.value.IsCanonicalPress()) {
                finishRace = true;
            }
            ApplyEvent(state, event);
            ++eventCursor;
        }

        while (endpointCursor + 1u < targets.size() &&
               targets[endpointCursor].timeMs < timeMs) {
            ++endpointCursor;
        }
        const ReplayControlTarget *endpoint = nullptr;
        if (endpointCursor < targets.size() &&
            targets[endpointCursor].timeMs == timeMs) {
            endpoint = &targets[endpointCursor];
            ++endpointCursor;
        }
        const bool forceObservation =
                endpoint == nullptr &&
                finalObservationTickMs.has_value() &&
                timeMs == *finalObservationTickMs;

        ReplayRaceTransitionActions actions = timeline.baseActions;
        actions.finishRace = finishRace;
        if (!raceSpawnEstablished && AtOrAfter(timeMs, raceSpawnMs)) {
            actions.establishRaceSpawn = true;
            raceSpawnEstablished = true;
        }
        if (AtOrAfter(timeMs, timeline.enableRaceSimulationAfterMs)) {
            actions.enableRaceSimulation = true;
            actions.respawnAtCheckpointCount = respawnEventCount;
            if (!raceStartResetDone &&
                !previousRaceRunning &&
                state.raceRunning) {
                actions.resetAtRaceStart = true;
                raceStartResetDone = true;
            }
        }
        previousRaceRunning = state.raceRunning;

        ReplayControlTick tick;
        tick.periodMs = timeline.controlTickMs;
        tick.timeMs = static_cast<u32>(timeMs);
        tick.observe = endpoint != nullptr || forceObservation;
        tick.actions = actions;
        tick.controls = ControlsFromState(state);
        tick.stuntsInput = StuntInputStateFromControlState(state, timeline);
        if (endpoint != nullptr) {
            tick.comparisonTarget = endpoint->comparisonTarget;
            ++out->comparisonTargetCount;
        }
        try {
            out->ticks.push_back(std::move(tick));
        } catch (const std::bad_alloc &) {
            return ReplayControlPlanBuildResult::TickAllocationFailed;
        }
    }

    if (timeline.appendUnobservedTrailingTick) {
        if (out->ticks.empty()) {
            return ReplayControlPlanBuildResult::MissingComparisonTarget;
        }
        ReplayControlTick trailing = out->ticks.back();
        constexpr u32 maxTickTime =
                static_cast<u32>(std::numeric_limits<s32>::max());
        if (trailing.periodMs > maxTickTime ||
            trailing.timeMs > maxTickTime - trailing.periodMs) {
            return ReplayControlPlanBuildResult::TargetTimeOutOfRange;
        }
        trailing.timeMs += trailing.periodMs;
        trailing.observe = false;
        trailing.comparisonTarget.reset();
        trailing.actions.establishRaceSpawn = false;
        trailing.actions.resetAtRaceStart = false;
        trailing.actions.finishRace = false;
        trailing.actions.respawnAtCheckpointCount = 0u;
        try {
            out->ticks.push_back(std::move(trailing));
        } catch (const std::bad_alloc &) {
            return ReplayControlPlanBuildResult::TickAllocationFailed;
        }
    }

    if (out->comparisonTargetCount != timeline.targetCount) {
        return ReplayControlPlanBuildResult::MissingComparisonTarget;
    }
    return ReplayControlPlanBuildResult::Success;
}

}  // namespace

ReplayControlPlanBuildResult BuildReplayControlPlan(
        const ReplayControlPlanRequest &request,
        ReplayControlPlan *out) {
    const bool inputOnlyValidation = request.IsInputOnlyValidation();
    if (out == nullptr ||
        request.controlTickMs == 0u ||
        (!inputOnlyValidation &&
         (!request.trajectory.has_value() ||
          request.trajectory->get().Empty() ||
          request.requestedSamples == 0u ||
          request.requestedSamples >
                  request.trajectory->get().SampleCount()))) {
        return ReplayControlPlanBuildResult::InvalidRequest;
    }
    const u32 targetCount =
            inputOnlyValidation ? 1u : request.requestedSamples;

    ReplayControlTimeline timeline;
    timeline.controlTickMs = request.controlTickMs;
    timeline.targetCount = targetCount;
    timeline.inputTimeline = &request.inputTimeline;
    timeline.validationDurationMs = request.validationDurationMs;
    timeline.validationPrestartMs = request.validationPrestartMs;
    timeline.inputTimeBaseMs = request.inputTimeBaseMs;
    timeline.baseActions = request.baseActions;
    timeline.enableRaceSimulationAfterMs = request.enableRaceSimulationAfterMs;
    timeline.establishRaceSpawnAtMs = request.establishRaceSpawnAtMs;
    timeline.appendUnobservedTrailingTick =
            request.appendUnobservedTrailingTick;

    std::vector<ReplayControlTarget> targets;
    try {
        targets.resize(targetCount);
    } catch (const std::bad_alloc &) {
        return ReplayControlPlanBuildResult::TargetAllocationFailed;
    }

    if (inputOnlyValidation) {
        targets[0].timeMs =
                static_cast<s32>(request.validationPrestartMs) +
                request.validationDurationMs;
    } else {
        for (u32 i = 0u; i < targetCount; ++i) {
            const std::size_t sampleIndex = static_cast<std::size_t>(i);
            const std::size_t sampleTimeMs =
                    request.trajectory->get().SampleTimeMs(sampleIndex);
            const std::uint64_t compareClockTime =
                    static_cast<std::uint64_t>(request.validationPrestartMs) +
                    sampleTimeMs;
            if (sampleTimeMs >
                        static_cast<std::size_t>(
                                std::numeric_limits<s32>::max()) ||
                compareClockTime >
                        static_cast<std::uint64_t>(
                                std::numeric_limits<s32>::max())) {
                return ReplayControlPlanBuildResult::TargetTimeOutOfRange;
            }
            targets[i].timeMs = static_cast<s32>(compareClockTime);
            targets[i].comparisonTarget =
                    request.trajectory->get().PositionAt(sampleIndex);
        }
    }

    if (!TargetsAreOrdered(targets)) {
        return ReplayControlPlanBuildResult::TargetTimeOutOfRange;
    }
    for (const ReplayControlTarget &target : targets) {
        if (!TargetIsFinite(target)) {
            return ReplayControlPlanBuildResult::NonFiniteTarget;
        }
    }
    return BuildTimelinePlan(timeline, targets, out);
}
