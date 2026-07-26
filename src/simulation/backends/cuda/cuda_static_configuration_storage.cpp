#include "simulation/backends/cuda/cuda_static_configuration_storage.h"

#include <chrono>
#include <utility>
#include <vector>

namespace forevervalidator::simulation {

#if FOREVERVALIDATOR_HAS_CUDA
bool UploadCudaSceneBytes(const std::byte *source,
                          std::size_t size,
                          void **destination,
                          double *milliseconds,
                          std::string *diagnostic) noexcept;
void ReleaseCudaSceneBytes(void *allocation) noexcept;
#endif

CudaDeviceStaticConfiguration::~CudaDeviceStaticConfiguration() {
    Reset();
}

CudaDeviceStaticConfiguration::CudaDeviceStaticConfiguration(
        CudaDeviceStaticConfiguration &&other) noexcept
    : deviceData_(std::exchange(other.deviceData_, nullptr)),
      deviceBytes_(std::exchange(other.deviceBytes_, 0u)),
      configurationHash_(
              std::exchange(other.configurationHash_, 0u)) {}

CudaDeviceStaticConfiguration &
CudaDeviceStaticConfiguration::operator=(
        CudaDeviceStaticConfiguration &&other) noexcept {
    if (this != &other) {
        Reset();
        deviceData_ = std::exchange(other.deviceData_, nullptr);
        deviceBytes_ = std::exchange(other.deviceBytes_, 0u);
        configurationHash_ =
                std::exchange(other.configurationHash_, 0u);
    }
    return *this;
}

CudaStaticConfigurationTransferMetrics
CudaDeviceStaticConfiguration::Upload(
        const CudaHostStaticConfiguration &source) noexcept {
    Reset();
    CudaStaticConfigurationTransferMetrics result;
    const auto packStart = std::chrono::steady_clock::now();
    std::vector<std::byte> bytes;
    if (!PackCudaStaticConfiguration(source, &bytes)) {
        result.diagnostic =
                "CUDA static configuration packing failed";
        return result;
    }
    const auto packEnd = std::chrono::steady_clock::now();
    result.packMilliseconds =
            std::chrono::duration<double, std::milli>(
                    packEnd - packStart).count();
    result.hostPackedBytes = bytes.size();
#if FOREVERVALIDATOR_HAS_CUDA
    if (!UploadCudaSceneBytes(
                bytes.data(), bytes.size(), &deviceData_,
                &result.uploadMilliseconds, &result.diagnostic)) {
        Reset();
        return result;
    }
    deviceBytes_ = bytes.size();
    configurationHash_ = source.deterministicHash;
    result.deviceBytes = deviceBytes_;
    result.success = true;
    result.diagnostic =
            "CUDA immutable vehicle and environment configuration uploaded";
#else
    result.diagnostic =
            "CUDA static configuration upload unavailable in a CPU-only build";
#endif
    return result;
}

void CudaDeviceStaticConfiguration::Reset() noexcept {
#if FOREVERVALIDATOR_HAS_CUDA
    ReleaseCudaSceneBytes(deviceData_);
#endif
    deviceData_ = nullptr;
    deviceBytes_ = 0u;
    configurationHash_ = 0u;
}

}  // namespace forevervalidator::simulation
