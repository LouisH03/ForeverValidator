#ifndef FOREVERVALIDATOR_CUDA_STUNT_CERTIFICATION_H
#define FOREVERVALIDATOR_CUDA_STUNT_CERTIFICATION_H

#include <cstdint>
#include <string>
#include <vector>

#include "simulation/backends/cuda/cuda_state_layout.h"

namespace forevervalidator::simulation {

enum class CudaStuntCommandKind : std::uint32_t {
    Update,
    RespawnPenalty,
    TimePenalty,
};

struct CudaStuntCommand {
    CudaStuntCommandKind kind = CudaStuntCommandKind::Update;
    std::uint32_t overtimeMs = 0u;
    ReplayStuntSimulationState state{};
};

struct CudaStuntExecution {
    bool success = false;
    std::uint32_t failureCommand = UINT32_MAX;
    std::uint32_t failureDetail = 0u;
    CudaRaceState finalState{};
    std::string diagnostic;
};

CudaStuntExecution ExecuteCudaStuntCommandsForCertification(
        const CudaRaceState &initialState,
        const std::vector<CudaStuntCommand> &commands) noexcept;

}  // namespace forevervalidator::simulation

#endif
