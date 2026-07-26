#ifndef FOREVERVALIDATOR_CUDA_BACKEND_H
#define FOREVERVALIDATOR_CUDA_BACKEND_H

#include <forevervalidator/validation.h>

namespace forevervalidator::simulation {

struct CudaArithmeticCertification {
    bool passed = false;
    std::uint64_t checkedValues = 0u;
    std::uint64_t mismatchedValues = 0u;
    std::uint32_t firstMismatchOperation = 0u;
    std::uint32_t firstMismatchInput = 0u;
    std::uint32_t expectedBits = 0u;
    std::uint32_t actualBits = 0u;
    std::string diagnostic;
};

CudaBackendDiagnostics QueryCudaRuntimeDiagnostics() noexcept;
CudaArithmeticCertification CertifyCudaArithmetic(
        std::uint32_t sampleCount) noexcept;

}  // namespace forevervalidator::simulation

#endif
