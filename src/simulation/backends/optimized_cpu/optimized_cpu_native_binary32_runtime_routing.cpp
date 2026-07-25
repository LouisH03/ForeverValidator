#include "simulation/runtime/replay_physics_world.h"

void ReplayPhysicsWorld::StepOptimizedCpuNativeBinary32(
        forevervalidator::simulation::
                OptimizedCpuVehicleForceContext &vehicleForceContext) {
    zone_.PhysicsStep2OptimizedCpuNativeBinary32(vehicleForceContext);
}
