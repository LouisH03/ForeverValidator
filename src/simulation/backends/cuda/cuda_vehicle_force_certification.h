#ifndef FOREVERVALIDATOR_CUDA_VEHICLE_FORCE_CERTIFICATION_H
#define FOREVERVALIDATOR_CUDA_VEHICLE_FORCE_CERTIFICATION_H

#include <string>

#include "simulation/backends/cuda/cuda_state_layout.h"

namespace forevervalidator::simulation {

struct CudaVehicleForceExecution {
    bool success = false;
    bool supported = false;
    CudaCandidateState finalState{};
    std::string diagnostic;
};

CudaVehicleForceExecution ExecuteCudaVehicleForceForCertification(
        const void *deviceStaticConfiguration,
        const CudaCandidateState &initialState,
        float dt) noexcept;

CudaVehicleForceExecution ExecuteCudaVehicleForcePassForCertification(
        const void *deviceStaticConfiguration,
        const CudaCandidateState &initialState,
        float dt) noexcept;

}  // namespace forevervalidator::simulation

#endif
