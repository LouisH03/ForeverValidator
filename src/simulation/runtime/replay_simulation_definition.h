#ifndef TMNF_REPLAY_SIMULATION_DEFINITION_H
#define TMNF_REPLAY_SIMULATION_DEFINITION_H

#include <optional>

#include <forevervalidator/result.h>

#include "simulation/runtime/replay_environment_definition.h"
#include "engine/game/replay_vehicle_simulation_definition.h"
struct ReplayVehicleSourceBundle;

struct ReplaySimulationDefinition {
    ReplayEnvironmentDefinition environment{};
    VehicleSimulationDefinition vehicle{};
    // Only Stadium's vehicle and stunt-sensitive kernels are certified exact.
    bool optimizedCpuStadiumSpecializationsEnabled = true;
};

enum class ReplaySimulationDefinitionBuildResult : int {
    Success = 0,
    MissingVehicleDefinition = 2,
    InvalidVehiclePhysics = 6,
    InvalidInitialParameters = 27,
    AllocationFailure = 37,
    InvalidEnvironment = 47,
    InvalidMaterials = 67,
};

using ReplaySimulationDefinitionBuild =
        forevervalidator::DiscriminatedResult<
                ReplaySimulationDefinition,
                ReplaySimulationDefinitionBuildResult>;

ReplaySimulationDefinitionBuild BuildReplaySimulationDefinition(
        const ReplayVehicleSourceBundle &sources,
        const std::optional<ReplayWaterDefinition> &water);

#endif
