#include "simulation/backends/cuda/cuda_backend.h"

#include <iostream>

int main() {
    const forevervalidator::CudaBackendDiagnostics device =
            forevervalidator::QueryCudaBackendDiagnostics();
    if (!device.IsReady()) {
        std::cerr << device.diagnostic << '\n';
        return 1;
    }
    const forevervalidator::simulation::CudaArithmeticCertification result =
            forevervalidator::simulation::CertifyCudaArithmetic(1000000u);
    if (!result.passed) {
        std::cerr << result.diagnostic
                  << " checked=" << result.checkedValues
                  << " mismatches=" << result.mismatchedValues
                  << " operation=" << result.firstMismatchOperation
                  << " input=0x" << std::hex << result.firstMismatchInput
                  << " expected=0x" << result.expectedBits
                  << " actual=0x" << result.actualBits << '\n';
        return 1;
    }
    std::cout << result.diagnostic
              << " checked=" << result.checkedValues << '\n';
    return 0;
}
