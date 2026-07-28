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

bool TestReferenceOnlyBackendRouting() {
    CMwNodRef<CGameCtnBlockInfo> info = MakeMwNod<CGameCtnBlockInfo>();
    info->SetRespawnUsesCurrentTransform(true);
    CGameCtnBlock block;
    block.SetBlockInfo(info.Get());

    CTrackManiaRace referenceRace;
    ReplaySimulationRuntime referenceRuntime(
            referenceRace, forevervalidator::SimulationBackend::Reference);
    CSceneVehicleCar referenceVehicle;
    referenceRace.BindVehicle(&referenceVehicle);
    referenceVehicle.VehicleFreeWheelingSet(1);
    referenceRace.OnCheckpoint(nullptr, &block);
    bool okay = Check(
            !referenceVehicle.CaptureRuntimeClone()
                     .controls.forcedLowSpeedFriction,
            "Reference backend did not enable the checkpoint rule");

    CTrackManiaRace optimizedRace;
    ReplaySimulationRuntime optimizedRuntime(
            optimizedRace, forevervalidator::SimulationBackend::OptimizedCpu);
    CSceneVehicleCar optimizedVehicle;
    optimizedRace.BindVehicle(&optimizedVehicle);
    optimizedVehicle.VehicleFreeWheelingSet(1);
    optimizedRace.OnCheckpoint(nullptr, &block);
    okay &= Check(
            optimizedVehicle.CaptureRuntimeClone()
                    .controls.forcedLowSpeedFriction,
            "Optimized CPU backend enabled the Reference checkpoint rule");
    return okay;
}

}  // namespace

int main() {
    const bool contact = TestCurrentTransformCheckpointContactClearsFreewheel();
    const bool routing = TestReferenceOnlyBackendRouting();
    return contact && routing ? 0 : 1;
}
