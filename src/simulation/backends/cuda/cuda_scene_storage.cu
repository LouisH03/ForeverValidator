#include "simulation/backends/cuda/cuda_scene_storage.h"

#include <cuda_runtime_api.h>

#include <chrono>

namespace forevervalidator::simulation {

bool UploadCudaSceneBytes(const std::byte *source,
                          std::size_t size,
                          void **destination,
                          double *milliseconds,
                          std::string *diagnostic) noexcept {
    if (source == nullptr || size == 0u || destination == nullptr) {
        if (diagnostic != nullptr) {
            *diagnostic = "invalid CUDA scene upload request";
        }
        return false;
    }
    const auto start = std::chrono::steady_clock::now();
    cudaError_t error = cudaMalloc(destination, size);
    if (error == cudaSuccess) {
        error = cudaMemcpy(
                *destination, source, size, cudaMemcpyHostToDevice);
    }
    if (error == cudaSuccess) {
        error = cudaDeviceSynchronize();
    }
    const auto end = std::chrono::steady_clock::now();
    if (milliseconds != nullptr) {
        *milliseconds =
                std::chrono::duration<double, std::milli>(
                        end - start).count();
    }
    if (error != cudaSuccess) {
        if (*destination != nullptr) {
            cudaFree(*destination);
            *destination = nullptr;
        }
        if (diagnostic != nullptr) {
            *diagnostic = std::string("CUDA scene upload failed: ") +
                    cudaGetErrorString(error);
        }
        return false;
    }
    return true;
}

void ReleaseCudaSceneBytes(void *allocation) noexcept {
    if (allocation != nullptr) {
        cudaFree(allocation);
    }
}

}  // namespace forevervalidator::simulation
