#include "validation/evaluation/replay_recorded_input_outcome.h"

#include <limits>

#include "format/replay/replay_input_timeline.h"
#include "simulation/runtime/replay_simulation_session.h"
#include "validation/evaluation/replay_validation_model.h"

ReplayRecordedInputOutcome ObserveReplayRecordedInputOutcome(
        const ReplayInputTimeline &timeline,
        std::uint32_t inputTimeBaseMs) {
    ReplayRecordedInputOutcome outcome;
    const std::vector<ReplayInputEvent> &events = timeline.Events();
    for (const ReplayInputEvent &event : events) {
        if (event.timeMs >= inputTimeBaseMs &&
            event.action == ReplayInputActionKind::Respawn &&
            event.value.IsActive()) {
            ++outcome.respawnCount;
        }
    }

    if (events.empty()) {
        return outcome;
    }
    const ReplayInputEvent &finalEvent = events.back();
    if (finalEvent.action != ReplayInputActionKind::FinishLine ||
        !finalEvent.value.IsCanonicalPress() ||
        finalEvent.timeMs < inputTimeBaseMs) {
        return outcome;
    }
    const std::uint32_t elapsedMs = finalEvent.timeMs - inputTimeBaseMs;
    if (elapsedMs <= static_cast<std::uint32_t>(
                             std::numeric_limits<std::int32_t>::max())) {
        outcome.finishRaceTimeMs = static_cast<std::int32_t>(elapsedMs);
    }
    return outcome;
}

bool ApplyReplayRecordedInputOutcome(
        const ReplayValidationPlan &plan,
        const ReplayRecordedInputOutcome &recorded,
        ReplaySimulationTimelineResult *simulationResult) {
    if (simulationResult == nullptr) {
        return false;
    }
    if (plan.validationMode == ReplayValidationMode::Race) {
        return true;
    }
    if (plan.validationMode == ReplayValidationMode::Platform ||
        plan.validationMode == ReplayValidationMode::Puzzle ||
        plan.validationMode == ReplayValidationMode::Stunts) {
        simulationResult->raceCompleted = false;
        simulationResult->finishTimeMs.reset();
    }
    simulationResult->executedRespawnCount = recorded.respawnCount;
    if (!recorded.finishRaceTimeMs.has_value()) {
        return true;
    }
    const std::uint64_t finishTickMs =
            static_cast<std::uint64_t>(plan.validationPrestartMs) +
            static_cast<std::uint32_t>(*recorded.finishRaceTimeMs);
    if (finishTickMs > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    simulationResult->raceCompleted = true;
    simulationResult->finishTimeMs = static_cast<std::uint32_t>(finishTickMs);
    return true;
}
