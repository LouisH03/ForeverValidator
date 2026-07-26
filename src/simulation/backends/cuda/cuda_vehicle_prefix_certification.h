#ifndef FOREVERVALIDATOR_CUDA_VEHICLE_PREFIX_CERTIFICATION_H
#define FOREVERVALIDATOR_CUDA_VEHICLE_PREFIX_CERTIFICATION_H

#include <string>

#include "simulation/backends/cuda/cuda_state_layout.h"

namespace forevervalidator::simulation {

struct CudaVehiclePrefixExecution {
    bool success = false;
    CudaCandidateState finalState{};
    std::string diagnostic;
};

CudaVehiclePrefixExecution ExecuteCudaVehiclePrefixForCertification(
        const void *deviceStaticConfiguration,
        const CudaCandidateState &initialState,
        float dt) noexcept;

}  // namespace forevervalidator::simulation

#endif
