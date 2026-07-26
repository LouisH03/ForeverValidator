#ifndef FOREVERVALIDATOR_CUDA_PHYSICS_STEP_CERTIFICATION_H
#define FOREVERVALIDATOR_CUDA_PHYSICS_STEP_CERTIFICATION_H

#include <string>

#include "simulation/backends/cuda/cuda_state_layout.h"

namespace forevervalidator::simulation {

struct CudaPhysicsStepExecution {
    bool success = false;
    CudaCandidateState finalState{};
    std::string diagnostic;
};

CudaPhysicsStepExecution ExecuteCudaPhysicsStepForCertification(
        const void *deviceScene,
        const void *deviceStaticConfiguration,
        const CudaCandidateState &state) noexcept;

CudaPhysicsStepExecution ExecuteCudaPreCollisionForCertification(
        const void *deviceStaticConfiguration,
        const CudaCandidateState &state,
        float dt) noexcept;

CudaPhysicsStepExecution ExecuteCudaCollisionSubstepForCertification(
        const void *deviceScene,
        const void *deviceStaticConfiguration,
        const CudaCandidateState &state,
        float dt) noexcept;

}  // namespace forevervalidator::simulation

#endif
