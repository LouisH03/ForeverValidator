#include "format/replay/replay_challenge_metadata.h"

bool ReplayArchiveIdentifier::Empty(void) const {
    return id.empty() && collection.empty() && author.empty();
}

ReplayMapEnvironment DecodeReplayMapEnvironment(std::string_view collection) {
    if (collection == "Alpine") return ReplayMapEnvironment::Alpine;
    if (collection == "Speed") return ReplayMapEnvironment::Speed;
    if (collection == "Rally") return ReplayMapEnvironment::Rally;
    if (collection == "Island") return ReplayMapEnvironment::Island;
    if (collection == "Coast") return ReplayMapEnvironment::Coast;
    if (collection == "Bay") return ReplayMapEnvironment::Bay;
    if (collection == "Stadium") return ReplayMapEnvironment::Stadium;
    return ReplayMapEnvironment::Unknown;
}

ReplayVehicleModel DecodeReplayVehicleModel(std::string_view identifier) {
    if (identifier == "SnowCar") return ReplayVehicleModel::SnowCar;
    if (identifier == "American" || identifier == "DesertCar") {
        return ReplayVehicleModel::DesertCar;
    }
    if (identifier == "Rally" || identifier == "RallyCar") {
        return ReplayVehicleModel::RallyCar;
    }
    if (identifier == "SportCar" || identifier == "IslandCar") {
        return ReplayVehicleModel::IslandCar;
    }
    if (identifier == "CoastCar") return ReplayVehicleModel::CoastCar;
    if (identifier == "BayCar") return ReplayVehicleModel::BayCar;
    if (identifier == "StadiumCar") return ReplayVehicleModel::StadiumCar;
    return ReplayVehicleModel::Unknown;
}

ReplayVehicleModel DefaultReplayVehicleModel(
        ReplayMapEnvironment environment) {
    switch (environment) {
    case ReplayMapEnvironment::Alpine: return ReplayVehicleModel::SnowCar;
    case ReplayMapEnvironment::Speed: return ReplayVehicleModel::DesertCar;
    case ReplayMapEnvironment::Rally: return ReplayVehicleModel::RallyCar;
    case ReplayMapEnvironment::Island: return ReplayVehicleModel::IslandCar;
    case ReplayMapEnvironment::Coast: return ReplayVehicleModel::CoastCar;
    case ReplayMapEnvironment::Bay: return ReplayVehicleModel::BayCar;
    case ReplayMapEnvironment::Stadium:
        return ReplayVehicleModel::StadiumCar;
    case ReplayMapEnvironment::Unknown: return ReplayVehicleModel::Unknown;
    }
    return ReplayVehicleModel::Unknown;
}

const char *ReplayMapEnvironmentName(ReplayMapEnvironment environment) {
    switch (environment) {
    case ReplayMapEnvironment::Alpine: return "Alpine";
    case ReplayMapEnvironment::Speed: return "Speed";
    case ReplayMapEnvironment::Rally: return "Rally";
    case ReplayMapEnvironment::Island: return "Island";
    case ReplayMapEnvironment::Coast: return "Coast";
    case ReplayMapEnvironment::Bay: return "Bay";
    case ReplayMapEnvironment::Stadium: return "Stadium";
    case ReplayMapEnvironment::Unknown: return "Unknown";
    }
    return "Unknown";
}

const char *ReplayVehicleModelName(ReplayVehicleModel vehicle) {
    switch (vehicle) {
    case ReplayVehicleModel::SnowCar: return "SnowCar";
    case ReplayVehicleModel::DesertCar: return "DesertCar";
    case ReplayVehicleModel::RallyCar: return "RallyCar";
    case ReplayVehicleModel::IslandCar: return "IslandCar";
    case ReplayVehicleModel::CoastCar: return "CoastCar";
    case ReplayVehicleModel::BayCar: return "BayCar";
    case ReplayVehicleModel::StadiumCar: return "StadiumCar";
    case ReplayVehicleModel::Unknown: return "Unknown";
    }
    return "Unknown";
}

const char *EChallengePlayModeName(EChallengePlayMode playMode) {
    switch (playMode) {
    case EChallengePlayMode::Race: return "Race";
    case EChallengePlayMode::Platform: return "Platform";
    case EChallengePlayMode::Puzzle: return "Puzzle";
    case EChallengePlayMode::Crazy: return "Crazy";
    case EChallengePlayMode::Shortcut: return "Shortcut";
    case EChallengePlayMode::Stunts: return "Stunts";
    }
    return "Unknown";
}
