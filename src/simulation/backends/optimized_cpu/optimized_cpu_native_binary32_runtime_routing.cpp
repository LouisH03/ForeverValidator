#include "simulation/runtime/replay_physics_world.h"

#include "engine/core/binary32_math.h"

void ReplayPhysicsWorld::StepOptimizedCpuNativeBinary32(
        forevervalidator::simulation::
                OptimizedCpuVehicleForceContext &vehicleForceContext) {
    Binary32::NativeSqrtScope nativeSqrtScope;
    zone_.PhysicsStep2OptimizedCpuNativeBinary32(vehicleForceContext);
}
