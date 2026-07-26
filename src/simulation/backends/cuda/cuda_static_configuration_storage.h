#ifndef FOREVERVALIDATOR_CUDA_STATIC_CONFIGURATION_STORAGE_H
#define FOREVERVALIDATOR_CUDA_STATIC_CONFIGURATION_STORAGE_H

#include <cstdint>
#include <string>

#include "simulation/backends/cuda/cuda_static_configuration.h"

namespace forevervalidator::simulation {

struct CudaStaticConfigurationTransferMetrics {
    bool success = false;
    std::uint64_t hostPackedBytes = 0u;
    std::uint64_t deviceBytes = 0u;
    double packMilliseconds = 0.0;
    double uploadMilliseconds = 0.0;
    std::string diagnostic;
};

class CudaDeviceStaticConfiguration {
public:
    CudaDeviceStaticConfiguration() = default;
    ~CudaDeviceStaticConfiguration();
    CudaDeviceStaticConfiguration(
            CudaDeviceStaticConfiguration &&other) noexcept;
    CudaDeviceStaticConfiguration &operator=(
            CudaDeviceStaticConfiguration &&other) noexcept;

    CudaDeviceStaticConfiguration(
            const CudaDeviceStaticConfiguration &) = delete;
    CudaDeviceStaticConfiguration &operator=(
            const CudaDeviceStaticConfiguration &) = delete;

    CudaStaticConfigurationTransferMetrics Upload(
            const CudaHostStaticConfiguration &source) noexcept;
    void Reset() noexcept;
    bool Ready() const noexcept { return deviceData_ != nullptr; }
    std::uint64_t ConfigurationHash() const noexcept {
        return configurationHash_;
    }
    std::uint64_t DeviceBytes() const noexcept { return deviceBytes_; }
    const void *DeviceData() const noexcept { return deviceData_; }

private:
    void *deviceData_ = nullptr;
    std::uint64_t deviceBytes_ = 0u;
    std::uint64_t configurationHash_ = 0u;
};

}  // namespace forevervalidator::simulation

#endif
