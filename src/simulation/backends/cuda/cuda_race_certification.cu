#include "simulation/backends/cuda/cuda_race_certification.h"

#include <cuda_runtime.h>

#include <string>

#include "simulation/backends/cuda/cuda_race.cuh"

namespace forevervalidator::simulation {
namespace {

__global__ void ExecuteRaceContactKernel(
        CudaCandidateState *state,
        const CudaSceneActor *actor) {
    if (blockIdx.x != 0u || threadIdx.x != 0u) return;
    cuda::race::OnTriggerContact(*state, *actor);
}

std::string Failure(const char *operation, cudaError_t error) {
    return std::string(operation) + " failed: " +
           cudaGetErrorName(error) + " (" +
           cudaGetErrorString(error) + ")";
}

}  // namespace

CudaRaceContactExecution ExecuteCudaRaceContactForCertification(
        const CudaCandidateState &initialState,
        const CudaSceneActor &actor) noexcept {
    CudaRaceContactExecution result;
    CudaCandidateState *deviceState = nullptr;
    CudaSceneActor *deviceActor = nullptr;
    auto cleanup = [&]() {
        if (deviceActor != nullptr) cudaFree(deviceActor);
        if (deviceState != nullptr) cudaFree(deviceState);
    };

    cudaError_t error = cudaMalloc(
            reinterpret_cast<void **>(&deviceState),
            sizeof(CudaCandidateState));
    if (error == cudaSuccess) {
        error = cudaMalloc(
                reinterpret_cast<void **>(&deviceActor),
                sizeof(CudaSceneActor));
    }
    if (error == cudaSuccess) {
        error = cudaMemcpy(
                deviceState, &initialState,
                sizeof(CudaCandidateState), cudaMemcpyHostToDevice);
    }
    if (error == cudaSuccess) {
        error = cudaMemcpy(
                deviceActor, &actor,
                sizeof(CudaSceneActor), cudaMemcpyHostToDevice);
    }
    if (error != cudaSuccess) {
        result.diagnostic = Failure("CUDA race contact setup", error);
        cleanup();
        return result;
    }

    ExecuteRaceContactKernel<<<1u, 1u>>>(deviceState, deviceActor);
    error = cudaGetLastError();
    if (error == cudaSuccess) {
        error = cudaMemcpy(
                &result.finalState, deviceState,
                sizeof(CudaCandidateState), cudaMemcpyDeviceToHost);
    }
    cleanup();
    if (error != cudaSuccess) {
        result.diagnostic = Failure("CUDA race contact execution", error);
        return result;
    }
    result.success = true;
    result.diagnostic = "CUDA race contact certification kernel completed";
    return result;
}

}  // namespace forevervalidator::simulation
