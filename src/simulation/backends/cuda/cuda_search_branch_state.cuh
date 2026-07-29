#ifndef FOREVERVALIDATOR_CUDA_SEARCH_BRANCH_STATE_CUH
#define FOREVERVALIDATOR_CUDA_SEARCH_BRANCH_STATE_CUH

#include <cuda_runtime.h>

#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "simulation/backends/cuda/cuda_search_executor.h"

namespace forevervalidator::simulation::cuda_search_detail {

struct DeviceControlState {
    bool accelerate = false;
    bool brake = false;
    bool steerRight = false;
    bool steerLeft = false;
    std::int32_t steerLeftTime = 0;
    std::int32_t steerRightTime = 0;
    std::int32_t steerTime = 0;
    std::int32_t steerValue = 0;
    std::int32_t accelerateTime = 0;
    std::int32_t brakeTime = 0;
    std::int32_t gasTime = 0;
    std::int32_t gasValue = 0;
    std::int64_t stuntLastChangeTime[6]{};
};

__host__ __device__ inline int StuntActionIndex(std::uint32_t action) {
    switch (action) {
    case 5u: return 0;
    case 6u: return 1;
    case 4u: return 2;
    case 1u: return 3;
    case 3u: return 4;
    case 2u: return 5;
    default: return -1;
    }
}

__host__ __device__ inline bool IsActiveSwitch(
        const CudaSearchInputEvent &event) {
    return event.valueKind == 1u && event.value != 0;
}

__host__ __device__ inline void ApplyControlEvent(
        DeviceControlState &state,
        const CudaSearchInputEvent &event,
        std::int64_t timeOriginMs = 0) {
    const std::int64_t absoluteTime =
            timeOriginMs + static_cast<std::int64_t>(event.timeMs);
    const std::int32_t time = static_cast<std::int32_t>(absoluteTime);
    const int stuntIndex = StuntActionIndex(event.action);
    if (stuntIndex >= 0) {
        state.stuntLastChangeTime[stuntIndex] = absoluteTime;
    }
    switch (event.action) {
    case 1u:
        state.accelerate = IsActiveSwitch(event);
        state.accelerateTime = time;
        break;
    case 2u:
        state.gasValue = event.value;
        state.gasTime = time;
        break;
    case 3u:
        state.brake = IsActiveSwitch(event);
        state.brakeTime = time;
        break;
    case 4u:
        state.steerValue = event.value;
        state.steerTime = time;
        break;
    case 5u:
        state.steerLeft = IsActiveSwitch(event);
        state.steerLeftTime = time;
        break;
    case 6u:
        state.steerRight = IsActiveSwitch(event);
        state.steerRightTime = time;
        break;
    default:
        break;
    }
}

__host__ __device__ inline ReplayVehicleControlState ControlsFromState(
        const DeviceControlState &state) {
    float steering = 0.0f;
    const std::int32_t digitalSteerTime =
            state.steerLeftTime > state.steerRightTime
            ? state.steerLeftTime : state.steerRightTime;
    const std::int32_t steerMagnitude =
            state.steerValue < 0 ? -state.steerValue : state.steerValue;
    const bool analogWins =
            state.steerTime > digitalSteerTime ||
            (state.steerTime == digitalSteerTime &&
             !state.steerLeft && !state.steerRight &&
             steerMagnitude > 655);
    if (analogWins) {
        steering = static_cast<float>(state.steerValue) / 65536.0f;
    } else if (state.steerLeft) {
        steering = -1.0f;
    } else if (state.steerRight) {
        steering = 1.0f;
    }

    float gateA = 0.0f;
    float gateB = 0.0f;
    const std::int32_t digitalGateTime =
            state.accelerateTime > state.brakeTime
            ? state.accelerateTime : state.brakeTime;
    const bool gasWins =
            state.gasTime > digitalGateTime ||
            (state.gasTime == digitalGateTime &&
             !state.accelerate && !state.brake);
    if (gasWins) {
        if (state.gasValue <= -19661) {
            gateA = 1.0f;
        } else if (state.gasValue >= 19661) {
            gateB = 1.0f;
        }
    } else {
        gateA = state.accelerate ? 1.0f : 0.0f;
        gateB = state.brake ? 1.0f : 0.0f;
    }
    return {gateA, gateB, steering};
}

__host__ __device__ inline ReplayStuntInputState StuntsFromState(
        const DeviceControlState &state,
        std::uint32_t prestartDurationMs) {
    ReplayStuntInputState result;
    auto *lastChangeTimeMs =
            reinterpret_cast<std::uint32_t *>(&result.lastChangeTimeMs);
    for (std::uint32_t index = 0u; index < 6u; ++index) {
        const std::int64_t translated =
                static_cast<std::int64_t>(prestartDurationMs) +
                state.stuntLastChangeTime[index];
        lastChangeTimeMs[index] = translated <= 0
                ? 0u
                : translated >= UINT32_MAX
                ? UINT32_MAX
                : static_cast<std::uint32_t>(translated);
    }
    return result;
}

struct SearchInputPartition {
    std::vector<CudaSearchInputEvent> immutablePrefix;
    std::vector<CudaSearchInputEvent> mutableSuffix;
    DeviceControlState branchControls{};
    DeviceControlState mutableBoundaryControls{};
    std::int64_t mutableFromTimeMs = 0;
};

inline bool PartitionSearchInputs(
        const std::vector<CudaSearchInputEvent> &inputs,
        std::int64_t branchTimeMs,
        std::int64_t mutableFromTimeMs,
        SearchInputPartition *partition) {
    if (partition == nullptr ||
        branchTimeMs < 0 ||
        mutableFromTimeMs <= branchTimeMs ||
        mutableFromTimeMs > INT32_MAX) {
        return false;
    }

    SearchInputPartition result;
    result.mutableFromTimeMs = mutableFromTimeMs;
    result.immutablePrefix.reserve(inputs.size());
    result.mutableSuffix.reserve(inputs.size());
    std::int32_t previousTime = INT32_MIN;
    for (const CudaSearchInputEvent &input : inputs) {
        if (input.timeMs < previousTime ||
            input.timeMs < 0 ||
            (input.valueKind == 2u &&
             (input.value < -65536 || input.value > 65536)) ||
            (input.valueKind == 1u &&
             input.value != 0 && input.value != 1)) {
            return false;
        }
        previousTime = input.timeMs;
        if (input.timeMs <= branchTimeMs) {
            ApplyControlEvent(result.branchControls, input);
        }
        if (input.timeMs < mutableFromTimeMs) {
            ApplyControlEvent(result.mutableBoundaryControls, input);
            result.immutablePrefix.push_back(input);
            continue;
        }
        CudaSearchInputEvent relative = input;
        relative.timeMs = static_cast<std::int32_t>(
                static_cast<std::int64_t>(input.timeMs) -
                mutableFromTimeMs);
        result.mutableSuffix.push_back(relative);
    }
    *partition = std::move(result);
    return true;
}

inline CudaSearchInputEvent AbsoluteSuffixEvent(
        CudaSearchInputEvent event,
        std::int64_t mutableFromTimeMs) {
    event.timeMs = static_cast<std::int32_t>(
            mutableFromTimeMs + event.timeMs);
    return event;
}

}  // namespace forevervalidator::simulation::cuda_search_detail

#endif
