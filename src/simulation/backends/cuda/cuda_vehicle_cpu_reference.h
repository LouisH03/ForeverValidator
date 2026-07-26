#ifndef FOREVERVALIDATOR_CUDA_VEHICLE_CPU_REFERENCE_H
#define FOREVERVALIDATOR_CUDA_VEHICLE_CPU_REFERENCE_H

class CSceneVehicleCar;

struct CudaVehicleCpuReferenceAccess {
    static void IntegrateVehicle(
            CSceneVehicleCar &car, float dt);
    static void ComputeForces(
            CSceneVehicleCar &car, float dt);
};

#endif
