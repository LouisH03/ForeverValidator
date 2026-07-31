#include "simulation/replay/replay_map_scene.h"
#include <utility>

#include "simulation/replay/replay_challenge_construction.h"
#include "engine/game/trackmania_race.h"
#include <new>
ReplayMapScene::~ReplayMapScene() {
    persistentCollisionZone_.Reset();
}

void ReplayMapScene::Reset(CTrackManiaRace &race) {
    race.ResetValidationSession();
    persistentCollisionZone_.Reset();
    staticCorpuses_.Clear();
    dedicatedCollisionCorpuses_.Clear();
    models_.Clear();
    blockPlacements_.Clear();
    challenge_.reset();
    active_ = false;
    ready_ = false;
    collisionZoneConstructed_ = false;
    stationaryCorpusesInstalled_ = false;
}

ReplayMapSceneResult ReplayMapScene::PreloadChallenge(
        CGameCtnChallengeConstruction &construction) {
    persistentCollisionZone_.Reset();
    staticCorpuses_.Clear();
    dedicatedCollisionCorpuses_.Clear();
    models_.Clear();
    blockPlacements_.Clear();
    challenge_.reset();
    active_ = false;
    ready_ = false;
    collisionZoneConstructed_ = false;
    stationaryCorpusesInstalled_ = false;

    if (!construction.Ready() ||
        !construction.HasPreparedBlockPlacements()) {
        return ReplayMapSceneResult::ChallengeUnavailable;
    }

    std::unique_ptr<CGameCtnChallenge> challenge;
    if (!construction.MovePreparedSceneTo(
                challenge, blockPlacements_)) {
        return ReplayMapSceneResult::ChallengeUnavailable;
    }
    challenge_ = std::move(challenge);
    return ReplayMapSceneResult::Ready;
}

bool ReplayMapScene::ClonePreparedFrom(const ReplayMapScene &source) {
    persistentCollisionZone_.Reset();
    staticCorpuses_.Clear();
    dedicatedCollisionCorpuses_.Clear();
    try {
        challenge_ = source.challenge_;
        blockPlacements_ = source.blockPlacements_;
        models_ = source.models_;
    } catch (const std::bad_alloc &) {
        challenge_.reset();
        blockPlacements_.Clear();
        models_.Clear();
        active_ = false;
        ready_ = false;
        collisionZoneConstructed_ = false;
        stationaryCorpusesInstalled_ = false;
        return false;
    }
    active_ = source.active_;
    ready_ = false;
    collisionZoneConstructed_ = false;
    stationaryCorpusesInstalled_ = false;
    return challenge_ != nullptr && !models_.Empty();
}

ReplayMapSceneResult ReplayMapScene::InstallModels(
        StaticSceneModelCollection models) {
    staticCorpuses_.Clear();
    dedicatedCollisionCorpuses_.Clear();
    try {
        models_ = std::move(models);
    } catch (const std::bad_alloc &) {
        models_.Clear();
        return ReplayMapSceneResult::MissingModelSources;
    }
    ready_ = false;
    stationaryCorpusesInstalled_ = false;
    return ReplayMapSceneResult::Ready;
}

void ReplayMapScene::Activate() {
    active_ = true;
}

ReplayMapSceneResult ReplayMapScene::BuildStaticCorpuses() {
    staticCorpuses_.Clear();
    dedicatedCollisionCorpuses_.Clear();
    if (!staticCorpuses_.BuildFromModels(
                models_,
                ReplayStaticCorpusModelSelection::
                        ExcludeDedicatedInitialCollision) ||
        !dedicatedCollisionCorpuses_.BuildFromModels(models_)) {
        return ReplayMapSceneResult::StaticCorpusConstructionFailed;
    }
    if (!models_.IsComplete()) {
        return ReplayMapSceneResult::MissingModelSources;
    }
    if (staticCorpuses_.Count() + dedicatedCollisionCorpuses_.Count() !=
        models_.Models().size()) {
        return ReplayMapSceneResult::CorpusCountMismatch;
    }
    return ReplayMapSceneResult::Ready;
}

ReplayMapSceneResult ReplayMapScene::EnsureReady(CTrackManiaRace &race) {
    if (!active_) {
        return ReplayMapSceneResult::Inactive;
    }
    if (ready_) {
        return ReplayMapSceneResult::Ready;
    }

    const ReplayMapSceneResult buildResult = BuildStaticCorpuses();
    if (buildResult != ReplayMapSceneResult::Ready) {
        return buildResult;
    }
    ready_ = true;
    race.ResetValidationSession();
    race.BindCheckpointCourse(&staticCorpuses_);
    race.BindVehicle(nullptr);
    race.PrepareCheckpoints();
    return ReplayMapSceneResult::Ready;
}

ReplayMapSceneResult ReplayMapScene::InstallStationaryCorpuses(
        CSceneVehicleCar *vehicle,
        CTrackManiaRace &race) {
    race.ResetValidationSession();
    race.BindCheckpointCourse(&staticCorpuses_);
    race.BindVehicle(vehicle);
    race.PrepareCheckpoints();
    return InstallReplaySceneCollisionCorpuses(
                   staticCorpuses_,
                   dedicatedCollisionCorpuses_,
                   persistentCollisionZone_)
                   ? ReplayMapSceneResult::Ready
                   : ReplayMapSceneResult::StationaryCorpusInstallFailed;
}

ReplayMapSceneResult ReplayMapScene::SelectCollisionZone(
        CHmsCollisionManagerSZone &localZone,
        CSceneVehicleCar *vehicle,
        CTrackManiaRace &race,
        CHmsCollisionManagerSZone *&selectedZone) {
    selectedZone = &localZone;
    if (!active_) {
        return ReplayMapSceneResult::Ready;
    }

    const ReplayMapSceneResult readyResult = EnsureReady(race);
    if (readyResult != ReplayMapSceneResult::Ready) {
        return readyResult;
    }
    if (!collisionZoneConstructed_) {
        persistentCollisionZone_.Reset();
        collisionZoneConstructed_ = true;
    }
    if (!stationaryCorpusesInstalled_) {
        const ReplayMapSceneResult installResult =
                InstallStationaryCorpuses(vehicle, race);
        if (installResult != ReplayMapSceneResult::Ready) {
            return installResult;
        }
        stationaryCorpusesInstalled_ = true;
    } else {
        race.ResetValidationSession();
        race.BindCheckpointCourse(&staticCorpuses_);
        race.BindVehicle(vehicle);
        race.PrepareCheckpoints();
    }
    selectedZone = &persistentCollisionZone_;
    return ReplayMapSceneResult::Ready;
}

bool ReplayMapScene::FirstStartLineSpawnLocation(GmIso4 &location) const {
    std::optional<GmIso4> spawn =
            blockPlacements_.FirstSurvivingStartLineSpawn();
    if (!spawn.has_value()) {
        spawn = blockPlacements_.FirstRetainedLogicalStartLineSpawn();
    }
    if (!spawn.has_value()) {
        return false;
    }
    location = *spawn;
    return true;
}
