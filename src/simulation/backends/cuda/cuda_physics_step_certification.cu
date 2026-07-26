#include "simulation/backends/cuda/cuda_physics_step_certification.h"

#include <cuda_runtime.h>

#include "simulation/backends/cuda/cuda_physics_step.cuh"

namespace forevervalidator::simulation {
namespace {

__global__ void ExecutePhysicsStepKernel(
        const CudaPackedSceneHeader *scene,
        const CudaPackedStaticConfigurationHeader *configuration,
        CudaCandidateState *state,
        cuda::collision::CudaCollisionScratch *scratch,
        std::uint32_t *status) {
    if (blockIdx.x != 0u || threadIdx.x != 0u) return;
    state->vehicle.mobil.absorbContactEnabled = true;
    state->vehicle.mobil.physicsUpdatesEnabled = true;
    *status = static_cast<std::uint32_t>(
            cuda::physics::Step(
                    scene, configuration, *state, *scratch));
}

__global__ void ExecutePreCollisionKernel(
        const CudaPackedStaticConfigurationHeader *configuration,
        CudaCandidateState *state,
        float dt,
        std::uint32_t *status) {
    if (blockIdx.x != 0u || threadIdx.x != 0u) return;
    cuda::environment::BeginForcePass(
            state->body, configuration);
    cuda::vehicle::ForceStatus forceStatus =
            cuda::vehicle::ForceStatus::Success;
    if (state->vehicle.mobil.physicsUpdatesEnabled) {
        forceStatus = cuda::vehicle::ComputeForcesModel6(
                *state, configuration, dt);
    }
    if (forceStatus != cuda::vehicle::ForceStatus::Success) {
        *status = static_cast<std::uint32_t>(forceStatus) + 1u;
        return;
    }
    cuda::dynamics::PreCollision(state->body, dt);
    *status = 0u;
}

__global__ void ExecuteCollisionSubstepKernel(
        const CudaPackedSceneHeader *scene,
        const CudaPackedStaticConfigurationHeader *configuration,
        CudaCandidateState *state,
        cuda::collision::CudaCollisionScratch *scratch,
        float dt,
        std::uint32_t *status) {
    if (blockIdx.x != 0u || threadIdx.x != 0u) return;
    *status = static_cast<std::uint32_t>(
            cuda::physics::CollisionSubstep(
                    scene, configuration, *state, dt, *scratch));
}

std::string Failure(const char *operation, cudaError_t error) {
    return std::string(operation) + " failed: " +
           cudaGetErrorName(error) + " (" +
           cudaGetErrorString(error) + ")";
}

}  // namespace

CudaPhysicsStepExecution ExecuteCudaPhysicsStepForCertification(
        const void *deviceScene,
        const void *deviceStaticConfiguration,
        const CudaCandidateState &state) noexcept {
    CudaPhysicsStepExecution result;
    CudaCandidateState *deviceState = nullptr;
    cuda::collision::CudaCollisionScratch *deviceScratch = nullptr;
    std::uint32_t *deviceStatus = nullptr;
    auto cleanup = [&]() {
        if (deviceStatus != nullptr) cudaFree(deviceStatus);
        if (deviceScratch != nullptr) cudaFree(deviceScratch);
        if (deviceState != nullptr) cudaFree(deviceState);
    };
    cudaError_t error = cudaMalloc(
            reinterpret_cast<void **>(&deviceState),
            sizeof(CudaCandidateState));
    if (error == cudaSuccess) {
        error = cudaMalloc(
                reinterpret_cast<void **>(&deviceScratch),
                sizeof(cuda::collision::CudaCollisionScratch));
    }
    if (error == cudaSuccess) {
        error = cudaMalloc(
                reinterpret_cast<void **>(&deviceStatus),
                sizeof(std::uint32_t));
    }
    if (error != cudaSuccess) {
        result.diagnostic =
                Failure("cudaMalloc(physics step)", error);
        cleanup();
        return result;
    }
    error = cudaMemcpy(
            deviceState, &state, sizeof(state),
            cudaMemcpyHostToDevice);
    if (error != cudaSuccess) {
        result.diagnostic =
                Failure("cudaMemcpy(physics state H2D)", error);
        cleanup();
        return result;
    }
    ExecutePhysicsStepKernel<<<1u, 1u>>>(
            static_cast<const CudaPackedSceneHeader *>(
                    deviceScene),
            static_cast<
                    const CudaPackedStaticConfigurationHeader *>(
                    deviceStaticConfiguration),
            deviceState, deviceScratch, deviceStatus);
    error = cudaGetLastError();
    if (error != cudaSuccess) {
        result.diagnostic =
                Failure("physics step launch", error);
        cleanup();
        return result;
    }
    std::uint32_t status = UINT32_MAX;
    error = cudaMemcpy(
            &status, deviceStatus, sizeof(status),
            cudaMemcpyDeviceToHost);
    if (error == cudaSuccess) {
        error = cudaMemcpy(
                &result.finalState, deviceState,
                sizeof(result.finalState),
                cudaMemcpyDeviceToHost);
    }
    cleanup();
    if (error != cudaSuccess) {
        result.diagnostic =
                Failure("cudaMemcpy(physics state D2H)", error);
        return result;
    }
    if (status != static_cast<std::uint32_t>(
                          cuda::physics::Status::Success)) {
        result.diagnostic =
                "CUDA physics step returned status " +
                std::to_string(status);
        return result;
    }
    result.success = true;
    result.diagnostic =
            "CUDA physics step completed";
    return result;
}

CudaPhysicsStepExecution ExecuteCudaPreCollisionForCertification(
        const void *deviceStaticConfiguration,
        const CudaCandidateState &state,
        float dt) noexcept {
    CudaPhysicsStepExecution result;
    if (deviceStaticConfiguration == nullptr || !(dt > 0.0f)) {
        result.diagnostic =
                "invalid CUDA pre-collision certification request";
        return result;
    }
    CudaCandidateState *deviceState = nullptr;
    std::uint32_t *deviceStatus = nullptr;
    auto cleanup = [&]() {
        if (deviceStatus != nullptr) cudaFree(deviceStatus);
        if (deviceState != nullptr) cudaFree(deviceState);
    };
    cudaError_t error = cudaMalloc(
            reinterpret_cast<void **>(&deviceState),
            sizeof(CudaCandidateState));
    if (error == cudaSuccess) {
        error = cudaMalloc(
                reinterpret_cast<void **>(&deviceStatus),
                sizeof(std::uint32_t));
    }
    if (error == cudaSuccess) {
        error = cudaMemcpy(
                deviceState, &state, sizeof(state),
                cudaMemcpyHostToDevice);
    }
    if (error != cudaSuccess) {
        result.diagnostic =
                Failure("CUDA pre-collision allocation/copy", error);
        cleanup();
        return result;
    }
    ExecutePreCollisionKernel<<<1u, 1u>>>(
            static_cast<const
                    CudaPackedStaticConfigurationHeader *>(
                    deviceStaticConfiguration),
            deviceState, dt, deviceStatus);
    error = cudaGetLastError();
    std::uint32_t status = UINT32_MAX;
    if (error == cudaSuccess) {
        error = cudaMemcpy(
                &status, deviceStatus, sizeof(status),
                cudaMemcpyDeviceToHost);
    }
    if (error == cudaSuccess) {
        error = cudaMemcpy(
                &result.finalState, deviceState,
                sizeof(result.finalState),
                cudaMemcpyDeviceToHost);
    }
    cleanup();
    if (error != cudaSuccess) {
        result.diagnostic =
                Failure("CUDA pre-collision execution", error);
        return result;
    }
    if (status != 0u) {
        result.diagnostic =
                "CUDA pre-collision force status " +
                std::to_string(status);
        return result;
    }
    result.success = true;
    result.diagnostic =
            "CUDA pre-collision state completed";
    return result;
}

CudaPhysicsStepExecution
ExecuteCudaCollisionSubstepForCertification(
        const void *deviceScene,
        const void *deviceStaticConfiguration,
        const CudaCandidateState &state,
        float dt) noexcept {
    CudaPhysicsStepExecution result;
    if (deviceScene == nullptr ||
        deviceStaticConfiguration == nullptr || !(dt > 0.0f)) {
        result.diagnostic =
                "invalid CUDA collision-substep certification request";
        return result;
    }
    CudaCandidateState *deviceState = nullptr;
    cuda::collision::CudaCollisionScratch *deviceScratch = nullptr;
    std::uint32_t *deviceStatus = nullptr;
    auto cleanup = [&]() {
        if (deviceStatus != nullptr) cudaFree(deviceStatus);
        if (deviceScratch != nullptr) cudaFree(deviceScratch);
        if (deviceState != nullptr) cudaFree(deviceState);
    };
    cudaError_t error = cudaMalloc(
            reinterpret_cast<void **>(&deviceState),
            sizeof(CudaCandidateState));
    if (error == cudaSuccess) {
        error = cudaMalloc(
                reinterpret_cast<void **>(&deviceScratch),
                sizeof(cuda::collision::CudaCollisionScratch));
    }
    if (error == cudaSuccess) {
        error = cudaMalloc(
                reinterpret_cast<void **>(&deviceStatus),
                sizeof(std::uint32_t));
    }
    if (error == cudaSuccess) {
        error = cudaMemcpy(
                deviceState, &state, sizeof(state),
                cudaMemcpyHostToDevice);
    }
    if (error != cudaSuccess) {
        result.diagnostic =
                Failure("CUDA collision-substep allocation/copy", error);
        cleanup();
        return result;
    }
    ExecuteCollisionSubstepKernel<<<1u, 1u>>>(
            static_cast<const CudaPackedSceneHeader *>(deviceScene),
            static_cast<const CudaPackedStaticConfigurationHeader *>(
                    deviceStaticConfiguration),
            deviceState, deviceScratch, dt, deviceStatus);
    error = cudaGetLastError();
    std::uint32_t status = UINT32_MAX;
    if (error == cudaSuccess) {
        error = cudaMemcpy(
                &status, deviceStatus, sizeof(status),
                cudaMemcpyDeviceToHost);
    }
    if (error == cudaSuccess) {
        error = cudaMemcpy(
                &result.finalState, deviceState,
                sizeof(result.finalState),
                cudaMemcpyDeviceToHost);
    }
    cleanup();
    if (error != cudaSuccess) {
        result.diagnostic =
                Failure("CUDA collision-substep execution", error);
        return result;
    }
    if (status != static_cast<std::uint32_t>(
                          cuda::physics::Status::Success)) {
        result.diagnostic =
                "CUDA collision substep returned status " +
                std::to_string(status);
        return result;
    }
    result.success = true;
    result.diagnostic =
            "CUDA collision substep completed";
    return result;
}

}  // namespace forevervalidator::simulation
