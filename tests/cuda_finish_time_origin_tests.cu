#include "simulation/backends/cuda/cuda_finish_time_origin.cuh"

#include <cuda_runtime_api.h>

#include <cstdint>
#include <iostream>

namespace {

__global__ void CaptureTickStart(
        std::uint32_t tickTimeMs,
        std::uint64_t *tickStartNs) {
    *tickStartNs =
            forevervalidator::simulation::cuda::finish::
                    TickStartNanoseconds(tickTimeMs);
}

}  // namespace

int main() {
    constexpr std::uint32_t TickTimeMs = 29580u;
    constexpr std::uint32_t TickPeriodMs = 10u;
    constexpr std::uint64_t ExpectedTickStartNs = 29580000000u;
    constexpr std::uint64_t RepresentativeFinishNs = 29589583155u;
    static_assert(
            forevervalidator::simulation::cuda::finish::
                    TickStartNanoseconds(TickTimeMs) ==
            ExpectedTickStartNs);

    std::uint64_t *deviceTickStartNs = nullptr;
    if (cudaMalloc(&deviceTickStartNs, sizeof(*deviceTickStartNs)) !=
        cudaSuccess) {
        std::cerr << "CUDA finish-origin allocation failed\n";
        return 1;
    }
    CaptureTickStart<<<1, 1>>>(TickTimeMs, deviceTickStartNs);
    std::uint64_t tickStartNs = 0u;
    const cudaError_t copyStatus = cudaMemcpy(
            &tickStartNs, deviceTickStartNs,
            sizeof(tickStartNs), cudaMemcpyDeviceToHost);
    const cudaError_t freeStatus = cudaFree(deviceTickStartNs);
    if (copyStatus != cudaSuccess || freeStatus != cudaSuccess) {
        std::cerr << "CUDA finish-origin execution failed\n";
        return 1;
    }
    const std::uint64_t tickEndNs =
            tickStartNs +
            static_cast<std::uint64_t>(TickPeriodMs) * 1000000u;
    if (tickStartNs != ExpectedTickStartNs ||
        !(tickStartNs < RepresentativeFinishNs &&
          RepresentativeFinishNs <= tickEndNs)) {
        std::cerr << "CUDA finish interval did not start at tick T\n";
        return 1;
    }
    return 0;
}
