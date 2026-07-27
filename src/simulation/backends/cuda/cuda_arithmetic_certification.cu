#include "simulation/backends/cuda/cuda_backend.h"

#include "engine/core/binary32_math.h"
#include "simulation/backends/cuda/cuda_exact_math.cuh"
#include "simulation/backends/cuda/cuda_rng.cuh"

#include <cuda_runtime_api.h>

#include <array>
#include <cstring>
#include <limits>
#include <vector>

namespace forevervalidator::simulation {
namespace {

enum ArithmeticOperation : std::uint32_t {
    Sqrt = 0u,
    Sin,
    Cos,
    Asin,
    Acos,
    Atan2,
    Exp,
    Fmod,
    FromUnsigned,
    TruncateUnsigned,
    RandomState,
    RandomNatural,
    OperationCount,
};

struct ArithmeticOutputs {
    std::uint32_t bits[OperationCount]{};
};

struct SquareRootMismatch {
    std::uint32_t input = UINT32_MAX;
    std::uint32_t expected = 0u;
    std::uint32_t actual = 0u;
};

__device__ std::uint32_t FloatBits(float value) {
    return __float_as_uint(value);
}

__global__ void ArithmeticKernel(
        const std::uint32_t *inputs,
        ArithmeticOutputs *outputs,
        std::uint32_t count) {
    const std::uint32_t index =
            blockIdx.x * blockDim.x + threadIdx.x;
    if (index >= count) {
        return;
    }
    const std::uint32_t bits = inputs[index];
    const float unit =
            __uint2float_rn(bits & 0x00ffffffu) /
            16777215.0f;
    const float signedUnit =
            (bits & 0x80000000u) != 0u ? -unit : unit;
    const float angle = signedUnit * 120.0f;
    const float positive = unit * 100000.0f;
    const float divisor = 0.125f + unit * 31.875f;
    ArithmeticOutputs result;
    result.bits[Sqrt] =
            FloatBits(cuda::exact::Sqrt(positive));
    result.bits[Sin] = FloatBits(cuda::exact::Sin(angle));
    result.bits[Cos] = FloatBits(cuda::exact::Cos(angle));
    result.bits[Asin] =
            FloatBits(cuda::exact::Asin(signedUnit));
    result.bits[Acos] =
            FloatBits(cuda::exact::Acos(signedUnit));
    result.bits[Atan2] = FloatBits(cuda::exact::Atan2(
            signedUnit, 1.0f - 2.0f * unit));
    result.bits[Exp] =
            FloatBits(cuda::exact::Exp(signedUnit * 100.0f));
    result.bits[Fmod] =
            FloatBits(cuda::exact::Fmod(angle, divisor));
    result.bits[FromUnsigned] = FloatBits(
            cuda::exact::FromUnsignedInteger(bits));
    result.bits[TruncateUnsigned] =
            cuda::exact::TruncateToUint32Modulo(
                    signedUnit * 8589934592.0f);
    std::uint32_t randomState = bits;
    result.bits[RandomNatural] = cuda::rng::Natural(
            randomState, bits & 0xffu,
            (bits & 0xffu) + ((bits >> 8u) & 0x3ffu));
    result.bits[RandomState] = randomState;
    outputs[index] = result;
}

__global__ void ExhaustiveSquareRootKernel(
        SquareRootMismatch *mismatch) {
    const std::uint64_t first =
            static_cast<std::uint64_t>(blockIdx.x) * blockDim.x +
            threadIdx.x;
    const std::uint64_t stride =
            static_cast<std::uint64_t>(gridDim.x) * blockDim.x;
    constexpr std::uint64_t LastNonnegative = 0x7f800000ull;
    for (std::uint64_t bits = first;
         bits <= LastNonnegative;
         bits += stride) {
        const float value =
                __uint_as_float(static_cast<std::uint32_t>(bits));
        const std::uint32_t expected = __float_as_uint(
                __double2float_rn(
                        sqrt(static_cast<double>(value))));
        const std::uint32_t actual =
                __float_as_uint(sqrtf(value));
        if (expected != actual &&
            atomicCAS(
                    &mismatch->input,
                    UINT32_MAX,
                    static_cast<std::uint32_t>(bits)) ==
                    UINT32_MAX) {
            mismatch->expected = expected;
            mismatch->actual = actual;
        }
    }
}

std::uint32_t Bits(float value) {
    std::uint32_t result = 0u;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

std::uint32_t Mix(std::uint32_t value) {
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

std::array<std::uint32_t, OperationCount> CpuOutputs(
        std::uint32_t bits) {
    const float unit =
            Binary32::FromUnsignedInteger(bits & 0x00ffffffu) /
            16777215.0f;
    const float signedUnit =
            (bits & 0x80000000u) != 0u ? -unit : unit;
    const float angle = signedUnit * 120.0f;
    const float positive = unit * 100000.0f;
    const float divisor = 0.125f + unit * 31.875f;
    std::array<std::uint32_t, OperationCount> result{};
    result[Sqrt] = Bits(CIsqrt(positive));
    result[Sin] = Bits(CIsin(angle));
    result[Cos] = Bits(CIcos(angle));
    result[Asin] = Bits(CIasin(signedUnit));
    result[Acos] = Bits(CIacos(signedUnit));
    result[Atan2] =
            Bits(CIatan2(signedUnit, 1.0f - 2.0f * unit));
    result[Exp] = Bits(CIexp(signedUnit * 100.0f));
    result[Fmod] = Bits(CIfmod(angle, divisor));
    result[FromUnsigned] =
            Bits(Binary32::FromUnsignedInteger(bits));
    result[TruncateUnsigned] =
            Binary32::TruncateToUint32Modulo(
                    signedUnit * 8589934592.0f);
    std::uint32_t randomState = bits;
    randomState = randomState * 214013u + 2531011u;
    const std::uint32_t randomValue =
            (randomState >> 16u) & 0x7fffu;
    const std::uint32_t minimum = bits & 0xffu;
    const std::uint32_t range =
            ((bits >> 8u) & 0x3ffu) + 1u;
    const float unitRandom =
            Binary32::FromUnsignedInteger(randomValue) / 32768.0f;
    const float scaledRange =
            unitRandom * Binary32::FromUnsignedInteger(range);
    result[RandomNatural] = static_cast<std::uint32_t>(
            scaledRange +
            Binary32::FromUnsignedInteger(minimum));
    result[RandomState] = randomState;
    return result;
}

}  // namespace

CudaArithmeticCertification CertifyCudaArithmetic(
        std::uint32_t sampleCount) noexcept {
    CudaArithmeticCertification result;
    if (sampleCount == 0u) {
        result.diagnostic = "CUDA arithmetic sample count is zero";
        return result;
    }
    try {
        std::vector<std::uint32_t> inputs(sampleCount);
        std::vector<ArithmeticOutputs> outputs(sampleCount);
        for (std::uint32_t index = 0u; index < sampleCount; ++index) {
            inputs[index] = Mix(index + 0x9e3779b9u);
        }
        std::uint32_t *deviceInputs = nullptr;
        ArithmeticOutputs *deviceOutputs = nullptr;
        SquareRootMismatch *deviceSquareRootMismatch = nullptr;
        cudaError_t error = cudaMalloc(
                reinterpret_cast<void **>(&deviceInputs),
                inputs.size() * sizeof(inputs[0]));
        if (error == cudaSuccess) {
            error = cudaMalloc(
                    reinterpret_cast<void **>(&deviceOutputs),
                    outputs.size() * sizeof(outputs[0]));
        }
        if (error == cudaSuccess) {
            error = cudaMalloc(
                    reinterpret_cast<void **>(
                            &deviceSquareRootMismatch),
                    sizeof(*deviceSquareRootMismatch));
        }
        if (error == cudaSuccess) {
            error = cudaMemset(
                    deviceSquareRootMismatch,
                    0xff,
                    sizeof(*deviceSquareRootMismatch));
        }
        if (error == cudaSuccess) {
            error = cudaMemcpy(
                    deviceInputs, inputs.data(),
                    inputs.size() * sizeof(inputs[0]),
                    cudaMemcpyHostToDevice);
        }
        if (error == cudaSuccess) {
            constexpr std::uint32_t BlockSize = 256u;
            ArithmeticKernel<<<
                    (sampleCount + BlockSize - 1u) / BlockSize,
                    BlockSize>>>(
                    deviceInputs, deviceOutputs, sampleCount);
            error = cudaGetLastError();
        }
        if (error == cudaSuccess) {
            ExhaustiveSquareRootKernel<<<4096u, 256u>>>(
                    deviceSquareRootMismatch);
            error = cudaGetLastError();
        }
        if (error == cudaSuccess) {
            error = cudaMemcpy(
                    outputs.data(), deviceOutputs,
                    outputs.size() * sizeof(outputs[0]),
                    cudaMemcpyDeviceToHost);
        }
        SquareRootMismatch squareRootMismatch;
        if (error == cudaSuccess) {
            error = cudaMemcpy(
                    &squareRootMismatch,
                    deviceSquareRootMismatch,
                    sizeof(squareRootMismatch),
                    cudaMemcpyDeviceToHost);
        }
        cudaFree(deviceSquareRootMismatch);
        cudaFree(deviceOutputs);
        cudaFree(deviceInputs);
        if (error != cudaSuccess) {
            result.diagnostic = std::string(
                    "CUDA arithmetic certification failed: ") +
                    cudaGetErrorString(error);
            return result;
        }
        constexpr std::uint64_t NonnegativeBinary32Values =
                0x7f800001ull;
        result.checkedValues += NonnegativeBinary32Values;
        if (squareRootMismatch.input != UINT32_MAX) {
            ++result.mismatchedValues;
            result.firstMismatchOperation = Sqrt;
            result.firstMismatchInput =
                    squareRootMismatch.input;
            result.expectedBits =
                    squareRootMismatch.expected;
            result.actualBits =
                    squareRootMismatch.actual;
        }
        for (std::uint32_t index = 0u;
             index < sampleCount;
             ++index) {
            const auto expected = CpuOutputs(inputs[index]);
            for (std::uint32_t operation = 0u;
                 operation < OperationCount;
                 ++operation) {
                ++result.checkedValues;
                if (expected[operation] !=
                    outputs[index].bits[operation]) {
                    if (result.mismatchedValues == 0u) {
                        result.firstMismatchOperation = operation;
                        result.firstMismatchInput = inputs[index];
                        result.expectedBits = expected[operation];
                        result.actualBits =
                                outputs[index].bits[operation];
                    }
                    ++result.mismatchedValues;
                }
            }
        }
        result.passed = result.mismatchedValues == 0u;
        result.diagnostic = result.passed
                ? "CUDA arithmetic certification passed"
                : "CUDA arithmetic certification found bitwise mismatches";
        return result;
    } catch (...) {
        result.diagnostic =
                "CUDA arithmetic certification allocation failed";
        return result;
    }
}

}  // namespace forevervalidator::simulation
