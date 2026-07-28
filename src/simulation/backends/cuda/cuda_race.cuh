#ifndef FOREVERVALIDATOR_CUDA_RACE_CUH
#define FOREVERVALIDATOR_CUDA_RACE_CUH

#include "simulation/backends/cuda/cuda_scene_layout.h"
#include "simulation/backends/cuda/cuda_state_layout.h"

namespace forevervalidator::simulation::cuda::race {
namespace detail {

__device__ inline void SetSpawn(
        CudaRacePhysicsState &race,
        const GmIso4 &spawn,
        bool updateHistory) {
    if (updateHistory) {
        race.player.previousSpawnLocation = spawn;
    }
    race.player.currentSpawnLocation = spawn;
}

__device__ inline void ClearFreewheel(
        CudaCandidatePhysicsState &candidate) {
    candidate.vehicle.controls.forcedLowSpeedFriction = false;
    ++candidate.race.progress.freewheelClearCount;
}

__device__ inline bool AcceptCheckpointSlot(
        CudaCandidatePhysicsState &candidate,
        std::uint32_t checkpointIndex,
        std::uint32_t checkpointSlot,
        const GmIso4 *spawn) {
    CudaRacePhysicsState &race = candidate.race;
    if (checkpointIndex >= race.checkpointSlotsPassed.count ||
        checkpointSlot >= race.checkpointSlotsPassed.count ||
        race.checkpointSlotsPassed.Get(checkpointSlot)) {
        return false;
    }
    race.checkpointSlotsPassed.Set(checkpointSlot);
    if (checkpointIndex !=
        race.progress.requiredCheckpointCount) {
        ++race.progress.currentLapCheckpointCount;
        ++race.progress.checkpointCount;
    }
    ++race.progress.totalCheckpointEventCount;
    if (spawn != nullptr) {
        if (!race.currentSpawnLocationInitialized) {
            race.player.previousSpawnLocation = *spawn;
            race.currentSpawnLocationInitialized = true;
        }
        race.lastAcceptedSpawnLocation.present = true;
        race.lastAcceptedSpawnLocation.value = *spawn;
        race.playerSpawnLocation.present = true;
        race.playerSpawnLocation.value = *spawn;
        SetSpawn(race, *spawn, false);
    } else {
        SetSpawn(
                race, race.player.previousSpawnLocation, false);
        if (race.currentSpawnLocationInitialized) {
            race.playerSpawnLocation.present = true;
            race.playerSpawnLocation.value =
                    race.player.previousSpawnLocation;
        }
    }
    ClearFreewheel(candidate);
    return true;
}

__device__ inline void Checkpoint(
        CudaCandidatePhysicsState &candidate,
        const CudaSceneActor &actor) {
    CudaRacePhysicsState &race = candidate.race;
    race.progress.lastBlockRole = actor.checkpointRole;
    race.progress.lastContactBlockId = actor.raceBlockId;
    if (actor.respawnUsesCurrentTransform) {
        ClearFreewheel(candidate);
    }
    if (actor.checkpointSlot == UINT32_MAX) return;
    const GmIso4 *spawn = nullptr;
    GmIso4 currentSpawn{};
    if (actor.respawnUsesCurrentTransform) {
        currentSpawn = race.player.currentSpawnLocation;
        spawn = &currentSpawn;
    } else if (actor.hasCheckpointSpawn) {
        spawn = &actor.checkpointSpawn;
    }
    if (AcceptCheckpointSlot(
                candidate,
                race.progress.currentLapCheckpointCount,
                actor.checkpointSlot, spawn)) {
        race.progress.lastAcceptedBlockId =
                race.progress.lastContactBlockId;
    }
}

__device__ inline void Finish(
        CudaCandidatePhysicsState &candidate,
        const CudaSceneActor &actor) {
    CudaRacePhysicsState &race = candidate.race;
    ReplayRaceProgress &progress = race.progress;
    progress.lastBlockRole = actor.checkpointRole;
    if (progress.raceCompleted ||
        progress.currentLapCheckpointCount <
                progress.requiredCheckpointCount) {
        return;
    }
    if (race.replayPlayMode ==
        static_cast<std::uint32_t>(
                EChallengePlayMode::Shortcut)) {
        ++progress.finishCount;
        progress.completedLapCount = 1u;
        progress.raceCompleted = true;
        return;
    }
    const std::uint32_t finishSlot =
            progress.requiredCheckpointCount;
    if (!AcceptCheckpointSlot(
                candidate, finishSlot, finishSlot, nullptr)) {
        return;
    }
    ++progress.finishCount;
    ++progress.completedLapCount;
    if (progress.requiredLapCount != 0u &&
        progress.completedLapCount >=
                progress.requiredLapCount) {
        progress.raceCompleted = true;
        return;
    }
    race.checkpointSlotsPassed.Clear();
    progress.currentLapCheckpointCount = 0u;
}

}  // namespace detail

__device__ inline void OnTriggerContact(
        CudaCandidatePhysicsState &candidate,
        const CudaSceneActor &actor) {
    if (!actor.hasCheckpoint ||
        actor.checkpointRole ==
                static_cast<std::uint32_t>(
                        BlockRaceRole::None)) {
        return;
    }
    candidate.race.preparedEventTimeMs =
            candidate.world.tickTimeMs;
    candidate.race.player.eventPrepared = true;
    candidate.race.progress.lastPrepareTimeMs =
            candidate.world.tickTimeMs;
    ++candidate.race.progress.preparedEventCount;
    switch (static_cast<BlockRaceRole>(
            actor.checkpointRole)) {
    case BlockRaceRole::Checkpoint:
        detail::Checkpoint(candidate, actor);
        return;
    case BlockRaceRole::FinishLine:
    case BlockRaceRole::StartFinishLine:
        detail::Finish(candidate, actor);
        return;
    default:
        return;
    }
}

}  // namespace forevervalidator::simulation::cuda::race

#endif
