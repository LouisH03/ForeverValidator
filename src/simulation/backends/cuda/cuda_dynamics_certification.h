#ifndef FOREVERVALIDATOR_CUDA_DYNAMICS_CERTIFICATION_H
#define FOREVERVALIDATOR_CUDA_DYNAMICS_CERTIFICATION_H

#include <cstdint>
#include <string>

namespace forevervalidator::simulation {

struct CudaDynamicsCertificationResult {
    bool success = false;
    std::uint64_t checkedStates = 0u;
    std::uint64_t checkedFields = 0u;
    std::uint64_t firstMismatchState = UINT64_MAX;
    std::string diagnostic;
};

CudaDynamicsCertificationResult
CertifyCudaPreCollisionDynamics() noexcept;

}  // namespace forevervalidator::simulation

#endif
