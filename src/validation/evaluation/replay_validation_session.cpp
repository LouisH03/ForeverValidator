#include "validation/evaluation/replay_validation_session.h"
#include <functional>

#include "format/replay/replay_file.h"
std::optional<std::int32_t> ReplayValidationReplay::ExpectedRaceTimeMs() const {
    return inputTimeline_.Metadata().raceTimeMs;
}

std::optional<std::int32_t>
ReplayValidationReplay::ExpectedStuntsScore() const {
    if (!inputTimeline_.Metadata().stuntScore.has_value()) {
        return std::nullopt;
    }
    return static_cast<std::int32_t>(
            *inputTimeline_.Metadata().stuntScore);
}

std::optional<std::int32_t> ReplayValidationReplay::ExpectedRespawns() const {
    if (!inputTimeline_.Metadata().respawnCount.has_value()) {
        return std::nullopt;
    }
    return static_cast<std::int32_t>(
            *inputTimeline_.Metadata().respawnCount);
}

std::size_t ReplayValidationReplay::ExpectedSampleCount(
        const ReplayGhostTrajectory &trajectory,
        std::int32_t inputDurationMs,
        std::optional<std::int32_t> expectedRaceTimeMs) {
    std::int32_t compareUntilMs = expectedRaceTimeMs.value_or(0);
    if (compareUntilMs == -1) {
        compareUntilMs = inputDurationMs;
    }
    return trajectory.CountThrough(compareUntilMs);
}

std::size_t ReplayValidationReplay::ExpectedSamples() const {
    return ExpectedSampleCount(
            ghostTrajectory_,
            static_cast<std::int32_t>(inputTimeline_.Metadata().durationMs),
            ExpectedRaceTimeMs());
}

std::uint32_t ReplayValidationReplay::RequestedSamples(
        const ReplayValidationConfiguration &configuration) const {
    const std::size_t expectedSamples = ExpectedSamples();
    if (expectedSamples >= configuration.requestedSamples) {
        return configuration.requestedSamples;
    }
    return static_cast<std::uint32_t>(expectedSamples);
}

ReplayValidationPlan ReplayValidationReplay::BuildStreamPlan(
        const ReplayValidationConfiguration &configuration,
        ReplayValidationMode validationMode) const {
    ReplayValidationPlan plan;
    plan.validationMode = validationMode;
    plan.sampleCount = ghostTrajectory_.SampleCount();
    plan.samplePeriodMs = ghostTrajectory_.FixedStepMs();
    plan.requestedSamples = RequestedSamples(configuration);
    plan.expectedSamples = ExpectedSamples();
    plan.controlTickMs = configuration.controlTickMs;
    plan.validationPrestartMs = configuration.validationPrestartMs;
    plan.baseActions = configuration.baseActions;
    plan.inputTimeBaseMs = configuration.inputTimeBaseMs;
    plan.validationSeed = inputTimeline_.Metadata().validationSeed;
    plan.validationDurationMs = static_cast<std::int32_t>(
            inputTimeline_.Metadata().durationMs);
    plan.enableRaceSimulationAfterMs =
            static_cast<std::int32_t>(configuration.validationPrestartMs);
    plan.establishRaceSpawnAtMs = 0;
    plan.baseActions.enableStuntsSimulation =
            validationMode == ReplayValidationMode::Stunts;
    plan.baseActions.stuntsTimeLimitMs =
            plan.baseActions.enableStuntsSimulation
                    ? stuntsTimeLimitMs_.value_or(
                              DefaultChallengeStuntsTimeLimitMs)
                    : 0u;

    // Race keeps the legacy zero sentinel. Platform and Puzzle require an
    // explicit recorded time so missing metadata can be rejected.
    // Stunts validates its independently simulated score instead of race time.
    if (validationMode == ReplayValidationMode::Stunts) {
        plan.expectedRaceTimeMs = std::nullopt;
        plan.expectedStuntsScore = ExpectedStuntsScore();
    } else if (validationMode == ReplayValidationMode::Platform ||
               validationMode == ReplayValidationMode::Puzzle) {
        plan.expectedRaceTimeMs = ExpectedRaceTimeMs();
        plan.expectedStuntsScore = std::nullopt;
    } else {
        plan.expectedRaceTimeMs = ExpectedRaceTimeMs().value_or(0);
        plan.expectedStuntsScore = std::nullopt;
    }
    plan.expectedRespawns = ExpectedRespawns();
    return plan;
}

ReplayValidationMetadata ReplayValidationReplay::BuildMetadata(
        const ReplayValidationConfiguration &configuration) const {
    ReplayValidationMetadata metadata;
    metadata.sampleCount = ghostTrajectory_.SampleCount();
    metadata.samplePeriodMs = ghostTrajectory_.FixedStepMs();
    metadata.inputDurationMs = static_cast<std::int32_t>(
            inputTimeline_.Metadata().durationMs);
    metadata.expectedRaceTimeMs = ExpectedRaceTimeMs();
    metadata.expectedRespawns = ExpectedRespawns();
    metadata.requestedSamples = RequestedSamples(configuration);
    metadata.expectedSamples = ExpectedSamples();
    metadata.actionCount = inputTimeline_.DefinedActionCount();
    metadata.eventCount = inputTimeline_.EventCount();
    return metadata;
}

namespace {

ReplayFileValidationResult BuildReplayFileMetadata(
        const ReplayFile &replayFile,
        const ReplayValidationReplay &replay,
        const ReplayValidationConfiguration &configuration) {
    ReplayFileValidationResult result;
    result.metadata = replay.BuildMetadata(configuration);
    const ReplayGhostArchiveMetadata &ghostArchive =
            replayFile.GhostArchiveMetadata();
    result.metadata.encodedGhostSampleByteCount =
            ghostArchive.encodedSampleByteCount;
    result.metadata.encodedGhostStateByteCount =
            ghostArchive.encodedStateByteCount;
    return result;
}

}  // namespace

std::optional<ReplayFileValidationResult> ClassifyReplayCompatibility(
        const ReplayFile &replayFile,
        const ReplayValidationConfiguration &configuration) {
    if (!configuration.IsValid() ||
        replayFile.CompatibilityMetadata().IsCompatible()) {
        return std::nullopt;
    }
    ReplayValidationReplay replay(
            replayFile.InputTimeline(),
            replayFile.GhostTrajectory(),
            replayFile.ChallengeMetadata().stuntsTimeLimitMs);
    ReplayFileValidationResult result = BuildReplayFileMetadata(
            replayFile, replay, configuration);
    result.validation.expectedSamples = static_cast<std::uint32_t>(
            result.metadata.expectedSamples);
    result.validation.status =
            ReplayValidationStatus::IncompatibleReplayVersion;
    result.validation.outcome = ReplayValidationOutcome::Invalid;
    return result;
}

std::optional<ReplayFileValidationResult> ClassifyReplayInputAvailability(
        const ReplayFile &replayFile,
        const ReplayValidationConfiguration &configuration) {
    if (!configuration.IsValid() || replayFile.HasValidationInput()) {
        return std::nullopt;
    }
    ReplayValidationReplay replay(
            replayFile.InputTimeline(),
            replayFile.GhostTrajectory(),
            replayFile.ChallengeMetadata().stuntsTimeLimitMs);
    ReplayFileValidationResult result = BuildReplayFileMetadata(
            replayFile, replay, configuration);
    result.validation.expectedSamples = static_cast<std::uint32_t>(
            result.metadata.expectedSamples);
    result.validation.status = ReplayValidationStatus::InputUnavailable;
    result.validation.outcome = ReplayValidationOutcome::Unavailable;
    return result;
}

ReplayFileValidationBuild ValidateReplayFile(
        const ReplayFile &replayFile,
        ReplayValidationMode validationMode,
        ReplaySimulationSession &simulationSession,
        const ReplaySimulationDefinition &simulationDefinition,
        const ReplayValidationConfiguration &configuration) {
    if (!configuration.IsValid()) {
        return ReplayFileValidationBuild::Failure(
                ReplayValidationExecutionResult::MissingInput);
    }
    std::optional<ReplayFileValidationResult> compatibility =
            ClassifyReplayCompatibility(replayFile, configuration);
    if (compatibility.has_value()) {
        return ReplayFileValidationBuild::Success(
                std::move(*compatibility));
    }
    std::optional<ReplayFileValidationResult> inputAvailability =
            ClassifyReplayInputAvailability(replayFile, configuration);
    if (inputAvailability.has_value()) {
        return ReplayFileValidationBuild::Success(
                std::move(*inputAvailability));
    }

    ReplayValidationReplay replay(
            replayFile.InputTimeline(),
            replayFile.GhostTrajectory(),
            replayFile.ChallengeMetadata().stuntsTimeLimitMs);
    ReplayValidationPlan plan = replay.BuildStreamPlan(
            configuration, validationMode);
    const ReplayChallengeMetadata &challenge =
            replayFile.ChallengeMetadata();
    plan.playMode = challenge.playMode.value_or(EChallengePlayMode::Race);
    plan.isLapRace = challenge.isLapRace;
    plan.lapCount = challenge.isLapRace ? challenge.lapCount : 1u;

    ReplayFileValidationResult result = BuildReplayFileMetadata(
            replayFile, replay, configuration);

    ReplayValidationExecutionOutput execution;
    const ReplayValidationExecutionResult status = ExecuteReplayValidation(
            &execution,
            simulationSession,
            plan,
            simulationDefinition,
            replay.Trajectory(),
            replay.InputTimeline());
    if (status == ReplayValidationExecutionResult::Success) {
        result.validation = std::move(execution.validation);
        result.raceOutcome = std::move(execution.raceOutcome);
        return ReplayFileValidationBuild::Success(std::move(result));
    }
    if (status == ReplayValidationExecutionResult::MapStartUnavailable) {
        result.validation.expectedSamples = static_cast<std::uint32_t>(
                plan.expectedSamples);
        result.validation.status = ReplayValidationStatus::WrongSimulation;
        result.validation.outcome = ReplayValidationOutcome::WrongSimulation;
        result.validation.wrongSimulation = true;
        return ReplayFileValidationBuild::Success(std::move(result));
    }
    return ReplayFileValidationBuild::Failure(status);
}
