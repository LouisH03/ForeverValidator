#include "simulation/backends/cuda/cuda_vehicle_cpu_reference.h"

#include "engine/scene/scene_vehicle_car.h"

void CudaVehicleCpuReferenceAccess::IntegrateVehicle(
        CSceneVehicleCar &car,
        float dt) {
    car.IntegrateVehicle(dt);
}

void CudaVehicleCpuReferenceAccess::ComputeForces(
        CSceneVehicleCar &car,
        float dt) {
    car.ComputeForces(dt);
}
