#include "simulation/backends/cuda/cuda_vehicle_force_certification.h"

#include <cuda_runtime.h>

#include <string>

#include "simulation/backends/cuda/cuda_static_configuration.h"
#include "simulation/backends/cuda/cuda_environment.cuh"
#include "simulation/backends/cuda/cuda_vehicle_forces.cuh"

namespace forevervalidator::simulation {
namespace {

__global__ void ExecuteVehicleForceKernel(
        const void *configurationData,
        CudaCandidateState *state,
        float dt,
        bool beginEnvironment,
        std::uint32_t *status) {
    if (blockIdx.x != 0u || threadIdx.x != 0u) return;
    const auto *configuration =
            static_cast<const CudaPackedStaticConfigurationHeader *>(
                    configurationData);
    if (configuration == nullptr ||
        configuration->magic !=
                CudaPackedStaticConfigurationHeader::Magic ||
        configuration->schemaVersion !=
                CudaPackedStaticConfigurationHeader::SchemaVersion ||
        state == nullptr ||
        state->schemaVersion != CudaCandidateState::SchemaVersion ||
        !(dt > 0.0f)) {
        *status = UINT32_MAX;
        return;
    }
    if (beginEnvironment) {
        cuda::environment::BeginForcePass(
                state->body, configuration);
    }
    *status = static_cast<std::uint32_t>(
            cuda::vehicle::ComputeForcesModel6(
                    *state, configuration, dt));
}

std::string Failure(const char *operation, cudaError_t error) {
    return std::string(operation) + " failed: " +
           cudaGetErrorName(error) + " (" +
           cudaGetErrorString(error) + ")";
}

}  // namespace

CudaVehicleForceExecution ExecuteCudaVehicleForceForCertificationImpl(
        const void *deviceStaticConfiguration,
        const CudaCandidateState &initialState,
        float dt,
        bool beginEnvironment) noexcept {
    CudaVehicleForceExecution result;
    if (deviceStaticConfiguration == nullptr ||
        initialState.schemaVersion !=
                CudaCandidateState::SchemaVersion ||
        !(dt > 0.0f)) {
        result.diagnostic =
                "invalid CUDA vehicle-force certification request";
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
    if (error != cudaSuccess) {
        result.diagnostic = Failure("cudaMalloc(state)", error);
        return result;
    }
    error = cudaMalloc(
            reinterpret_cast<void **>(&deviceStatus),
            sizeof(std::uint32_t));
    if (error != cudaSuccess) {
        result.diagnostic = Failure("cudaMalloc(status)", error);
        cleanup();
        return result;
    }
    error = cudaMemcpy(
            deviceState, &initialState, sizeof(CudaCandidateState),
            cudaMemcpyHostToDevice);
    if (error != cudaSuccess) {
        result.diagnostic = Failure("cudaMemcpy(state H2D)", error);
        cleanup();
        return result;
    }
    ExecuteVehicleForceKernel<<<1u, 1u>>>(
            deviceStaticConfiguration, deviceState, dt,
            beginEnvironment, deviceStatus);
    error = cudaGetLastError();
    if (error != cudaSuccess) {
        result.diagnostic = Failure("vehicle-force launch", error);
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
                sizeof(CudaCandidateState),
                cudaMemcpyDeviceToHost);
    }
    cleanup();
    if (error != cudaSuccess) {
        result.diagnostic = Failure("cudaMemcpy(D2H)", error);
        return result;
    }
    result.success = status != UINT32_MAX;
    result.supported =
            status == static_cast<std::uint32_t>(
                    cuda::vehicle::ForceStatus::Success);
    result.diagnostic = result.supported
            ? "CUDA vehicle-force certification kernel completed"
            : "CUDA vehicle-force transition is not implemented";
    return result;
}

CudaVehicleForceExecution ExecuteCudaVehicleForceForCertification(
        const void *deviceStaticConfiguration,
        const CudaCandidateState &initialState,
        float dt) noexcept {
    return ExecuteCudaVehicleForceForCertificationImpl(
            deviceStaticConfiguration, initialState, dt, false);
}

CudaVehicleForceExecution ExecuteCudaVehicleForcePassForCertification(
        const void *deviceStaticConfiguration,
        const CudaCandidateState &initialState,
        float dt) noexcept {
    return ExecuteCudaVehicleForceForCertificationImpl(
            deviceStaticConfiguration, initialState, dt, true);
}

}  // namespace forevervalidator::simulation
