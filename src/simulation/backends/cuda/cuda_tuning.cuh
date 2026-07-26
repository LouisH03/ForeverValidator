#ifndef FOREVERVALIDATOR_CUDA_TUNING_CUH
#define FOREVERVALIDATOR_CUDA_TUNING_CUH

#include "simulation/backends/cuda/cuda_exact_math.cuh"
#include "simulation/backends/cuda/cuda_static_configuration.h"

namespace forevervalidator::simulation::cuda::tuning {

template<typename T>
__device__ inline const T *Section(
        const CudaPackedStaticConfigurationHeader *configuration,
        const CudaStaticConfigurationSection &section) {
    return reinterpret_cast<const T *>(
            reinterpret_cast<const unsigned char *>(configuration) +
            section.offset);
}

__device__ inline float Evaluate(
        const CudaPackedStaticConfigurationHeader *configuration,
        CudaTuningCurveId id,
        float input,
        bool forceConstant = false) {
    const auto *curves =
            reinterpret_cast<const CudaTuningCurve *>(
                    &configuration->tuning.curves);
    const CudaTuningCurve &curve =
            curves[static_cast<std::uint32_t>(id)];
    if (curve.keyCount == 0u) {
        return 0.0f;
    }
    const CudaTuningCurveKey *allKeys =
            Section<CudaTuningCurveKey>(
                    configuration, configuration->curveKeys);
    const CudaTuningCurveKey *keys = allKeys + curve.firstKey;
    if (curve.keyCount == 1u) {
        return keys[0].value;
    }
    constexpr float KeyEpsilon = 1.0e-5f;
    const auto lowerBound = [](float value) {
        return exact::FromDouble(
                static_cast<double>(value) -
                static_cast<double>(KeyEpsilon));
    };
    const auto upperBound = [](float value) {
        return exact::FromDouble(
                static_cast<double>(value) +
                static_cast<double>(KeyEpsilon));
    };
    std::uint32_t keyIndex = 0u;
    std::uint32_t nextKeyIndex = 0u;
    if (input < lowerBound(keys[0].position)) {
        keyIndex = 0u;
        nextKeyIndex = 0u;
    } else if (input >
               upperBound(keys[curve.keyCount - 1u].position)) {
        keyIndex = curve.keyCount - 1u;
        nextKeyIndex = keyIndex;
    } else {
        std::uint32_t current = 0u;
        for (std::uint32_t scanned = 0u;
             scanned <= curve.keyCount; ++scanned) {
            const std::uint32_t next =
                    current + 1u < curve.keyCount
                    ? current + 1u
                    : 0u;
            const float lower =
                    lowerBound(keys[current].position);
            const float upper =
                    upperBound(keys[next].position);
            if (!isnan(input) && !isnan(lower) &&
                !isnan(upper) && input >= lower &&
                input <= upper) {
                keyIndex = current;
                nextKeyIndex = next;
                break;
            }
            current = next;
            if (scanned == curve.keyCount) {
                keyIndex = current;
                nextKeyIndex =
                        current + 1u < curve.keyCount
                        ? current + 1u
                        : 0u;
            }
        }
    }
    float blend = 0.0f;
    if (keyIndex != nextKeyIndex) {
        const float firstPosition = keys[keyIndex].position;
        const float span =
                keys[nextKeyIndex].position - firstPosition;
        blend = fabsf(span) >= KeyEpsilon
                ? (input - firstPosition) / span
                : 0.0f;
    }
    if (keyIndex == nextKeyIndex || forceConstant ||
        curve.interpolation ==
                static_cast<std::uint32_t>(
                        ReplayTuningCurveInterpolation::Constant)) {
        return keys[keyIndex].value;
    }
    const float firstValue = keys[keyIndex].value;
    const float nextValue = keys[nextKeyIndex].value;
    return (1.0f - blend) * firstValue +
           blend * nextValue;
}

__device__ inline float SpeedInput(float speed) {
    return exact::FromDouble(
            static_cast<double>(speed) *
            static_cast<double>(3.6f));
}

__device__ inline float EvaluateSpeed(
        const CudaPackedStaticConfigurationHeader *configuration,
        CudaTuningCurveId id,
        float speed,
        bool forceConstant = false) {
    return Evaluate(
            configuration, id, SpeedInput(speed),
            forceConstant);
}

__device__ inline const float *TransmissionValues(
        const CudaPackedStaticConfigurationHeader *configuration,
        CudaTransmissionArrayId id,
        std::uint32_t *count = nullptr) {
    const auto *arrays =
            reinterpret_cast<const CudaTransmissionArray *>(
                    &configuration->tuning.gearedDrive.
                            transmissionArrays);
    const CudaTransmissionArray &array =
            arrays[static_cast<std::uint32_t>(id)];
    if (count != nullptr) {
        *count = array.valueCount;
    }
    return Section<float>(
                   configuration,
                   configuration->transmissionValues) +
           array.firstValue;
}

}  // namespace forevervalidator::simulation::cuda::tuning

#endif
