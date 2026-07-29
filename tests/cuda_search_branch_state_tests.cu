#include "simulation/backends/cuda/cuda_search_branch_state.cuh"

#include <cuda_runtime.h>

#include <cstdint>
#include <iostream>
#include <vector>

namespace {

using forevervalidator::simulation::CudaSearchInputEvent;
using forevervalidator::simulation::cuda_search_detail::AbsoluteSuffixEvent;
using forevervalidator::simulation::cuda_search_detail::ApplyControlEvent;
using forevervalidator::simulation::cuda_search_detail::ControlsFromState;
using forevervalidator::simulation::cuda_search_detail::DeviceControlState;
using forevervalidator::simulation::cuda_search_detail::PartitionSearchInputs;
using forevervalidator::simulation::cuda_search_detail::SearchInputPartition;
using forevervalidator::simulation::cuda_search_detail::StuntsFromState;

CudaSearchInputEvent Switch(
        std::int32_t timeMs,
        std::uint32_t action,
        bool active) {
    return {timeMs, action, 1u, active ? 1 : 0};
}

CudaSearchInputEvent Analog(
        std::int32_t timeMs,
        std::uint32_t action,
        std::int32_t value) {
    return {timeMs, action, 2u, value};
}

bool SameState(
        const DeviceControlState &left,
        const DeviceControlState &right) {
    if (left.accelerate != right.accelerate ||
        left.brake != right.brake ||
        left.steerRight != right.steerRight ||
        left.steerLeft != right.steerLeft ||
        left.steerLeftTime != right.steerLeftTime ||
        left.steerRightTime != right.steerRightTime ||
        left.steerTime != right.steerTime ||
        left.steerValue != right.steerValue ||
        left.accelerateTime != right.accelerateTime ||
        left.brakeTime != right.brakeTime ||
        left.gasTime != right.gasTime ||
        left.gasValue != right.gasValue) {
        return false;
    }
    for (std::size_t index = 0u; index < 6u; ++index) {
        if (left.stuntLastChangeTime[index] !=
            right.stuntLastChangeTime[index]) {
            return false;
        }
    }
    return true;
}

DeviceControlState ScanThrough(
        const std::vector<CudaSearchInputEvent> &events,
        std::int64_t inclusiveTimeMs) {
    DeviceControlState result;
    for (const CudaSearchInputEvent &event : events) {
        if (event.timeMs > inclusiveTimeMs) {
            break;
        }
        ApplyControlEvent(result, event);
    }
    return result;
}

bool CheckPartitionAndCachedStates() {
    const std::vector<CudaSearchInputEvent> events{
            Switch(10, 1u, true),
            Analog(20, 4u, 12000),
            Switch(30, 5u, true),
            Analog(50, 2u, -25000),
            Switch(55, 3u, true),
            Switch(60, 1u, false),
            Analog(65, 4u, -32000),
            Switch(80, 3u, false)};
    SearchInputPartition partition;
    if (!PartitionSearchInputs(events, 50, 60, &partition) ||
        partition.immutablePrefix.size() != 5u ||
        partition.mutableSuffix.size() != 3u ||
        partition.mutableSuffix[0].timeMs != 0 ||
        partition.mutableSuffix[1].timeMs != 5 ||
        partition.mutableSuffix[2].timeMs != 20 ||
        !SameState(partition.branchControls, ScanThrough(events, 50)) ||
        !SameState(
                partition.mutableBoundaryControls,
                ScanThrough(events, 59))) {
        std::cerr << "partition or cached control state mismatch\n";
        return false;
    }
    if (partition.branchControls.brake ||
        !partition.mutableBoundaryControls.brake ||
        partition.branchControls.gasTime != 50 ||
        partition.mutableBoundaryControls.brakeTime != 55 ||
        partition.mutableBoundaryControls.stuntLastChangeTime[4] != 55) {
        std::cerr << "held input timestamp mismatch\n";
        return false;
    }

    std::vector<CudaSearchInputEvent> mutated = partition.mutableSuffix;
    mutated.erase(mutated.begin());
    mutated[0].timeMs = 1;
    mutated.push_back(Analog(25, 4u, 777));
    std::vector<CudaSearchInputEvent> materialized =
            partition.immutablePrefix;
    for (CudaSearchInputEvent event : mutated) {
        materialized.push_back(AbsoluteSuffixEvent(event, 60));
    }
    if (materialized.size() !=
                partition.immutablePrefix.size() + mutated.size()) {
        return false;
    }
    for (std::size_t index = 0u;
         index < partition.immutablePrefix.size(); ++index) {
        const CudaSearchInputEvent &left = materialized[index];
        const CudaSearchInputEvent &right =
                partition.immutablePrefix[index];
        if (left.timeMs != right.timeMs ||
            left.action != right.action ||
            left.valueKind != right.valueKind ||
            left.value != right.value) {
            std::cerr << "immutable prefix changed during suffix edits\n";
            return false;
        }
    }
    return true;
}

struct DeviceResult {
    DeviceControlState state{};
    ReplayVehicleControlState controls{};
    ReplayStuntInputState stunts{};
};

__global__ void ApplySuffixKernel(
        DeviceControlState initial,
        const CudaSearchInputEvent *suffix,
        std::uint32_t count,
        std::int64_t originMs,
        std::uint32_t prestartMs,
        DeviceResult *result) {
    if (blockIdx.x != 0u || threadIdx.x != 0u) {
        return;
    }
    for (std::uint32_t index = 0u; index < count; ++index) {
        ApplyControlEvent(initial, suffix[index], originMs);
    }
    result->state = initial;
    result->controls = ControlsFromState(initial);
    result->stunts = StuntsFromState(initial, prestartMs);
}

bool CheckHostDeviceSuffixParity() {
    const std::vector<CudaSearchInputEvent> events{
            Switch(10, 1u, true),
            Analog(20, 4u, 12000),
            Switch(55, 3u, true),
            Switch(60, 1u, false),
            Analog(65, 4u, -32000),
            Switch(80, 3u, false)};
    SearchInputPartition partition;
    if (!PartitionSearchInputs(events, 50, 60, &partition)) {
        return false;
    }

    DeviceResult expected;
    expected.state = partition.mutableBoundaryControls;
    for (const CudaSearchInputEvent &event : partition.mutableSuffix) {
        ApplyControlEvent(expected.state, event, 60);
    }
    expected.controls = ControlsFromState(expected.state);
    expected.stunts = StuntsFromState(expected.state, 100);

    CudaSearchInputEvent *deviceEvents = nullptr;
    DeviceResult *deviceResult = nullptr;
    cudaError_t error = cudaMalloc(
            reinterpret_cast<void **>(&deviceEvents),
            partition.mutableSuffix.size() *
                    sizeof(CudaSearchInputEvent));
    if (error == cudaSuccess) {
        error = cudaMalloc(
                reinterpret_cast<void **>(&deviceResult),
                sizeof(DeviceResult));
    }
    if (error == cudaSuccess) {
        error = cudaMemcpy(
                deviceEvents, partition.mutableSuffix.data(),
                partition.mutableSuffix.size() *
                        sizeof(CudaSearchInputEvent),
                cudaMemcpyHostToDevice);
    }
    if (error == cudaSuccess) {
        ApplySuffixKernel<<<1u, 1u>>>(
                partition.mutableBoundaryControls,
                deviceEvents,
                static_cast<std::uint32_t>(
                        partition.mutableSuffix.size()),
                60,
                100u,
                deviceResult);
        error = cudaGetLastError();
    }
    DeviceResult actual;
    if (error == cudaSuccess) {
        error = cudaMemcpy(
                &actual, deviceResult, sizeof(actual),
                cudaMemcpyDeviceToHost);
    }
    cudaFree(deviceResult);
    cudaFree(deviceEvents);
    if (error != cudaSuccess) {
        std::cerr << "CUDA branch control parity launch failed\n";
        return false;
    }
    if (!SameState(actual.state, expected.state) ||
        actual.controls.lowSpeedGateA != expected.controls.lowSpeedGateA ||
        actual.controls.lowSpeedGateB != expected.controls.lowSpeedGateB ||
        actual.controls.steering != expected.controls.steering ||
        actual.stunts.lastChangeTimeMs !=
                expected.stunts.lastChangeTimeMs ||
        actual.state.accelerateTime != 60 ||
        actual.state.steerTime != 65 ||
        actual.state.brakeTime != 80 ||
        actual.stunts.lastChangeTimeMs[2] != 165u) {
        std::cerr << "host/device suffix control parity mismatch\n";
        return false;
    }
    return true;
}

}  // namespace

int main() {
    return CheckPartitionAndCachedStates() &&
                   CheckHostDeviceSuffixParity()
            ? 0
            : 1;
}
