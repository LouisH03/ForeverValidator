#ifndef TMNF_REPLAY_CHALLENGE_METADATA_H
#define TMNF_REPLAY_CHALLENGE_METADATA_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/game/game_ctn_types.h"

inline constexpr std::uint32_t DefaultChallengeStuntsTimeLimitMs = 60000u;

struct ReplayArchiveIdentifier {
    std::string id;
    std::string collection;
    std::string author;

    bool Empty(void) const;
};

enum class ReplayMapEnvironment {
    Unknown,
    Alpine,
    Speed,
    Rally,
    Island,
    Coast,
    Bay,
    Stadium,
};

enum class ReplayVehicleModel {
    Unknown,
    SnowCar,
    DesertCar,
    RallyCar,
    IslandCar,
    CoastCar,
    BayCar,
    StadiumCar,
};

struct ReplayPuzzleBlockStockEntry {
    ReplayArchiveIdentifier blockModel;
    std::uint32_t count = 0u;
};

struct ReplayChallengeMetadata {
    std::string mapName;
    ReplayArchiveIdentifier challengeVehicle;
    std::optional<EChallengePlayMode> playMode;
    std::optional<std::uint32_t> stuntsTimeLimitMs;
    bool isLapRace = false;
    std::uint32_t lapCount = 0u;
    std::vector<ReplayPuzzleBlockStockEntry> puzzleBlockStock;
};

ReplayMapEnvironment DecodeReplayMapEnvironment(std::string_view collection);
ReplayVehicleModel DecodeReplayVehicleModel(std::string_view identifier);
ReplayVehicleModel DefaultReplayVehicleModel(
        ReplayMapEnvironment environment);
const char *ReplayMapEnvironmentName(ReplayMapEnvironment environment);
const char *ReplayVehicleModelName(ReplayVehicleModel vehicle);
const char *EChallengePlayModeName(EChallengePlayMode playMode);

#endif
