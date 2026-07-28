#include "engine/game/game_ctn_block.h"
#include "engine/game/game_ctn_block_info.h"
#include "engine/game/trackmania_race.h"
#include "engine/scene/scene_vehicle_car.h"
#include "simulation/runtime/replay_simulation_runtime.h"

#include <iostream>

namespace {

bool Check(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

bool TestCurrentTransformCheckpointContactClearsFreewheel() {
    CTrackManiaRace race;
    CSceneVehicleCar vehicle;
    CMwNodRef<CGameCtnBlockInfo> currentTransformInfo =
            MakeMwNod<CGameCtnBlockInfo>();
    currentTransformInfo->SetRespawnUsesCurrentTransform(true);
    CGameCtnBlock currentTransformBlock;
    currentTransformBlock.SetBlockInfo(currentTransformInfo.Get());
    race.BindVehicle(&vehicle);
    race.SetCurrentTransformCheckpointFreewheelClearEnabled(true);

    vehicle.VehicleFreeWheelingSet(1);
    race.OnCheckpoint(nullptr, &currentTransformBlock);
    bool okay = Check(
            !vehicle.CaptureRuntimeClone()
                     .controls.forcedLowSpeedFriction,
            "current-transform checkpoint contact retained freewheel");
    okay &= Check(
            race.Progress().freewheelClearCount == 1u,
            "current-transform checkpoint contact was not counted");

    race.SetCurrentTransformCheckpointFreewheelClearEnabled(false);
    vehicle.VehicleFreeWheelingSet(1);
    race.OnCheckpoint(nullptr, &currentTransformBlock);
    okay &= Check(
            vehicle.CaptureRuntimeClone()
                    .controls.forcedLowSpeedFriction,
            "disabled Reference rule changed another backend");
    okay &= Check(
            race.Progress().freewheelClearCount == 1u,
            "disabled Reference rule changed the clear count");

    race.SetCurrentTransformCheckpointFreewheelClearEnabled(true);
    CMwNodRef<CGameCtnBlockInfo> ordinaryInfo =
            MakeMwNod<CGameCtnBlockInfo>();
    ordinaryInfo->SetRespawnUsesCurrentTransform(false);
    CGameCtnBlock ordinaryBlock;
    ordinaryBlock.SetBlockInfo(ordinaryInfo.Get());
    vehicle.VehicleFreeWheelingSet(1);
    race.OnCheckpoint(nullptr, &ordinaryBlock);
    okay &= Check(
            vehicle.CaptureRuntimeClone()
                    .controls.forcedLowSpeedFriction,
            "ordinary checkpoint contact cleared freewheel");
    okay &= Check(
            race.Progress().freewheelClearCount == 1u,
            "ordinary checkpoint contact changed the clear count");
    return okay;
}

bool BackendClearsCurrentTransformCheckpointFreewheel(
        forevervalidator::SimulationBackend backend) {
    CMwNodRef<CGameCtnBlockInfo> info = MakeMwNod<CGameCtnBlockInfo>();
    info->SetRespawnUsesCurrentTransform(true);
    CGameCtnBlock block;
    block.SetBlockInfo(info.Get());

    CTrackManiaRace race;
    ReplaySimulationRuntime runtime(race, backend);
    CSceneVehicleCar vehicle;
    race.BindVehicle(&vehicle);
    vehicle.VehicleFreeWheelingSet(1);
    race.OnCheckpoint(nullptr, &block);
    return !vehicle.CaptureRuntimeClone()
                     .controls.forcedLowSpeedFriction;
}

bool TestParityBackendRouting() {
    bool okay = Check(
            BackendClearsCurrentTransformCheckpointFreewheel(
                    forevervalidator::SimulationBackend::Reference),
            "Reference backend did not enable the checkpoint rule");
    okay &= Check(
            BackendClearsCurrentTransformCheckpointFreewheel(
                    forevervalidator::SimulationBackend::OptimizedCpu),
            "Optimized CPU backend did not enable the checkpoint rule");
    okay &= Check(
            BackendClearsCurrentTransformCheckpointFreewheel(
                    forevervalidator::SimulationBackend::Cuda),
            "CUDA staging runtime did not enable the checkpoint rule");
    okay &= Check(
            !BackendClearsCurrentTransformCheckpointFreewheel(
                    forevervalidator::SimulationBackend::SpeculativeTicking),
            "unrelated speculative backend enabled the checkpoint rule");
    return okay;
}

}  // namespace

int main() {
    const bool contact = TestCurrentTransformCheckpointContactClearsFreewheel();
    const bool routing = TestParityBackendRouting();
    return contact && routing ? 0 : 1;
}
