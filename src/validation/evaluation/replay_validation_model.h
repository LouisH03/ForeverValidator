#ifndef TMNF_REPLAY_VALIDATION_MODEL_H
#define TMNF_REPLAY_VALIDATION_MODEL_H

#include <cstddef>
#include <cstdint>
#include <optional>

#include "engine/game/game_ctn_types.h"
#include "simulation/control/replay_control_timeline.h"
#include "validation/evaluation/replay_validation_mode.h"
#include "validation/evaluation/replay_validation_result.h"

struct ReplayValidationConfiguration {
    std::uint32_t requestedSamples = 0u;
    std::uint32_t controlTickMs = 0u;
    std::uint32_t validationPrestartMs = 0u;
    ReplayRaceTransitionActions baseActions{};
    std::uint32_t inputTimeBaseMs = 0u;

    bool IsValid() const {
        return requestedSamples != 0u &&
               controlTickMs != 0u &&
               validationPrestartMs != 0u;
    }
};

struct ReplayValidationPlan {
    ReplayValidationMode validationMode = ReplayValidationMode::Race;
    std::size_t sampleCount = 0u;
    std::uint32_t samplePeriodMs = 0u;
    std::uint32_t requestedSamples = 0u;
    std::size_t expectedSamples = 0u;
    std::uint32_t controlTickMs = 0u;
    std::uint32_t validationPrestartMs = 0u;
    ReplayRaceTransitionActions baseActions{};
    std::uint32_t inputTimeBaseMs = 0u;
    std::uint32_t validationSeed = 0u;
    std::int32_t validationDurationMs = 0;
    std::optional<std::int32_t> enableRaceSimulationAfterMs;
    std::optional<std::int32_t> establishRaceSpawnAtMs;
    std::optional<std::int32_t> expectedRaceTimeMs;
    std::optional<std::int32_t> expectedStuntsScore;
    std::optional<std::int32_t> expectedRespawns;
    EChallengePlayMode playMode = EChallengePlayMode::Race;
    bool isLapRace = false;
    std::uint32_t lapCount = 1u;
};

struct ReplayValidationMetadata {
    std::size_t sampleCount = 0u;
    std::uint32_t samplePeriodMs = 0u;
    std::size_t encodedGhostSampleByteCount = 0u;
    std::size_t encodedGhostStateByteCount = 0u;
    std::int32_t inputDurationMs = 0;
    std::optional<std::int32_t> expectedRaceTimeMs;
    std::optional<std::int32_t> expectedRespawns;
    std::uint32_t requestedSamples = 0u;
    std::size_t expectedSamples = 0u;
    std::size_t actionCount = 0u;
    std::size_t eventCount = 0u;
};

struct ReplayRaceOutcome {
    std::optional<bool> raceCompleted;
    std::optional<std::int32_t> raceTimeMs;
    std::optional<std::int32_t> stuntsScore;
    std::uint32_t respawnCount = 0u;
};

struct ReplayValidationExecutionOutput {
    ReplayValidationResult validation;
    ReplayRaceOutcome raceOutcome;
};

#endif
