#include "simulation/backends/cuda/cuda_stunt_certification.h"

#include <cuda_runtime.h>

#include <string>

#include "simulation/backends/cuda/cuda_stunts.cuh"

namespace forevervalidator::simulation {
namespace {

struct DeviceResult {
    std::uint32_t failureCommand = UINT32_MAX;
    std::uint32_t failureDetail = 0u;
};

__global__ void ExecuteStuntCommandsKernel(
        CudaRaceState *race,
        const CudaStuntCommand *commands,
        std::uint32_t commandCount,
        DeviceResult *result) {
    if (blockIdx.x != 0u || threadIdx.x != 0u) return;
    for (std::uint32_t index = 0u; index < commandCount; ++index) {
        cuda::stunts::Status status =
                cuda::stunts::Status::Success;
        switch (commands[index].kind) {
        case CudaStuntCommandKind::Update:
            status = cuda::stunts::UpdateState(
                    *race, commands[index].state);
            break;
        case CudaStuntCommandKind::RespawnPenalty:
            cuda::stunts::ApplyRespawnPenalty(*race);
            break;
        case CudaStuntCommandKind::TimePenalty:
            status = cuda::stunts::ApplyTimePenalty(
                    *race, commands[index].overtimeMs);
            break;
        default:
            result->failureCommand = index;
            result->failureDetail = UINT32_MAX;
            return;
        }
        if (status != cuda::stunts::Status::Success) {
            result->failureCommand = index;
            result->failureDetail =
                    static_cast<std::uint32_t>(status);
            return;
        }
    }
}

std::string Failure(const char *operation, cudaError_t error) {
    return std::string(operation) + " failed: " +
           cudaGetErrorName(error) + " (" +
           cudaGetErrorString(error) + ")";
}

}  // namespace

CudaStuntExecution ExecuteCudaStuntCommandsForCertification(
        const CudaRaceState &initialState,
        const std::vector<CudaStuntCommand> &commands) noexcept {
    CudaStuntExecution result;
    if (commands.empty() ||
        commands.size() > UINT32_MAX) {
        result.diagnostic =
                "invalid CUDA stunt certification request";
        return result;
    }
    CudaRaceState *deviceRace = nullptr;
    CudaStuntCommand *deviceCommands = nullptr;
    DeviceResult *deviceResult = nullptr;
    auto cleanup = [&]() {
        if (deviceResult != nullptr) cudaFree(deviceResult);
        if (deviceCommands != nullptr) cudaFree(deviceCommands);
        if (deviceRace != nullptr) cudaFree(deviceRace);
    };
    cudaError_t error = cudaMalloc(
            reinterpret_cast<void **>(&deviceRace),
            sizeof(CudaRaceState));
    if (error == cudaSuccess) {
        error = cudaMalloc(
                reinterpret_cast<void **>(&deviceCommands),
                commands.size() * sizeof(CudaStuntCommand));
    }
    if (error == cudaSuccess) {
        error = cudaMalloc(
                reinterpret_cast<void **>(&deviceResult),
                sizeof(DeviceResult));
    }
    DeviceResult emptyResult;
    if (error == cudaSuccess) {
        error = cudaMemcpy(
                deviceRace, &initialState, sizeof(CudaRaceState),
                cudaMemcpyHostToDevice);
    }
    if (error == cudaSuccess) {
        error = cudaMemcpy(
                deviceCommands, commands.data(),
                commands.size() * sizeof(CudaStuntCommand),
                cudaMemcpyHostToDevice);
    }
    if (error == cudaSuccess) {
        error = cudaMemcpy(
                deviceResult, &emptyResult, sizeof(DeviceResult),
                cudaMemcpyHostToDevice);
    }
    if (error != cudaSuccess) {
        result.diagnostic = Failure("CUDA stunt setup", error);
        cleanup();
        return result;
    }

    ExecuteStuntCommandsKernel<<<1u, 1u>>>(
            deviceRace, deviceCommands,
            static_cast<std::uint32_t>(commands.size()),
            deviceResult);
    error = cudaGetLastError();
    DeviceResult hostResult;
    if (error == cudaSuccess) {
        error = cudaMemcpy(
                &hostResult, deviceResult, sizeof(DeviceResult),
                cudaMemcpyDeviceToHost);
    }
    if (error == cudaSuccess) {
        error = cudaMemcpy(
                &result.finalState, deviceRace, sizeof(CudaRaceState),
                cudaMemcpyDeviceToHost);
    }
    cleanup();
    if (error != cudaSuccess) {
        result.diagnostic = Failure("CUDA stunt execution", error);
        return result;
    }
    result.failureCommand = hostResult.failureCommand;
    result.failureDetail = hostResult.failureDetail;
    result.success = hostResult.failureCommand == UINT32_MAX;
    result.diagnostic = result.success
            ? "CUDA stunt certification kernel completed"
            : "CUDA stunt certification command failed";
    return result;
}

}  // namespace forevervalidator::simulation
