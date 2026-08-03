#ifndef TMNF_REPLAY_FILE_H
#define TMNF_REPLAY_FILE_H

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "engine/game/replay_challenge_map_input.h"
#include "format/replay/replay_challenge_metadata.h"
#include "format/replay/replay_ghost_trajectory.h"
#include "format/replay/replay_input_timeline.h"
enum class ReplayFileReadError {
    Success,
    InvalidRequest,
    FileReadFailed,
    InvalidContainer,
    AllocationFailed,
    RootBodyDecompressionFailed,
    MissingGhostBuffer,
    MissingInputStream,
    TooManyInputActions,
    InvalidGhostMetadata,
    InvalidInputHeader,
    InvalidInputActions,
    InvalidInputEventHeader,
    InvalidInputEvents,
    InvalidInputTimeline,
    MissingGhostState,
    GhostStateDecompressionFailed,
    InvalidGhostState,
    GhostTrajectoryAllocationFailed,
    MissingEmbeddedChallenge,
    InvalidEmbeddedChallenge,
    InvalidMap,
};

struct ReplayGhostArchiveMetadata {
    std::size_t encodedStateByteCount = 0u;
    std::size_t encodedSampleByteCount = 0u;
};

enum class ReplayVersion : std::uint8_t {
    TMr6,
    TMr7,
};

enum class ReplayCompatibility : std::uint8_t {
    Compatible,
    Incompatible,
};

struct ReplayCompatibilityMetadata {
    ReplayVersion replayVersion = ReplayVersion::TMr7;
    ReplayVersion currentGameVersion = ReplayVersion::TMr7;
    ReplayCompatibility compatibility = ReplayCompatibility::Compatible;

    bool IsCompatible() const {
        return compatibility == ReplayCompatibility::Compatible;
    }
};

class ReplayFile {
public:
    ReplayFile() = default;
    ReplayFile(const ReplayFile &) = delete;
    ReplayFile &operator=(const ReplayFile &) = delete;
    ReplayFile(ReplayFile &&) = default;
    ReplayFile &operator=(ReplayFile &&) = default;

    const ReplayInputTimeline &InputTimeline() const {
        return inputTimeline_;
    }

    const ReplayGhostTrajectory &GhostTrajectory() const {
        return ghostTrajectory_;
    }

    const CGameCtnReplayMapInput &MapInput() const {
        return mapInput_;
    }

    const ReplayGhostArchiveMetadata &GhostArchiveMetadata() const {
        return ghostArchiveMetadata_;
    }

    const ReplayCompatibilityMetadata &CompatibilityMetadata() const {
        return compatibilityMetadata_;
    }

    const ReplayChallengeMetadata &ChallengeMetadata() const {
        return challengeMetadata_;
    }

    const ReplayArchiveIdentifier &VehicleIdentifier() const {
        return vehicleIdentifier_;
    }

    bool HasValidationInput() const {
        return hasValidationInput_;
    }

    ReplayMapEnvironment MapEnvironment() const {
        return DecodeReplayMapEnvironment(mapInput_.DefaultCollectionName());
    }

    ReplayVehicleModel VehicleModel() const {
        return DecodeReplayVehicleModel(vehicleIdentifier_.id);
    }

private:
    friend ReplayFileReadError ReadReplayBytes(
            const std::uint8_t *bytes,
            std::size_t byteCount,
            ReplayFile *out);
    friend ReplayFileReadError ReadChallengeBytes(
            const std::uint8_t *bytes,
            std::size_t byteCount,
            ReplayFile *out);
    friend ReplayFileReadError ParseReplayStorage(
            const std::vector<std::uint8_t> &fileStorage,
            ReplayFile *out);

    ReplayFile(ReplayInputTimeline inputTimeline,
               ReplayGhostTrajectory ghostTrajectory,
               ReplayGhostArchiveMetadata ghostArchiveMetadata,
               ReplayCompatibilityMetadata compatibilityMetadata,
               CGameCtnReplayMapInput mapInput,
               ReplayChallengeMetadata challengeMetadata,
               ReplayArchiveIdentifier vehicleIdentifier,
               bool hasValidationInput)
        : inputTimeline_(std::move(inputTimeline)),
          ghostTrajectory_(std::move(ghostTrajectory)),
          ghostArchiveMetadata_(ghostArchiveMetadata),
          compatibilityMetadata_(compatibilityMetadata),
          mapInput_(std::move(mapInput)),
          challengeMetadata_(std::move(challengeMetadata)),
          vehicleIdentifier_(std::move(vehicleIdentifier)),
          hasValidationInput_(hasValidationInput) {}

    ReplayInputTimeline inputTimeline_;
    ReplayGhostTrajectory ghostTrajectory_;
    ReplayGhostArchiveMetadata ghostArchiveMetadata_;
    ReplayCompatibilityMetadata compatibilityMetadata_;
    CGameCtnReplayMapInput mapInput_;
    ReplayChallengeMetadata challengeMetadata_;
    ReplayArchiveIdentifier vehicleIdentifier_;
    bool hasValidationInput_ = false;
};

ReplayFileReadError ReadReplayBytes(
        const std::uint8_t *bytes,
        std::size_t byteCount,
        ReplayFile *out);
ReplayFileReadError ReadChallengeBytes(
        const std::uint8_t *bytes,
        std::size_t byteCount,
        ReplayFile *out);
const char *ReplayFileReadErrorName(ReplayFileReadError error);

#endif
