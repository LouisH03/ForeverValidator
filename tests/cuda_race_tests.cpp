#include "engine/game/game_ctn_block.h"
#include "engine/game/game_ctn_block_info.h"
#include "engine/game/trackmania_race.h"
#include "engine/scene/scene_vehicle_car.h"
#include "simulation/backends/cuda/cuda_race_certification.h"

#include <cstdint>
#include <iostream>

namespace {

struct ContactOutcome {
    bool forcedLowSpeedFriction = false;
    std::uint32_t freewheelClearCount = 0u;
};

ContactOutcome CpuContact(bool currentTransform) {
    CMwNodRef<CGameCtnBlockInfo> info = MakeMwNod<CGameCtnBlockInfo>();
    info->SetRaceRole(BlockRaceRole::Checkpoint);
    info->SetRespawnUsesCurrentTransform(currentTransform);
    CGameCtnBlock block;
    block.SetBlockInfo(info.Get());

    CTrackManiaRace race;
    CSceneVehicleCar vehicle;
    race.BindVehicle(&vehicle);
    race.SetCurrentTransformCheckpointFreewheelClearEnabled(true);
    vehicle.VehicleFreeWheelingSet(1);
    race.OnCheckpoint(nullptr, &block);
    return {
        vehicle.CaptureRuntimeClone().controls.forcedLowSpeedFriction,
        race.Progress().freewheelClearCount,
    };
}

ContactOutcome CudaContact(bool currentTransform) {
    using namespace forevervalidator::simulation;
    CudaCandidateState initial;
    initial.vehicle.controls.forcedLowSpeedFriction = true;
    CudaSceneActor actor;
    actor.hasCheckpoint = true;
    actor.checkpointRole =
            static_cast<std::uint32_t>(BlockRaceRole::Checkpoint);
    actor.checkpointSlot = UINT32_MAX;
    actor.respawnUsesCurrentTransform = currentTransform;

    const CudaRaceContactExecution executed =
            ExecuteCudaRaceContactForCertification(initial, actor);
    if (!executed.success) {
        std::cerr << executed.diagnostic << '\n';
        return {true, UINT32_MAX};
    }
    return {
        executed.finalState.vehicle.controls.forcedLowSpeedFriction,
        executed.finalState.race.progress.freewheelClearCount,
    };
}

bool CheckEqual(
        const ContactOutcome &cpu,
        const ContactOutcome &cuda,
        const char *name) {
    if (cpu.forcedLowSpeedFriction ==
                cuda.forcedLowSpeedFriction &&
        cpu.freewheelClearCount == cuda.freewheelClearCount) {
        return true;
    }
    std::cerr << name << " CPU/CUDA mismatch: friction="
              << cpu.forcedLowSpeedFriction << "/"
              << cuda.forcedLowSpeedFriction << " clears="
              << cpu.freewheelClearCount << "/"
              << cuda.freewheelClearCount << '\n';
    return false;
}

}  // namespace

int main() {
    const ContactOutcome currentCpu = CpuContact(true);
    const ContactOutcome currentCuda = CudaContact(true);
    if (!CheckEqual(
                currentCpu, currentCuda,
                "current-transform rejected checkpoint") ||
        currentCpu.forcedLowSpeedFriction ||
        currentCpu.freewheelClearCount != 1u) {
        std::cerr << "current-transform checkpoint did not clear freewheel\n";
        return 1;
    }

    const ContactOutcome ordinaryCpu = CpuContact(false);
    const ContactOutcome ordinaryCuda = CudaContact(false);
    if (!CheckEqual(
                ordinaryCpu, ordinaryCuda,
                "ordinary rejected checkpoint") ||
        !ordinaryCpu.forcedLowSpeedFriction ||
        ordinaryCpu.freewheelClearCount != 0u) {
        std::cerr << "ordinary checkpoint changed freewheel\n";
        return 1;
    }
    return 0;
}
