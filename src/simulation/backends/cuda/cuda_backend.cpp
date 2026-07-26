#include "simulation/backends/cuda/cuda_backend.h"

#include "simulation/backends/simulation_backend.h"

namespace forevervalidator::simulation {

#if FOREVERVALIDATOR_HAS_CUDA
CudaBackendDiagnostics QueryCompiledCudaRuntimeDiagnostics() noexcept;
#endif

CudaBackendDiagnostics QueryCudaRuntimeDiagnostics() noexcept {
#if FOREVERVALIDATOR_HAS_CUDA
    return QueryCompiledCudaRuntimeDiagnostics();
#else
    CudaBackendDiagnostics result;
    result.status = CudaBackendStatus::NotCompiled;
    result.diagnostic =
            "CUDA backend support was not compiled into this build";
    return result;
#endif
}

bool IsCudaBackendReady() noexcept {
    return QueryCudaRuntimeDiagnostics().IsReady();
}

}  // namespace forevervalidator::simulation

namespace forevervalidator {

CudaBackendDiagnostics QueryCudaBackendDiagnostics() noexcept {
    return simulation::QueryCudaRuntimeDiagnostics();
}

}  // namespace forevervalidator
