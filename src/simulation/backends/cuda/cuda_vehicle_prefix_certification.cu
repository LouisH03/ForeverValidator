#include "simulation/backends/cuda/cuda_vehicle_prefix_certification.h"

#include <cuda_runtime.h>

#include <string>

#include "simulation/backends/cuda/cuda_static_configuration.h"
#include "simulation/backends/cuda/cuda_vehicle_wheels.cuh"

namespace forevervalidator::simulation {
namespace {

__global__ void ExecuteVehiclePrefixKernel(
        const void *configurationData,
        CudaCandidateState *state,
        float dt,
        std::uint32_t *status) {
    if (blockIdx.x != 0u || threadIdx.x != 0u) {
        return;
    }
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
        *status = 1u;
        return;
    }
    cuda::vehicle::IntegrateVehiclePrefix(
            *state, configuration, dt);
    *status = 0u;
}

std::string Failure(const char *operation, cudaError_t error) {
    return std::string(operation) + " failed: " +
           cudaGetErrorName(error) + " (" +
           cudaGetErrorString(error) + ")";
}

}  // namespace

CudaVehiclePrefixExecution ExecuteCudaVehiclePrefixForCertification(
        const void *deviceStaticConfiguration,
        const CudaCandidateState &initialState,
        float dt) noexcept {
    CudaVehiclePrefixExecution result;
    if (deviceStaticConfiguration == nullptr ||
        initialState.schemaVersion !=
                CudaCandidateState::SchemaVersion ||
        !(dt > 0.0f)) {
        result.diagnostic =
                "invalid CUDA vehicle-prefix certification request";
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

    ExecuteVehiclePrefixKernel<<<1u, 1u>>>(
            deviceStaticConfiguration, deviceState, dt, deviceStatus);
    error = cudaGetLastError();
    if (error != cudaSuccess) {
        result.diagnostic = Failure("vehicle-prefix launch", error);
        cleanup();
        return result;
    }

    std::uint32_t status = 1u;
    error = cudaMemcpy(
            &status, deviceStatus, sizeof(status),
            cudaMemcpyDeviceToHost);
    if (error != cudaSuccess) {
        result.diagnostic = Failure("cudaMemcpy(status D2H)", error);
        cleanup();
        return result;
    }
    error = cudaMemcpy(
            &result.finalState, deviceState,
            sizeof(CudaCandidateState), cudaMemcpyDeviceToHost);
    if (error != cudaSuccess) {
        result.diagnostic = Failure("cudaMemcpy(state D2H)", error);
        cleanup();
        return result;
    }
    cleanup();
    if (status != 0u) {
        result.diagnostic =
                "CUDA vehicle-prefix kernel rejected the request";
        return result;
    }
    result.success = true;
    result.diagnostic =
            "CUDA vehicle-prefix certification kernel completed";
    return result;
}

}  // namespace forevervalidator::simulation
