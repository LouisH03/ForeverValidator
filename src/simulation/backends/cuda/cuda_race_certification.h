#ifndef FOREVERVALIDATOR_CUDA_RACE_CERTIFICATION_H
#define FOREVERVALIDATOR_CUDA_RACE_CERTIFICATION_H

#include <string>

#include "simulation/backends/cuda/cuda_scene_layout.h"
#include "simulation/backends/cuda/cuda_state_layout.h"

namespace forevervalidator::simulation {

struct CudaRaceContactExecution {
    bool success = false;
    CudaCandidateState finalState{};
    std::string diagnostic;
};

CudaRaceContactExecution ExecuteCudaRaceContactForCertification(
        const CudaCandidateState &initialState,
        const CudaSceneActor &actor) noexcept;

}  // namespace forevervalidator::simulation

#endif
