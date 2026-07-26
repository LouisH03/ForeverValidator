#include "simulation/backends/cuda/cuda_collision_certification.h"

#include <cuda_runtime.h>

#include <algorithm>
#include <new>
#include <string>

#include "simulation/backends/cuda/cuda_collision_response.cuh"

namespace forevervalidator::simulation {
namespace {

__global__ void ExecuteCollisionKernel(
        const void *sceneData,
        const void *configurationData,
        CudaCandidateState *state,
        cuda::collision::CudaCollisionScratch *scratch,
        std::uint32_t *status) {
    if (blockIdx.x != 0u || threadIdx.x != 0u) return;
    const auto *scene =
            static_cast<const CudaPackedSceneHeader *>(sceneData);
    const auto *configuration =
            static_cast<
                    const CudaPackedStaticConfigurationHeader *>(
                    configurationData);
    cuda::collision::Status collisionStatus =
            cuda::collision::Detect(
                    scene, configuration, *state, *scratch);
    if (collisionStatus ==
        cuda::collision::Status::Success) {
        collisionStatus = cuda::collision::Respond(
                scene, configuration, *state, *scratch);
    }
    *status = static_cast<std::uint32_t>(collisionStatus);
}

__global__ void ExecuteCollisionOrderingKernel(
        cuda::collision::CudaCollisionScratch *scratch) {
    if (blockIdx.x != 0u || threadIdx.x != 0u) return;
    cuda::collision::detail::SortForResponse(*scratch);
}

std::string Failure(const char *operation, cudaError_t error) {
    return std::string(operation) + " failed: " +
           cudaGetErrorName(error) + " (" +
           cudaGetErrorString(error) + ")";
}

}  // namespace

CudaCollisionExecution ExecuteCudaCollisionForCertification(
        const void *deviceScene,
        const void *deviceStaticConfiguration,
        const CudaCandidateState &state) noexcept {
    CudaCollisionExecution result;
    if (deviceScene == nullptr ||
        deviceStaticConfiguration == nullptr ||
        state.schemaVersion != CudaCandidateState::SchemaVersion) {
        result.diagnostic =
                "invalid CUDA collision certification request";
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
    if (error != cudaSuccess) {
        result.diagnostic = Failure("cudaMalloc(collision)", error);
        cleanup();
        return result;
    }
    error = cudaMemcpy(
            deviceState, &state, sizeof(state),
            cudaMemcpyHostToDevice);
    if (error != cudaSuccess) {
        result.diagnostic = Failure("cudaMemcpy(state H2D)", error);
        cleanup();
        return result;
    }
    ExecuteCollisionKernel<<<1u, 1u>>>(
            deviceScene, deviceStaticConfiguration, deviceState,
            deviceScratch, deviceStatus);
    error = cudaGetLastError();
    if (error != cudaSuccess) {
        result.diagnostic = Failure("collision launch", error);
        cleanup();
        return result;
    }
    std::uint32_t status = UINT32_MAX;
    cuda::collision::CudaCollisionScratch hostScratch;
    error = cudaMemcpy(
            &status, deviceStatus, sizeof(status),
            cudaMemcpyDeviceToHost);
    if (error == cudaSuccess) {
        error = cudaMemcpy(
                &hostScratch, deviceScratch, sizeof(hostScratch),
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
        result.diagnostic = Failure("cudaMemcpy(collision D2H)", error);
        return result;
    }
    result.status =
            static_cast<cuda::collision::Status>(status);
    result.accelerationCellVisits =
            hostScratch.accelerationCellVisits;
    result.accelerationSurfaceVisits =
            hostScratch.accelerationSurfaceVisits;
    result.meshCellVisits = hostScratch.meshCellVisits;
    result.meshCellIntersections =
            hostScratch.meshCellIntersections;
    result.meshTriangleCells = hostScratch.meshTriangleCells;
    result.triangleTests = hostScratch.triangleTests;
    result.triangleHits = hostScratch.triangleHits;
    result.firstVisitedShape = hostScratch.firstVisitedShape;
    result.firstVisitedSurface = hostScratch.firstVisitedSurface;
    result.firstShapeWorld = hostScratch.firstShapeWorld;
    result.firstMovingBounds = hostScratch.firstMovingBounds;
    result.firstEllipsoidBox = hostScratch.firstEllipsoidBox;
    result.firstSurfaceWorldBounds =
            hostScratch.firstSurfaceWorldBounds;
    result.firstMeshRootBounds = hostScratch.firstMeshRootBounds;
    result.firstResponseWheelIndex =
            hostScratch.firstResponseWheelIndex;
    result.firstResponseReplacementBefore =
            hostScratch.firstResponseReplacementBefore;
    result.firstResponseReplacementAfter =
            hostScratch.firstResponseReplacementAfter;
    if (result.status != cuda::collision::Status::Success) {
        result.diagnostic =
                "CUDA collision kernel returned status " +
                std::to_string(status);
        return result;
    }
    try {
        result.collisions.assign(
                hostScratch.collisions,
                hostScratch.collisions +
                        hostScratch.collisionCount);
    } catch (...) {
        result.diagnostic =
                "CUDA collision result allocation failed";
        return result;
    }
    result.success = true;
    result.diagnostic =
            "CUDA collision certification kernel completed";
    return result;
}

CudaCollisionOrderingExecution
ExecuteCudaCollisionOrderingForCertification(
        const std::vector<cuda::collision::CudaCollision> &collisions)
        noexcept {
    CudaCollisionOrderingExecution result;
    if (collisions.size() >
        cuda::collision::CollisionCapacity) {
        result.diagnostic =
                "CUDA collision ordering input exceeds capacity";
        return result;
    }
    cuda::collision::CudaCollisionScratch host;
    host.collisionCount =
            static_cast<std::uint32_t>(collisions.size());
    std::copy(
            collisions.begin(), collisions.end(),
            host.collisions);
    cuda::collision::CudaCollisionScratch *device = nullptr;
    cudaError_t error = cudaMalloc(
            reinterpret_cast<void **>(&device), sizeof(host));
    if (error == cudaSuccess) {
        error = cudaMemcpy(
                device, &host, sizeof(host),
                cudaMemcpyHostToDevice);
    }
    if (error == cudaSuccess) {
        ExecuteCollisionOrderingKernel<<<1u, 1u>>>(device);
        error = cudaGetLastError();
    }
    if (error == cudaSuccess) {
        error = cudaMemcpy(
                &host, device, sizeof(host),
                cudaMemcpyDeviceToHost);
    }
    if (device != nullptr) cudaFree(device);
    if (error != cudaSuccess) {
        result.diagnostic =
                Failure("CUDA collision ordering", error);
        return result;
    }
    try {
        result.collisions.assign(
                host.collisions,
                host.collisions + host.collisionCount);
    } catch (const std::bad_alloc &) {
        result.diagnostic =
                "CUDA collision ordering result allocation failed";
        return result;
    }
    result.success = true;
    result.diagnostic =
            "CUDA collision response ordering completed";
    return result;
}

}  // namespace forevervalidator::simulation
