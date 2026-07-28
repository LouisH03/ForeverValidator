#include "format/replay/replay_input_timeline.h"
#include "simulation/control/replay_control_plan.h"

#include <cstdint>
#include <iostream>
#include <vector>

namespace {

bool Check(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

bool TestRaceValidationAppendsUnobservedTick() {
    ReplayInputMetadata metadata;
    metadata.durationMs = 20u;
    const ReplayInputActionValue press = ReplayInputActionValue::Switch(
            ReplayInputSwitchState::Pressed);
    std::vector<ReplayInputEvent> events = {
            {10u, ReplayInputActionKind::Respawn, press},
            {20u, ReplayInputActionKind::Accelerate, press},
            {20u, ReplayInputActionKind::RaceRunning, press},
            {20u, ReplayInputActionKind::FinishLine, press},
            {20u, ReplayInputActionKind::Respawn, press},
    };
    ReplayInputTimeline timeline;
    if (!Check(
                ReplayInputTimeline::Create(
                        metadata,
                        {ReplayInputActionKind::Accelerate,
                         ReplayInputActionKind::RaceRunning,
                         ReplayInputActionKind::FinishLine,
                         ReplayInputActionKind::Respawn},
                        std::move(events),
                        &timeline) ==
                        ReplayInputTimelineCreateResult::Success,
                "could not create test input timeline")) {
        return false;
    }

    ReplayControlPlanRequest request(timeline);
    request.controlTickMs = 10u;
    request.validationDurationMs = 20;
    request.validationPrestartMs = 10u;
    request.enableRaceSimulationAfterMs = 30;
    request.establishRaceSpawnAtMs = 30;

    ReplayControlPlan ordinary;
    bool okay = Check(
            BuildReplayControlPlan(request, &ordinary) ==
                    ReplayControlPlanBuildResult::Success,
            "could not build ordinary control plan");
    if (!okay || ordinary.ticks.empty()) {
        return false;
    }
    okay &= Check(
            ordinary.ticks.back().timeMs == 30u &&
                    ordinary.ticks.back().observe,
            "default control plan did not end at its observed horizon");

    request.appendUnobservedTrailingTick = true;
    ReplayControlPlan extended;
    okay &= Check(
            BuildReplayControlPlan(request, &extended) ==
                    ReplayControlPlanBuildResult::Success,
            "could not build trailing-tick control plan");
    if (!okay || extended.ticks.size() != ordinary.ticks.size() + 1u) {
        return false;
    }
    const ReplayControlTick &observation =
            extended.ticks[extended.ticks.size() - 2u];
    const ReplayControlTick &trailing = extended.ticks.back();
    okay &= Check(
            observation.timeMs == 30u && observation.observe &&
                    observation.actions.establishRaceSpawn &&
                    observation.actions.resetAtRaceStart &&
                    observation.actions.finishRace &&
                    observation.actions.respawnAtCheckpointCount == 1u,
            "recorded horizon tick lost its observations or transitions");
    okay &= Check(
            trailing.timeMs == 40u && !trailing.observe &&
                    !trailing.comparisonTarget.has_value(),
            "trailing physics tick was not appended without observation");
    okay &= Check(
            !trailing.actions.establishRaceSpawn &&
                    !trailing.actions.resetAtRaceStart &&
                    !trailing.actions.finishRace &&
                    trailing.actions.respawnAtCheckpointCount == 0u,
            "trailing physics tick repeated one-shot transitions");
    okay &= Check(
            trailing.actions.enableRaceSimulation &&
                    trailing.controls.lowSpeedGateA == 1.0f,
            "trailing physics tick did not preserve persistent final input");
    okay &= Check(
            extended.comparisonTargetCount ==
                    ordinary.comparisonTargetCount,
            "trailing physics tick changed the comparison target count");
    return okay;
}

}  // namespace

int main() {
    return TestRaceValidationAppendsUnobservedTick() ? 0 : 1;
}
