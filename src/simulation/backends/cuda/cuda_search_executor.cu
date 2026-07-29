#include "simulation/backends/cuda/cuda_search_executor.h"

#include <cuda_runtime.h>
#include <cub/device/device_reduce.cuh>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <new>
#include <thread>
#include <type_traits>
#include <utility>

#include "simulation/backends/cuda/cuda_candidate_events.cuh"
#include "simulation/backends/cuda/cuda_exact_math.cuh"
#include "simulation/backends/cuda/cuda_finish_time_refinement.cuh"
#include "simulation/backends/cuda/cuda_modifier_event_ops.cuh"
#include "simulation/backends/cuda/cuda_physics_step.cuh"
#include "simulation/backends/cuda/cuda_static_configuration.h"
#include "simulation/backends/cuda/cuda_scene_layout.h"
#include "simulation/backends/cuda/cuda_search_branch_state.cuh"
#include "simulation/backends/cuda/cuda_search_winner_selection.cuh"
#include "simulation/backends/cuda/cuda_stunts.cuh"
#include "simulation/backends/cuda/cuda_vehicle_transitions.cuh"

namespace forevervalidator::simulation {
namespace {

constexpr std::uint32_t SimulationBlockSize = 32u;
constexpr std::uint32_t LatencyKernelMinimumBlocksPerSm = 1u;
constexpr std::uint32_t ThroughputKernelMinimumBlocksPerSm = 16u;

enum class DeviceCandidateStatus : std::uint32_t {
    Success,
    Cancelled,
    CapacityExceeded,
    UnsupportedPhysicsTransition,
};

using cuda_search_detail::BetterSample;
using cuda_search_detail::ControlsFromState;
using cuda_search_detail::DeviceControlState;
using cuda_search_detail::DeviceSample;
using cuda_search_detail::InvalidCandidateSlot;
using cuda_search_detail::ApplyControlEvent;
using cuda_search_detail::StuntsFromState;
using cuda_search_detail::StrictlyBetter;
namespace modifier_ops = cuda_search_modifier_detail;

struct DeviceBatchSummary {
    CudaSearchStatus status = CudaSearchStatus::Success;
    std::uint32_t evaluatedCandidateCount = 0u;
    std::uint64_t evaluatorCalls = 0u;
    std::uint64_t totalMutationCount = 0u;
    std::uint64_t mutationImprovementCount = 0u;
    std::uint32_t globalEventCount = 0u;
    bool bestChanged = false;
    bool bestValid = false;
    bool bestMutation = false;
    std::uint64_t bestCandidateId = 0u;
    std::uint32_t bestMutationCount = 0u;
};

template<typename T>
class DeviceAllocation {
public:
    DeviceAllocation() = default;
    ~DeviceAllocation() { Reset(); }
    DeviceAllocation(const DeviceAllocation &) = delete;
    DeviceAllocation &operator=(const DeviceAllocation &) = delete;
    DeviceAllocation(DeviceAllocation &&other) noexcept
        : data_(std::exchange(other.data_, nullptr)),
          count_(std::exchange(other.count_, 0u)) {}
    DeviceAllocation &operator=(DeviceAllocation &&other) noexcept {
        if (this != &other) {
            Reset();
            data_ = std::exchange(other.data_, nullptr);
            count_ = std::exchange(other.count_, 0u);
        }
        return *this;
    }

    bool Allocate(std::size_t count) {
        Reset();
        if (count == 0u) {
            return true;
        }
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            return false;
        }
        if (cudaMalloc(
                    reinterpret_cast<void **>(&data_),
                    count * sizeof(T)) != cudaSuccess) {
            data_ = nullptr;
            return false;
        }
        count_ = count;
        return true;
    }

    void Reset() {
        if (data_ != nullptr) {
            cudaFree(data_);
        }
        data_ = nullptr;
        count_ = 0u;
    }

    T *Get() const { return data_; }
    std::size_t Count() const { return count_; }
    std::size_t Bytes() const { return count_ * sizeof(T); }

private:
    T *data_ = nullptr;
    std::size_t count_ = 0u;
};

class MappedCancellation {
public:
    MappedCancellation() = default;
    ~MappedCancellation() { Reset(); }
    MappedCancellation(const MappedCancellation &) = delete;
    MappedCancellation &operator=(const MappedCancellation &) = delete;

    bool Allocate() {
        Reset();
        if (cudaHostAlloc(
                    reinterpret_cast<void **>(&host_),
                    sizeof(*host_),
                    cudaHostAllocMapped | cudaHostAllocPortable) !=
            cudaSuccess) {
            return false;
        }
        if (cudaHostGetDevicePointer(
                    reinterpret_cast<void **>(&device_),
                    host_, 0u) != cudaSuccess) {
            Reset();
            return false;
        }
        *host_ = 0u;
        return true;
    }

    void Reset() {
        if (host_ != nullptr) {
            cudaFreeHost(host_);
        }
        host_ = nullptr;
        device_ = nullptr;
    }

    std::uint32_t *Host() const { return host_; }
    std::uint32_t *Get() const { return device_; }
    std::size_t Bytes() const { return sizeof(std::uint32_t); }

private:
    std::uint32_t *host_ = nullptr;
    std::uint32_t *device_ = nullptr;
};

class Event {
public:
    Event() {
        valid_ = cudaEventCreate(&event_) == cudaSuccess;
    }
    ~Event() {
        if (valid_) {
            cudaEventDestroy(event_);
        }
    }
    bool Valid() const { return valid_; }
    cudaEvent_t Get() const { return event_; }

private:
    cudaEvent_t event_{};
    bool valid_ = false;
};

std::string CudaFailure(const char *operation, cudaError_t error) {
    return std::string(operation) + " failed: " +
            cudaGetErrorName(error) + " (" +
            cudaGetErrorString(error) + ")";
}

bool CanonicalBaselineInputs(
        const std::vector<CudaSearchInputEvent> &inputs,
        std::int64_t mutableFromTimeMs) {
    bool mutableInputSeen = false;
    std::int32_t previousMutableTime = INT32_MIN;
    for (std::size_t index = 0u; index < inputs.size(); ++index) {
        const CudaSearchInputEvent &input = inputs[index];
        if (input.timeMs < mutableFromTimeMs) {
            if (mutableInputSeen) {
                return false;
            }
            continue;
        }
        mutableInputSeen = true;
        if (input.timeMs < 0 ||
            input.timeMs < previousMutableTime ||
            (input.valueKind == 2u &&
             (input.value < -65536 || input.value > 65536)) ||
            (input.valueKind == 1u &&
             input.value != 0 && input.value != 1)) {
            return false;
        }
        for (std::size_t duplicate = index;
             duplicate != 0u &&
             inputs[duplicate - 1u].timeMs == input.timeMs;
             --duplicate) {
            if (inputs[duplicate - 1u].action == input.action) {
                return false;
            }
        }
        previousMutableTime = input.timeMs;
    }
    return true;
}

__device__ bool IsAnalog(const CudaSearchInputEvent &event) {
    return event.valueKind == 2u;
}

__device__ bool IsSwitch(const CudaSearchInputEvent &event) {
    return event.valueKind == 1u;
}

__device__ bool IsSteerAction(std::uint32_t action) {
    return action == 4u;
}

__device__ bool IsAccelerateAction(std::uint32_t action) {
    return action == 1u || action == 2u;
}

__device__ bool IsBrakeAction(std::uint32_t action) {
    return action == 3u;
}

__device__ std::int32_t SaturateAnalog(std::int64_t value) {
    if (value < -65536) {
        return -65536;
    }
    if (value > 65536) {
        return 65536;
    }
    return static_cast<std::int32_t>(value);
}

__device__ bool SameEvent(const CudaSearchInputEvent &left,
                          const CudaSearchInputEvent &right) {
    return left.timeMs == right.timeMs &&
            left.action == right.action &&
            left.valueKind == right.valueKind &&
            left.value == right.value;
}

__device__ CudaSearchInputEvent CandidateInputAt(
        const CudaSearchInputEvent *baselineInputs,
        const CudaSearchInputEvent *materializedInputs,
        const std::int32_t *compactValues,
        const std::uint32_t *compactOffsets,
        bool compact,
        std::uint32_t index,
        std::uint32_t candidateSlot,
        std::uint32_t candidateStride) {
    if (!compact) {
        return materializedInputs[index];
    }
    CudaSearchInputEvent result = baselineInputs[index];
    const std::uint32_t offset = compactOffsets[index];
    if (offset != UINT32_MAX) {
        result.value = compactValues[
                static_cast<std::uint64_t>(offset) *
                        candidateStride +
                candidateSlot];
    }
    return result;
}

class DeviceMt19937 {
public:
    __device__ DeviceMt19937(std::uint32_t *stateWords,
                             std::uint32_t slot,
                             std::uint32_t stride)
        : stateWords_(stateWords), slot_(slot), stride_(stride) {}

    __device__ void Seed(std::uint32_t seed,
                         std::uint64_t candidateId,
                         std::uint32_t passIndex) {
        const std::uint32_t seeds[4]{
                seed,
                static_cast<std::uint32_t>(candidateId),
                static_cast<std::uint32_t>(candidateId >> 32u),
                passIndex};
        for (std::uint32_t index = 0u; index < 624u; ++index) {
            State(index) = 0x8b8b8b8bu;
        }
        constexpr std::uint32_t n = 624u;
        constexpr std::uint32_t s = 4u;
        constexpr std::uint32_t p = 306u;
        constexpr std::uint32_t q = 317u;
        {
            const std::uint32_t r1 = 1371501266u;
            const std::uint32_t r2 = r1 + s;
            State(p) += r1;
            State(q) += r2;
            State(0u) = r2;
        }
        for (std::uint32_t k = 1u; k <= s; ++k) {
            const std::uint32_t kn = k % n;
            const std::uint32_t kpn = (k + p) % n;
            const std::uint32_t kqn = (k + q) % n;
            const std::uint32_t argument =
                    State(kn) ^ State(kpn) ^ State((k - 1u) % n);
            const std::uint32_t r1 =
                    1664525u * (argument ^ (argument >> 27u));
            const std::uint32_t r2 = r1 + kn + seeds[k - 1u];
            State(kpn) += r1;
            State(kqn) += r2;
            State(kn) = r2;
        }
        for (std::uint32_t k = s + 1u; k < n; ++k) {
            const std::uint32_t kn = k % n;
            const std::uint32_t kpn = (k + p) % n;
            const std::uint32_t kqn = (k + q) % n;
            const std::uint32_t argument =
                    State(kn) ^ State(kpn) ^ State((k - 1u) % n);
            const std::uint32_t r1 =
                    1664525u * (argument ^ (argument >> 27u));
            const std::uint32_t r2 = r1 + kn;
            State(kpn) += r1;
            State(kqn) += r2;
            State(kn) = r2;
        }
        for (std::uint32_t k = n; k < 2u * n; ++k) {
            const std::uint32_t kn = k % n;
            const std::uint32_t kpn = (k + p) % n;
            const std::uint32_t kqn = (k + q) % n;
            const std::uint32_t argument =
                    State(kn) + State(kpn) + State((k - 1u) % n);
            const std::uint32_t r3 =
                    1566083941u * (argument ^ (argument >> 27u));
            const std::uint32_t r4 = r3 - kn;
            State(kpn) ^= r3;
            State(kqn) ^= r4;
            State(kn) = r4;
        }
        cursor_ = n;
    }

    __device__ std::uint32_t Next() {
        if (cursor_ >= 624u) {
            Twist();
        }
        std::uint32_t value = State(cursor_++);
        value ^= value >> 11u;
        value ^= (value << 7u) & 0x9d2c5680u;
        value ^= (value << 15u) & 0xefc60000u;
        value ^= value >> 18u;
        return value;
    }

    __device__ std::uint64_t UniformUnsigned(std::uint64_t minimum,
                                             std::uint64_t maximum) {
        if (minimum > maximum) {
            const std::uint64_t swap = minimum;
            minimum = maximum;
            maximum = swap;
        }
        const std::uint64_t range = maximum - minimum;
        std::uint64_t result = 0u;
        if (range < UINT32_MAX) {
            const std::uint32_t extendedRange =
                    static_cast<std::uint32_t>(range + 1u);
            std::uint64_t product =
                    static_cast<std::uint64_t>(Next()) * extendedRange;
            std::uint32_t low = static_cast<std::uint32_t>(product);
            if (low < extendedRange) {
                const std::uint32_t threshold =
                        static_cast<std::uint32_t>(-extendedRange) %
                        extendedRange;
                while (low < threshold) {
                    product =
                            static_cast<std::uint64_t>(Next()) *
                            extendedRange;
                    low = static_cast<std::uint32_t>(product);
                }
            }
            result = product >> 32u;
        } else if (range == UINT32_MAX) {
            result = Next();
        } else {
            do {
                constexpr std::uint64_t generatorRange =
                        UINT64_C(1) << 32u;
                const std::uint64_t high = UniformUnsigned(
                        0u, range / generatorRange);
                const std::uint64_t temporary =
                        generatorRange * high;
                result = temporary + Next();
                if (result <= range && result >= temporary) {
                    break;
                }
            } while (true);
        }
        return result + minimum;
    }

    __device__ std::uint32_t UniformU32(std::uint32_t minimum,
                                       std::uint32_t maximum) {
        return static_cast<std::uint32_t>(
                UniformUnsigned(minimum, maximum));
    }

    __device__ std::int32_t UniformS32(std::int32_t minimum,
                                      std::int32_t maximum) {
        if (minimum > maximum) {
            const std::int32_t swap = minimum;
            minimum = maximum;
            maximum = swap;
        }
        const std::uint32_t unsignedMinimum =
                static_cast<std::uint32_t>(minimum);
        const std::uint32_t range =
                static_cast<std::uint32_t>(maximum) - unsignedMinimum;
        return static_cast<std::int32_t>(
                static_cast<std::uint32_t>(
                        UniformUnsigned(0u, range)) +
                unsignedMinimum);
    }

    __device__ std::int64_t UniformS64(std::int64_t minimum,
                                      std::int64_t maximum) {
        if (minimum > maximum) {
            const std::int64_t swap = minimum;
            minimum = maximum;
            maximum = swap;
        }
        const std::uint64_t unsignedMinimum =
                static_cast<std::uint64_t>(minimum);
        const std::uint64_t range =
                static_cast<std::uint64_t>(maximum) - unsignedMinimum;
        return static_cast<std::int64_t>(
                UniformUnsigned(0u, range) + unsignedMinimum);
    }

private:
    __device__ std::uint32_t &State(std::uint32_t index) {
        return stateWords_[
                static_cast<std::uint64_t>(index) * stride_ + slot_];
    }

    __device__ void Twist() {
        constexpr std::uint32_t upperMask = 0x80000000u;
        constexpr std::uint32_t lowerMask = 0x7fffffffu;
        constexpr std::uint32_t coefficient = 0x9908b0dfu;
        for (std::uint32_t index = 0u; index < 227u; ++index) {
            const std::uint32_t value =
                    (State(index) & upperMask) |
                    (State(index + 1u) & lowerMask);
            State(index) = State(index + 397u) ^
                    (value >> 1u) ^
                    ((value & 1u) ? coefficient : 0u);
        }
        for (std::uint32_t index = 227u; index < 623u; ++index) {
            const std::uint32_t value =
                    (State(index) & upperMask) |
                    (State(index + 1u) & lowerMask);
            State(index) = State(index - 227u) ^
                    (value >> 1u) ^
                    ((value & 1u) ? coefficient : 0u);
        }
        const std::uint32_t value =
                (State(623u) & upperMask) |
                (State(0u) & lowerMask);
        State(623u) = State(396u) ^ (value >> 1u) ^
                ((value & 1u) ? coefficient : 0u);
        cursor_ = 0u;
    }

    std::uint32_t *stateWords_ = nullptr;
    std::uint32_t slot_ = 0u;
    std::uint32_t stride_ = 0u;
    std::uint32_t cursor_ = 624u;
};

__device__ void ShuffleIndices(std::uint32_t *indices,
                               std::uint32_t count,
                               DeviceMt19937 &random) {
    if (count <= 1u) {
        return;
    }
    const std::uint64_t generatorRange = UINT32_MAX;
    const std::uint64_t range = count;
    if (generatorRange / range >= range) {
        std::uint32_t index = 1u;
        if ((range % 2u) == 0u) {
            const std::uint32_t selected =
                    random.UniformU32(0u, 1u);
            const std::uint32_t swap = indices[index];
            indices[index] = indices[selected];
            indices[selected] = swap;
            ++index;
        }
        while (index != count) {
            const std::uint64_t swapRange =
                    static_cast<std::uint64_t>(index) + 1u;
            const std::uint64_t position = random.UniformUnsigned(
                    0u, swapRange * (swapRange + 1u) - 1u);
            const std::uint32_t first =
                    static_cast<std::uint32_t>(
                            position / (swapRange + 1u));
            const std::uint32_t second =
                    static_cast<std::uint32_t>(
                            position % (swapRange + 1u));
            std::uint32_t swap = indices[index];
            indices[index] = indices[first];
            indices[first] = swap;
            ++index;
            swap = indices[index];
            indices[index] = indices[second];
            indices[second] = swap;
            ++index;
        }
        return;
    }
    for (std::uint32_t index = 1u; index < count; ++index) {
        const std::uint32_t selected =
                random.UniformU32(0u, index);
        const std::uint32_t swap = indices[index];
        indices[index] = indices[selected];
        indices[selected] = swap;
    }
}

__device__ std::uint32_t NormalizeEvents(
        CudaSearchInputEvent *events,
        std::uint32_t count,
        CudaSearchInputEvent *temporary,
        const CudaSearchInputEvent *passBaseline,
        std::uint32_t passBaselineCount,
        std::int64_t mutableFromTimeMs,
        bool legacyMutationPipeline,
        std::uint32_t capacity) {
    (void)legacyMutationPipeline;
    return cuda::candidate_events::NormalizeWithPrefix(
            events, count, temporary,
            passBaseline, passBaselineCount,
            mutableFromTimeMs, capacity);
}

__device__ std::uint32_t EffectiveChangeCount(
        const CudaSearchInputEvent *baseline,
        std::uint32_t baselineCount,
        const CudaSearchInputEvent *events,
        std::uint32_t eventCount) {
    const std::uint32_t common =
            baselineCount < eventCount ? baselineCount : eventCount;
    std::uint32_t result = baselineCount > eventCount
            ? baselineCount - eventCount
            : eventCount - baselineCount;
    for (std::uint32_t index = 0u; index < common; ++index) {
        if (!SameEvent(baseline[index], events[index])) {
            ++result;
        }
    }
    return result;
}

__device__ std::int32_t SteeringStateAt(
        const CudaSearchInputEvent *events,
        std::uint32_t count,
        std::int64_t timeMs,
        std::int32_t initialState,
        bool sortedByTime = false) {
    return modifier_ops::ChannelStateAt(
            events, count, 4u, 2u, timeMs,
            sortedByTime, initialState);
}

__device__ bool SwitchStateAt(
        const CudaSearchInputEvent *events,
        std::uint32_t count,
        std::uint32_t action,
        std::int64_t timeMs,
        bool initialState,
        bool sortedByTime = false) {
    return modifier_ops::ChannelStateAt(
                   events, count, action, 1u,
                   timeMs, sortedByTime,
                   initialState ? 1 : 0) != 0;
}

__device__ bool PushEvent(CudaSearchInputEvent *events,
                          std::uint32_t *count,
                          std::uint32_t capacity,
                          CudaSearchInputEvent event) {
    if (*count >= capacity) {
        return false;
    }
    events[(*count)++] = event;
    return true;
}

__device__ CudaSearchInputEvent AnalogEvent(
        std::int64_t timeMs,
        std::uint32_t action,
        std::int32_t value) {
    return {static_cast<std::int32_t>(timeMs), action, 2u, value};
}

__device__ CudaSearchInputEvent SwitchEvent(
        std::int64_t timeMs,
        std::uint32_t action,
        bool value) {
    return {static_cast<std::int32_t>(timeMs),
            action,
            1u,
            value ? 1 : 0};
}

__device__ std::uint32_t CollectEligible(
        const CudaSearchInputEvent *events,
        std::uint32_t count,
        std::uint32_t *eligible,
        const CudaSearchModifierConfiguration &modifier,
        bool sortedByTime) {
    const std::uint32_t begin = sortedByTime
            ? modifier_ops::LowerBoundTime(
                      events, count, modifier.window.minimumTimeMs)
            : 0u;
    const std::uint32_t end = sortedByTime
            ? modifier_ops::UpperBoundTime(
                      events, count, modifier.window.maximumTimeMs)
            : count;
    std::uint32_t result = 0u;
    for (std::uint32_t index = begin; index < end; ++index) {
        const CudaSearchInputEvent &event = events[index];
        if (!sortedByTime &&
            (event.timeMs < modifier.window.minimumTimeMs ||
             event.timeMs > modifier.window.maximumTimeMs)) {
            continue;
        }
        if ((IsSteerAction(event.action) && IsAnalog(event)) ||
            ((modifier.optionFlags & 2u) != 0u &&
             IsAccelerateAction(event.action)) ||
            ((modifier.optionFlags & 4u) != 0u &&
             IsBrakeAction(event.action))) {
            eligible[result++] = index;
        }
    }
    return result;
}

__device__ bool ApplyModifier(
        const CudaSearchModifierConfiguration &modifier,
        std::uint32_t passIndex,
        std::uint64_t candidateId,
        DeviceMt19937 &random,
        std::uint32_t tickDurationMs,
        std::int64_t mutableFromTimeMs,
        const DeviceControlState &initialControls,
        const CudaSearchInputEvent *globalBaseline,
        std::uint32_t globalBaselineCount,
        CudaSearchInputEvent *events,
        std::uint32_t *eventCount,
        std::uint32_t eventCapacity,
        CudaSearchInputEvent *temporary,
        CudaSearchInputEvent *passBaseline,
        std::uint32_t *eligible,
        const double *smoothWeights,
        bool legacyMutationPipeline,
        bool *normalized) {
    const std::uint32_t passBaselineCount = *eventCount;
    const bool passBaselineCanonical = *normalized;
    const bool insertionRestoresHeldState =
            modifier.kind == CudaSearchModifierKind::InputInsertion &&
            ((modifier.steering.enabled != 0u &&
              modifier.steering.maximumHoldMs > 0) ||
             (modifier.accelerate.enabled != 0u &&
              modifier.accelerate.maximumHoldMs > 0) ||
             (modifier.brake.enabled != 0u &&
              modifier.brake.maximumHoldMs > 0));
    const bool needsPassSnapshot =
            legacyMutationPipeline ||
            insertionRestoresHeldState ||
            modifier.window.minimumTimeMs < mutableFromTimeMs;
    if (needsPassSnapshot) {
        for (std::uint32_t index = 0u;
             index < passBaselineCount; ++index) {
            passBaseline[index] = events[index];
        }
    }
    const CudaSearchInputEvent *normalizationBaseline =
            needsPassSnapshot ? passBaseline : globalBaseline;
    const std::uint32_t normalizationBaselineCount =
            needsPassSnapshot ? passBaselineCount : globalBaselineCount;
    random.Seed(modifier.window.seed, candidateId, passIndex);

    switch (modifier.kind) {
    case CudaSearchModifierKind::RandomSteering: {
        const std::uint32_t begin = *normalized
                ? modifier_ops::LowerBoundTime(
                          events, *eventCount,
                          modifier.window.minimumTimeMs)
                : 0u;
        const std::uint32_t end = *normalized
                ? modifier_ops::UpperBoundTime(
                          events, *eventCount,
                          modifier.window.maximumTimeMs)
                : *eventCount;
        for (std::uint32_t index = begin; index < end; ++index) {
            CudaSearchInputEvent &event = events[index];
            if ((!*normalized &&
                 (event.timeMs < modifier.window.minimumTimeMs ||
                  event.timeMs > modifier.window.maximumTimeMs)) ||
                event.action != 4u || !IsAnalog(event)) {
                continue;
            }
            std::int32_t value =
                    random.UniformS32(-65536, 65536);
            if (value == event.value) {
                value = value == 65536 ? -65536 : 65536;
            }
            event.value = value;
        }
        if (!legacyMutationPipeline &&
            modifier.window.minimumTimeMs >= mutableFromTimeMs &&
            *normalized) {
            break;
        }
        *eventCount = NormalizeEvents(
                events, *eventCount, temporary,
                normalizationBaseline, normalizationBaselineCount,
                mutableFromTimeMs, legacyMutationPipeline,
                eventCapacity);
        if (*eventCount == UINT32_MAX) {
            return false;
        }
        *normalized = true;
        break;
    }
    case CudaSearchModifierKind::ExistingEvent: {
        const std::uint32_t eligibleCount = CollectEligible(
                events, *eventCount, eligible, modifier, *normalized);
        if (eligibleCount == 0u) {
            break;
        }
        ShuffleIndices(eligible, eligibleCount, random);
        const std::uint32_t requested = random.UniformU32(
                modifier.minimumCount, modifier.maximumCount);
        const std::uint32_t count =
                requested < eligibleCount ? requested : eligibleCount;
        const std::int64_t maximumShiftTicks =
                modifier.timeParameterMs /
                static_cast<std::int64_t>(tickDurationMs);
        for (std::uint32_t index = 0u; index < count; ++index) {
            CudaSearchInputEvent &event = events[eligible[index]];
            const std::int64_t shiftTicks = random.UniformS64(
                    -maximumShiftTicks, maximumShiftTicks);
            std::int64_t time =
                    static_cast<std::int64_t>(event.timeMs) +
                    shiftTicks * tickDurationMs;
            if (time < modifier.window.minimumTimeMs) {
                time = modifier.window.minimumTimeMs;
            }
            if (time > modifier.window.maximumTimeMs) {
                time = modifier.window.maximumTimeMs;
            }
            event.timeMs = static_cast<std::int32_t>(time);
            if (IsSteerAction(event.action)) {
                if ((modifier.optionFlags & 1u) != 0u) {
                    event.value = random.UniformS32(
                            modifier.secondaryAnalogMinimum,
                            modifier.secondaryAnalogMaximum);
                } else {
                    const std::int32_t delta = random.UniformS32(
                            modifier.analogMinimum,
                            modifier.analogMaximum);
                    event.value = SaturateAnalog(
                            static_cast<std::int64_t>(event.value) +
                            delta);
                }
            } else if (IsSwitch(event)) {
                event.value = event.value != 0 ? 0 : 1;
            }
        }
        if (!legacyMutationPipeline &&
            modifier.timeParameterMs == 0 &&
            modifier.window.minimumTimeMs >= mutableFromTimeMs &&
            *normalized) {
            break;
        }
        *eventCount = NormalizeEvents(
                events, *eventCount, temporary,
                normalizationBaseline, normalizationBaselineCount,
                mutableFromTimeMs, legacyMutationPipeline,
                eventCapacity);
        if (*eventCount == UINT32_MAX) {
            return false;
        }
        *normalized = true;
        break;
    }
    case CudaSearchModifierKind::SmoothSteering:
        for (std::uint32_t deformation = 0u;
             deformation < modifier.minimumCount; ++deformation) {
            const std::uint32_t deformationBaselineCount =
                    *eventCount;
            const bool deformationBaselineCanonical =
                    *normalized;
            const std::int64_t minimumTick =
                    modifier.window.minimumTimeMs / tickDurationMs;
            const std::int64_t maximumTick =
                    modifier.window.maximumTimeMs / tickDurationMs;
            const std::int64_t center =
                    random.UniformS64(minimumTick, maximumTick) *
                    tickDurationMs;
            const std::int32_t amplitude = random.UniformS32(
                    modifier.analogMinimum, modifier.analogMaximum);
            std::int64_t start =
                    center - modifier.timeParameterMs;
            if (start < modifier.window.minimumTimeMs) {
                start = modifier.window.minimumTimeMs;
            }
            std::int64_t end = center + modifier.timeParameterMs;
            if (end > modifier.window.maximumTimeMs) {
                end = modifier.window.maximumTimeMs;
            }
            start = start <= 0
                    ? 0
                    : (start / tickDurationMs) * tickDurationMs;
            for (std::int64_t time = start;
                 time <= end; time += tickDurationMs) {
                const std::uint64_t distance =
                        static_cast<std::uint64_t>(
                                time > center ? time - center
                                              : center - time);
                const std::uint32_t weightIndex =
                        modifier.weightOffset +
                        static_cast<std::uint32_t>(
                                distance / tickDurationMs);
                const std::int64_t delta = static_cast<std::int64_t>(
                        llround(static_cast<double>(amplitude) *
                                smoothWeights[weightIndex]));
                const std::int32_t steeringState =
                        deformationBaselineCanonical
                        ? modifier_ops::
                                  ChannelStateAtWithAppendedRun(
                                          events,
                                          deformationBaselineCount,
                                          *eventCount,
                                          4u, 2u, time,
                                          initialControls.steerValue)
                        : SteeringStateAt(
                                  events, *eventCount, time,
                                  initialControls.steerValue);
                const std::int32_t value = SaturateAnalog(
                        static_cast<std::int64_t>(steeringState) +
                        delta);
                if (!PushEvent(
                            events, eventCount, eventCapacity,
                            AnalogEvent(time, 4u, value))) {
                    return false;
                }
            }
            *eventCount = NormalizeEvents(
                    events, *eventCount, temporary,
                    normalizationBaseline, normalizationBaselineCount,
                    mutableFromTimeMs, legacyMutationPipeline,
                    eventCapacity);
            if (*eventCount == UINT32_MAX) {
                return false;
            }
            *normalized = true;
        }
        break;
    case CudaSearchModifierKind::InputInsertion: {
        const auto randomTime = [&]() {
            return random.UniformS64(
                           modifier.window.minimumTimeMs /
                                   tickDurationMs,
                           modifier.window.maximumTimeMs /
                                   tickDurationMs) *
                    tickDurationMs;
        };
        const auto randomHold = [&](std::int64_t maximum) {
            return maximum <= 0
                    ? INT64_C(0)
                    : random.UniformS64(
                                      0, maximum / tickDurationMs) *
                              tickDurationMs;
        };
        if (modifier.steering.enabled != 0u) {
            const std::uint32_t count = random.UniformU32(
                    modifier.steering.minimumCount,
                    modifier.steering.maximumCount);
            for (std::uint32_t index = 0u; index < count; ++index) {
                const std::int64_t start = randomTime();
                std::int64_t end =
                        start + randomHold(
                                        modifier.steering.maximumHoldMs);
                if (end > modifier.window.maximumTimeMs) {
                    end = modifier.window.maximumTimeMs;
                }
                const std::int32_t previous =
                        modifier_ops::RemoveActionRangeAndReadState(
                                events, eventCount, 4u, 2u,
                                start, end,
                                initialControls.steerValue);
                const std::int32_t value =
                        (modifier.optionFlags & 1u) != 0u
                        ? SaturateAnalog(
                                  static_cast<std::int64_t>(previous) +
                                  random.UniformS32(
                                          modifier.secondaryAnalogMinimum,
                                          modifier.secondaryAnalogMaximum))
                        : random.UniformS32(
                                  modifier.analogMinimum,
                                  modifier.analogMaximum);
                if (!PushEvent(
                            events, eventCount, eventCapacity,
                            AnalogEvent(start, 4u, value))) {
                    return false;
                }
                if (end > start &&
                    !PushEvent(
                            events, eventCount, eventCapacity,
                            AnalogEvent(
                                    end, 4u,
                                    SteeringStateAt(
                                            passBaseline,
                                            passBaselineCount,
                                            end,
                                            initialControls.steerValue,
                                            passBaselineCanonical)))) {
                    return false;
                }
            }
        }
        const auto insertSwitch =
                [&](const CudaSearchChannel &channel,
                    std::uint32_t action) {
                    if (channel.enabled == 0u) {
                        return true;
                    }
                    const std::uint32_t count = random.UniformU32(
                            channel.minimumCount,
                            channel.maximumCount);
                    for (std::uint32_t index = 0u;
                         index < count; ++index) {
                        const std::int64_t start = randomTime();
                        std::int64_t end =
                                start + randomHold(
                                                channel.maximumHoldMs);
                        if (end > modifier.window.maximumTimeMs) {
                            end = modifier.window.maximumTimeMs;
                        }
                        const bool previous =
                                modifier_ops::
                                        RemoveActionRangeAndReadState(
                                                events, eventCount,
                                                action, 1u,
                                                start, end,
                                                action == 1u
                                                        ? initialControls.
                                                                  accelerate
                                                        : initialControls.
                                                                  brake) != 0;
                        if (!PushEvent(
                                    events, eventCount, eventCapacity,
                                    SwitchEvent(
                                            start, action, !previous))) {
                            return false;
                        }
                        if (end > start &&
                            !PushEvent(
                                    events, eventCount, eventCapacity,
                                    SwitchEvent(
                                            end, action,
                                            SwitchStateAt(
                                                    passBaseline,
                                                    passBaselineCount,
                                                    action, end,
                                                    action == 1u
                                                            ? initialControls.
                                                                      accelerate
                                                            : initialControls.
                                                                      brake,
                                                    passBaselineCanonical)))) {
                            return false;
                        }
                    }
                    return true;
                };
        if (!insertSwitch(modifier.accelerate, 1u) ||
            !insertSwitch(modifier.brake, 3u)) {
            return false;
        }
        *eventCount = NormalizeEvents(
                events, *eventCount, temporary,
                normalizationBaseline, normalizationBaselineCount,
                mutableFromTimeMs, legacyMutationPipeline,
                eventCapacity);
        if (*eventCount == UINT32_MAX) {
            return false;
        }
        *normalized = true;
        break;
    }
    case CudaSearchModifierKind::InputDeletion: {
        const auto deleteChannel =
                [&](const CudaSearchChannel &channel,
                    std::uint32_t kind) {
                    if (channel.enabled == 0u) {
                        return;
                    }
                    const std::uint32_t requested =
                            random.UniformU32(
                                    0u, channel.maximumCount);
                    if (requested == 0u) {
                        return;
                    }
                    const std::uint32_t initialEligibleCount =
                            modifier_ops::CollectDeletionEligible(
                                    events, *eventCount, eligible,
                                    modifier.window.minimumTimeMs,
                                    modifier.window.maximumTimeMs,
                                    kind, *normalized);
                    std::uint32_t eligibleCount =
                            initialEligibleCount;
                    for (std::uint32_t removal = 0u;
                         removal < requested; ++removal) {
                        if (eligibleCount == 0u) {
                            break;
                        }
                        modifier_ops::SelectDeletionRank(
                                eligible, &eligibleCount,
                                random.UniformU32(
                                        0u, eligibleCount - 1u));
                    }
                    if (eligibleCount != initialEligibleCount) {
                        modifier_ops::CompactSelectedDeletionTail(
                                events, eventCount, eligible,
                                eligibleCount, initialEligibleCount);
                    }
                };
        deleteChannel(modifier.steering, 0u);
        deleteChannel(modifier.accelerate, 1u);
        deleteChannel(modifier.brake, 2u);
        if (!legacyMutationPipeline &&
            modifier.window.minimumTimeMs >= mutableFromTimeMs &&
            *normalized) {
            break;
        }
        *eventCount = NormalizeEvents(
                events, *eventCount, temporary,
                normalizationBaseline, normalizationBaselineCount,
                mutableFromTimeMs, legacyMutationPipeline,
                eventCapacity);
        if (*eventCount == UINT32_MAX) {
            return false;
        }
        *normalized = true;
        break;
    }
    }
    return true;
}

__device__ void ApplyControlPrefix(CudaCandidatePhysicsState &state,
                                   const CudaControlTick &tick) {
    state.world.schemePeriodMs = tick.periodMs;
    state.world.tickTimeMs = tick.timeMs;
}

__device__ bool ValidPackedInputs(
        const void *sceneData,
        const void *configurationData) {
    if (sceneData == nullptr || configurationData == nullptr) {
        return false;
    }
    const auto *scene =
            static_cast<const CudaPackedSceneHeader *>(sceneData);
    const auto *configuration =
            static_cast<const CudaPackedStaticConfigurationHeader *>(
                    configurationData);
    return scene->magic == CudaPackedSceneHeader::Magic &&
            scene->schemaVersion == CudaPackedSceneHeader::SchemaVersion &&
            configuration->magic ==
                    CudaPackedStaticConfigurationHeader::Magic &&
            configuration->schemaVersion ==
                    CudaPackedStaticConfigurationHeader::SchemaVersion;
}

__device__ bool ContainsVolume(
        const CudaSearchEvaluatorConfiguration &evaluator,
        const GmVec3 &position) {
    return static_cast<double>(position.x) >= evaluator.values[0] &&
            static_cast<double>(position.y) >= evaluator.values[1] &&
            static_cast<double>(position.z) >= evaluator.values[2] &&
            static_cast<double>(position.x) <= evaluator.values[3] &&
            static_cast<double>(position.y) <= evaluator.values[4] &&
            static_cast<double>(position.z) <= evaluator.values[5];
}

__device__ bool SegmentEntry(
        const CudaSearchEvaluatorConfiguration &evaluator,
        const GmVec3 &from,
        const GmVec3 &to,
        double *fraction) {
    double enter = 0.0;
    double leave = 1.0;
    const double a[3]{from.x, from.y, from.z};
    const double b[3]{to.x, to.y, to.z};
    for (std::uint32_t axis = 0u; axis < 3u; ++axis) {
        const double delta = b[axis] - a[axis];
        if (fabs(delta) <= 1e-12) {
            if (a[axis] < evaluator.values[axis] ||
                a[axis] > evaluator.values[axis + 3u]) {
                return false;
            }
            continue;
        }
        double near =
                (evaluator.values[axis] - a[axis]) / delta;
        double far =
                (evaluator.values[axis + 3u] - a[axis]) / delta;
        if (near > far) {
            const double swap = near;
            near = far;
            far = swap;
        }
        enter = enter > near ? enter : near;
        leave = leave < far ? leave : far;
        if (enter > leave) {
            return false;
        }
    }
    if (enter < 0.0 || enter > 1.0) {
        return false;
    }
    *fraction = enter;
    return true;
}

__device__ DeviceSample EvaluateState(
        const CudaSearchEvaluatorConfiguration &evaluator,
        const CudaCandidatePhysicsState &state,
        const GmVec3 &previousPosition,
        double previousTimeMs,
        double currentTimeMs,
        bool *reported) {
    DeviceSample result;
    result.timeMs = currentTimeMs;
    const GmVec3 &position = state.body.current.position;
    switch (evaluator.kind) {
    case CudaSearchEvaluatorKind::Velocity: {
        const GmVec3 &velocity = state.body.current.linearSpeed;
        const double x = velocity.x;
        const double y = velocity.y;
        const double z = velocity.z;
        const double speed = sqrt((x * x + y * y) + z * z);
        double alignment = 1.0;
        if ((evaluator.optionFlags & 3u) != 0u) {
            alignment = speed <= 1e-12
                    ? 0.0
                    : (x * evaluator.values[0] +
                       y * evaluator.values[1] +
                       z * evaluator.values[2]) /
                              speed;
            if (alignment < evaluator.values[3]) {
                return result;
            }
        }
        result.score = (evaluator.optionFlags & 1u) != 0u
                ? x * evaluator.values[0] +
                          y * evaluator.values[1] +
                          z * evaluator.values[2]
                : speed;
        result.detail0 = speed;
        result.detail1 = alignment;
        result.valid = true;
        break;
    }
    case CudaSearchEvaluatorKind::Point: {
        const double x = static_cast<double>(position.x) -
                evaluator.values[0];
        const double y = static_cast<double>(position.y) -
                evaluator.values[1];
        const double z = static_cast<double>(position.z) -
                evaluator.values[2];
        result.score = sqrt((x * x + y * y) + z * z);
        result.valid = true;
        break;
    }
    case CudaSearchEvaluatorKind::Pose: {
        const double x = static_cast<double>(position.x) -
                evaluator.values[0];
        const double y = static_cast<double>(position.y) -
                evaluator.values[1];
        const double z = static_cast<double>(position.z) -
                evaluator.values[2];
        const double positionError =
                sqrt((x * x + y * y) + z * z);
        const GmQuat &rotation = state.body.current.rotationQuat;
        double dot = fabs(
                evaluator.values[3] * rotation.x +
                evaluator.values[4] * rotation.y +
                evaluator.values[5] * rotation.z +
                evaluator.values[6] * rotation.w);
        dot = dot < 0.0 ? 0.0 : (dot > 1.0 ? 1.0 : dot);
        const double rotationError = 2.0 * acos(dot);
        result.score =
                (1.0 - evaluator.values[7]) * positionError +
                evaluator.values[7] * rotationError;
        result.detail0 = positionError;
        result.detail1 = rotationError;
        result.valid = true;
        break;
    }
    case CudaSearchEvaluatorKind::VolumeEntry:
        if (*reported || ContainsVolume(evaluator, previousPosition)) {
            return result;
        } else {
            double fraction = 0.0;
            if (!SegmentEntry(
                        evaluator, previousPosition, position,
                        &fraction)) {
                return result;
            }
            *reported = true;
            result.timeMs = previousTimeMs +
                    fraction * (currentTimeMs - previousTimeMs);
            result.score = result.timeMs;
            result.valid = true;
        }
        break;
    case CudaSearchEvaluatorKind::FinishTime:
        if (*reported || !state.race.progress.raceCompleted) {
            return result;
        }
        *reported = true;
        result.timeMs =
                state.race.progress.lastPrepareTimeMs;
        result.score = result.timeMs;
        result.valid = true;
        break;
    }
    return result;
}

__device__ bool MaximizeEvaluator(
        const CudaSearchEvaluatorConfiguration &evaluator) {
    return evaluator.kind == CudaSearchEvaluatorKind::Velocity;
}

__global__ void SeedCandidateBestSamplesKernel(
        DeviceSample *candidateBestSamples,
        const DeviceSample *incumbent) {
    if (blockIdx.x == 0u && threadIdx.x == 0u) {
        DeviceSample seed = *incumbent;
        seed.logicalOrder = 0u;
        seed.candidateSlot = InvalidCandidateSlot;
        candidateBestSamples[0] = seed;
    }
}

__global__ void GenerateSearchCandidatesKernel(
        const CudaSearchInputEvent *baselineInputs,
        std::uint32_t baselineInputCount,
        const CudaSearchModifierConfiguration *modifiers,
        std::uint32_t modifierCount,
        const double *smoothWeights,
        const DeviceControlState *mutableBoundaryControls,
        std::uint32_t tickDurationMs,
        std::uint64_t firstCandidateId,
        std::uint32_t candidateCount,
        bool baseline,
        bool legacyMutationPipeline,
        bool baselineInputsCanonical,
        bool compactRandomSteeringPipeline,
        std::uint32_t eventCapacity,
        const std::uint32_t *compactInputIndices,
        std::uint32_t compactInputCount,
        std::int32_t *candidateInputValues,
        DeviceSample *candidateBestSamples,
        std::uint32_t *randomStateWords,
        CudaSearchInputEvent *candidateEvents,
        CudaSearchInputEvent *temporaryEvents,
        CudaSearchInputEvent *passBaselineEvents,
        std::uint32_t *eligibleIndices,
        std::uint32_t *eventCounts,
        std::uint32_t *mutationCounts,
        DeviceCandidateStatus *statuses,
        bool *activeCandidates,
        const std::uint32_t *cancellation) {
    const std::uint32_t slot =
            blockIdx.x * blockDim.x + threadIdx.x;
    if (slot >= candidateCount) {
        return;
    }
    const std::uint64_t candidateId = firstCandidateId + slot;
    CudaSearchInputEvent *events =
            candidateEvents == nullptr
            ? nullptr
            : candidateEvents +
                      static_cast<std::uint64_t>(slot) * eventCapacity;
    CudaSearchInputEvent *temporary =
            temporaryEvents == nullptr
            ? nullptr
            : temporaryEvents +
                      static_cast<std::uint64_t>(slot) * eventCapacity;
    CudaSearchInputEvent *passBaseline =
            passBaselineEvents == nullptr
            ? nullptr
            : passBaselineEvents +
                      static_cast<std::uint64_t>(slot) * eventCapacity;
    std::uint32_t *eligible =
            eligibleIndices == nullptr
            ? nullptr
            : eligibleIndices +
                      static_cast<std::uint64_t>(slot) * eventCapacity;
    DeviceMt19937 random(randomStateWords, slot, candidateCount);
    std::uint32_t eventCount = baselineInputCount;
    if (compactRandomSteeringPipeline) {
        for (std::uint32_t index = 0u;
             index < compactInputCount; ++index) {
            candidateInputValues[
                    static_cast<std::uint64_t>(index) *
                            candidateCount +
                    slot] =
                    baselineInputs[compactInputIndices[index]].value;
        }
    } else {
        for (std::uint32_t index = 0u;
             index < baselineInputCount; ++index) {
            events[index] = baselineInputs[index];
        }
    }
    statuses[slot] = DeviceCandidateStatus::Success;
    candidateBestSamples[slot + 1u] = {};
    if (*reinterpret_cast<volatile const std::uint32_t *>(
                cancellation) != 0u) {
        statuses[slot] = DeviceCandidateStatus::Cancelled;
        activeCandidates[slot] = false;
        eventCounts[slot] = eventCount;
        mutationCounts[slot] = 0u;
        return;
    }
    if (compactRandomSteeringPipeline && baseline) {
        eventCounts[slot] = baselineInputCount;
        mutationCounts[slot] = 0u;
        activeCandidates[slot] = true;
        return;
    }
    if (!baseline) {
        if (compactRandomSteeringPipeline) {
            for (std::uint32_t pass = 0u;
                 pass < modifierCount; ++pass) {
                const CudaSearchModifierConfiguration modifier =
                        modifiers[pass];
                random.Seed(
                        modifier.window.seed, candidateId, pass);
                for (std::uint32_t compactIndex = 0u;
                     compactIndex < compactInputCount;
                     ++compactIndex) {
                    const CudaSearchInputEvent &input =
                            baselineInputs[
                                    compactInputIndices[compactIndex]];
                    if (input.timeMs <
                                    modifier.window.minimumTimeMs ||
                        input.timeMs >
                                    modifier.window.maximumTimeMs) {
                        continue;
                    }
                    std::int32_t value =
                            random.UniformS32(
                                    -65536, 65536);
                    std::int32_t &compactValue =
                            candidateInputValues[
                                    static_cast<std::uint64_t>(
                                            compactIndex) *
                                            candidateCount +
                                    slot];
                    if (value == compactValue) {
                        value = value == 65536 ? -65536 : 65536;
                    }
                    compactValue = value;
                }
            }
            std::uint32_t mutationCount = 0u;
            for (std::uint32_t index = 0u;
                 index < compactInputCount; ++index) {
                if (candidateInputValues[
                            static_cast<std::uint64_t>(index) *
                                    candidateCount +
                            slot] !=
                    baselineInputs[compactInputIndices[index]].value) {
                    ++mutationCount;
                }
            }
            eventCounts[slot] = baselineInputCount;
            mutationCounts[slot] = mutationCount;
            activeCandidates[slot] = mutationCount != 0u;
            return;
        }
        bool normalized = baselineInputsCanonical;
        for (std::uint32_t pass = 0u; pass < modifierCount; ++pass) {
            if (!ApplyModifier(
                        modifiers[pass], pass, candidateId,
                        random,
                        tickDurationMs, 0,
                        *mutableBoundaryControls,
                        baselineInputs, baselineInputCount,
                        events, &eventCount, eventCapacity,
                        temporary, passBaseline, eligible,
                        smoothWeights, legacyMutationPipeline,
                        &normalized)) {
                statuses[slot] =
                        DeviceCandidateStatus::CapacityExceeded;
                activeCandidates[slot] = false;
                eventCounts[slot] = eventCount;
                mutationCounts[slot] = 0u;
                return;
            }
        }
        if (legacyMutationPipeline || !normalized) {
            for (std::uint32_t index = 0u;
                 index < baselineInputCount; ++index) {
                passBaseline[index] = baselineInputs[index];
            }
            eventCount = NormalizeEvents(
                    events, eventCount, temporary,
                    passBaseline, baselineInputCount,
                    0,
                    legacyMutationPipeline, eventCapacity);
            if (eventCount == UINT32_MAX) {
                statuses[slot] =
                        DeviceCandidateStatus::CapacityExceeded;
                activeCandidates[slot] = false;
                eventCounts[slot] = eventCapacity;
                mutationCounts[slot] = 0u;
                return;
            }
        }
    }
    const std::uint32_t mutationCount = baseline
            ? 0u
            : EffectiveChangeCount(
                      baselineInputs, baselineInputCount,
                      events, eventCount);
    eventCounts[slot] = eventCount;
    mutationCounts[slot] = mutationCount;
    const bool active = baseline || mutationCount != 0u;
    activeCandidates[slot] = active;
}

template <typename State, bool SimulateStunts>
__device__ State LoadSearchState(
        const CudaCandidateState *branchState) {
    if constexpr (SimulateStunts) {
        return *branchState;
    } else {
        return static_cast<const CudaCandidatePhysicsState &>(
                *branchState);
    }
}

template <
        typename State,
        bool SimulateStunts,
        std::uint32_t MinimumBlocksPerSm>
__global__ __launch_bounds__(
        SimulationBlockSize,
        MinimumBlocksPerSm) void SimulateSearchCandidatesKernel(
        const void *sceneData,
        const void *configurationData,
        const CudaCandidateState *branchState,
        const DeviceControlState *mutableBoundaryControls,
        const CudaControlTick *baselineTicks,
        std::uint32_t timelineTickCount,
        const CudaSearchEvaluatorConfiguration *evaluator,
        std::uint32_t tickDurationMs,
        std::uint32_t prestartDurationMs,
        std::int64_t branchTimeMs,
        std::int64_t mutableFromTimeMs,
        std::int64_t evaluationStartTimeMs,
        std::uint32_t evaluationTickCount,
        std::uint64_t firstCandidateId,
        std::uint32_t candidateCount,
        bool baseline,
        std::uint32_t eventCapacity,
        DeviceSample *candidateBestSamples,
        const CudaSearchInputEvent *baselineInputs,
        const CudaSearchInputEvent *candidateEvents,
        const std::int32_t *candidateInputValues,
        const std::uint32_t *compactInputOffsets,
        std::uint32_t compactInputCount,
        bool compactRandomSteeringPipeline,
        const std::uint32_t *eventCounts,
        DeviceCandidateStatus *statuses,
        const bool *activeCandidates,
        cuda::collision::CudaCollision *collisionScratch,
        cuda::collision::CudaCollision *shapeCollisionScratch,
        GmIso4 *shapeWorldScratch,
        GmBoxAligned *movingBoundsScratch,
        cuda::collision::CudaCollisionSurfaceHit *
                surfaceHitScratch,
        cuda::collision::CudaCollisionMeshRange *
                meshRangeScratch,
        std::uint32_t *meshCellScratch,
        std::uint32_t scratchStride,
        std::uint32_t shapeCapacity,
        const std::uint32_t *cancellation) {
    const std::uint32_t slot =
            blockIdx.x * blockDim.x + threadIdx.x;
    if (slot >= candidateCount || !activeCandidates[slot]) {
        return;
    }
    const std::uint64_t candidateId = firstCandidateId + slot;
    const CudaSearchInputEvent *events =
            candidateEvents == nullptr
            ? nullptr
            : candidateEvents +
                      static_cast<std::uint64_t>(slot) * eventCapacity;
    const std::uint32_t eventCount = eventCounts[slot];
    const CudaSearchEvaluatorConfiguration configuredEvaluator =
            *evaluator;
    if (!ValidPackedInputs(sceneData, configurationData) ||
        branchState->schemaVersion != CudaCandidateState::SchemaVersion) {
        statuses[slot] =
                DeviceCandidateStatus::UnsupportedPhysicsTransition;
        return;
    }

    State state =
            LoadSearchState<State, SimulateStunts>(branchState);
    state.candidateId = static_cast<std::uint32_t>(candidateId);
    cuda::collision::CudaCollisionSearchScratch candidateScratch{
            0u,
            0u,
            0u,
            false,
            MinimumBlocksPerSm ==
                    ThroughputKernelMinimumBlocksPerSm,
            collisionScratch,
            shapeCollisionScratch,
            shapeWorldScratch,
            movingBoundsScratch,
            surfaceHitScratch,
            meshRangeScratch,
            meshCellScratch,
            slot,
            scratchStride,
            shapeCapacity};
    DeviceControlState controlState = *mutableBoundaryControls;
    std::uint32_t eventCursor = 0u;
    bool evaluatorReported = false;
    const bool maximize = MaximizeEvaluator(configuredEvaluator);
    DeviceSample localBest;
    std::uint32_t evaluationIndex = 0u;
    for (std::uint32_t tickIndex = 0u;
         tickIndex < timelineTickCount; ++tickIndex) {
        if ((tickIndex & 63u) == 0u &&
            *reinterpret_cast<volatile const std::uint32_t *>(
                    cancellation) != 0u) {
            statuses[slot] = DeviceCandidateStatus::Cancelled;
            candidateBestSamples[slot + 1u] = localBest;
            return;
        }
        const std::int64_t publicTime =
                branchTimeMs +
                static_cast<std::int64_t>(tickIndex + 1u) *
                        tickDurationMs;
        const std::int64_t suffixTime =
                publicTime - mutableFromTimeMs;
        while (eventCursor < eventCount) {
            const CudaSearchInputEvent event = CandidateInputAt(
                    baselineInputs, events, candidateInputValues,
                    compactInputOffsets,
                    compactRandomSteeringPipeline, eventCursor,
                    slot, candidateCount);
            if (event.timeMs > suffixTime) {
                break;
            }
            ApplyControlEvent(
                    controlState, event, mutableFromTimeMs);
            ++eventCursor;
        }
        CudaControlTick tick = baselineTicks[tickIndex];
        tick.controls = ControlsFromState(controlState);
        tick.stuntsInput =
                StuntsFromState(controlState, prestartDurationMs);
        const GmVec3 previousPosition = state.body.current.position;
        ApplyControlPrefix(state, tick);
        if (!state.firstStep) {
            cuda::transition::PrepareStep(
                    state, tick,
                    static_cast<const
                            CudaPackedStaticConfigurationHeader *>(
                            configurationData));
        }
        state.vehicle.mobil.absorbContactEnabled = true;
        state.vehicle.mobil.physicsUpdatesEnabled =
                (tick.actionFlags &
                 CudaControlActionSuppressVehicleForceCallbacks) == 0u;
        for (std::uint32_t respawn = 0u;
             respawn < tick.respawnAtCheckpointCount; ++respawn) {
            if (cuda::transition::Respawn(
                        state,
                        static_cast<const
                                CudaPackedStaticConfigurationHeader *>(
                                configurationData))) {
                ++state.incrementalRespawnCount;
                if constexpr (SimulateStunts) {
                    cuda::stunts::ApplyRespawnPenalty(
                            state.stunts);
                }
            }
        }
        const cuda::physics::Status physicsStatus =
                cuda::physics::Step<
                        false,
                        MinimumBlocksPerSm ==
                                ThroughputKernelMinimumBlocksPerSm,
                        true>(
                        static_cast<const CudaPackedSceneHeader *>(
                                sceneData),
                        static_cast<const
                                CudaPackedStaticConfigurationHeader *>(
                                configurationData),
                        state, candidateScratch);
        if (physicsStatus != cuda::physics::Status::Success) {
            statuses[slot] =
                    DeviceCandidateStatus::UnsupportedPhysicsTransition;
            candidateBestSamples[slot + 1u] = localBest;
            return;
        }
        if constexpr (SimulateStunts) {
            const cuda::stunts::Status stuntStatus =
                    cuda::stunts::Update(state, tick);
            if (stuntStatus != cuda::stunts::Status::Success) {
                statuses[slot] =
                        DeviceCandidateStatus::CapacityExceeded;
                candidateBestSamples[slot + 1u] = localBest;
                return;
            }
        }
        state.firstStep = false;
        ++state.controlCursor;
        if (publicTime < evaluationStartTimeMs) {
            continue;
        }
        DeviceSample sample = EvaluateState(
                configuredEvaluator, state, previousPosition,
                static_cast<double>(publicTime - tickDurationMs),
                static_cast<double>(publicTime),
                &evaluatorReported);
        sample.candidateId = candidateId;
        sample.candidateSlot = slot;
        sample.evaluationTick = evaluationIndex;
        sample.logicalOrder =
                1u +
                static_cast<std::uint64_t>(slot) *
                        evaluationTickCount +
                evaluationIndex;
        sample.mutation = !baseline;
        if (StrictlyBetter(sample, localBest, maximize)) {
            localBest = sample;
        }
        ++evaluationIndex;
    }
    candidateBestSamples[slot + 1u] = localBest;
}

template <typename State, bool SimulateStunts>
__global__ void RefineSearchFinishTimesKernel(
        const void *sceneData,
        const void *configurationData,
        const CudaCandidateState *branchState,
        const DeviceControlState *mutableBoundaryControls,
        const CudaControlTick *baselineTicks,
        std::uint32_t timelineTickCount,
        std::uint32_t tickDurationMs,
        std::uint32_t prestartDurationMs,
        std::int64_t branchTimeMs,
        std::int64_t mutableFromTimeMs,
        std::uint32_t eventCapacity,
        DeviceSample *candidateBestSamples,
        cuda::finish::Refinement *finishRefinements,
        const CudaSearchInputEvent *baselineInputs,
        const CudaSearchInputEvent *candidateEvents,
        const std::int32_t *candidateInputValues,
        const std::uint32_t *compactInputOffsets,
        std::uint32_t compactInputCount,
        bool compactRandomSteeringPipeline,
        const std::uint32_t *eventCounts,
        DeviceCandidateStatus *statuses,
        const bool *activeCandidates,
        cuda::collision::CudaCollision *collisionScratch,
        cuda::collision::CudaCollision *shapeCollisionScratch,
        GmIso4 *shapeWorldScratch,
        GmBoxAligned *movingBoundsScratch,
        cuda::collision::CudaCollisionSurfaceHit *
                surfaceHitScratch,
        cuda::collision::CudaCollisionMeshRange *
                meshRangeScratch,
        std::uint32_t *meshCellScratch,
        std::uint32_t scratchStride,
        std::uint32_t shapeCapacity,
        const std::uint32_t *cancellation,
        std::uint32_t candidateCount) {
    const std::uint32_t slot =
            blockIdx.x * blockDim.x + threadIdx.x;
    if (slot >= candidateCount || !activeCandidates[slot]) {
        return;
    }
    DeviceSample &sample = candidateBestSamples[slot + 1u];
    if (!sample.valid ||
        statuses[slot] != DeviceCandidateStatus::Success) {
        return;
    }
    cuda::finish::Refinement &refinement =
            finishRefinements[slot];
    refinement = {};
    const CudaSearchInputEvent *events =
            candidateEvents == nullptr
            ? nullptr
            : candidateEvents +
                      static_cast<std::uint64_t>(slot) * eventCapacity;
    const std::uint32_t eventCount = eventCounts[slot];
    State state =
            LoadSearchState<State, SimulateStunts>(branchState);
    state.candidateId =
            static_cast<std::uint32_t>(sample.candidateId);
    cuda::collision::CudaCollisionSearchScratch candidateScratch{
            0u,
            0u,
            0u,
            false,
            false,
            collisionScratch,
            shapeCollisionScratch,
            shapeWorldScratch,
            movingBoundsScratch,
            surfaceHitScratch,
            meshRangeScratch,
            meshCellScratch,
            slot,
            scratchStride,
            shapeCapacity};
    DeviceControlState controlState = *mutableBoundaryControls;
    std::uint32_t eventCursor = 0u;
    for (std::uint32_t tickIndex = 0u;
         tickIndex < timelineTickCount; ++tickIndex) {
        if ((tickIndex & 63u) == 0u &&
            *reinterpret_cast<volatile const std::uint32_t *>(
                    cancellation) != 0u) {
            statuses[slot] = DeviceCandidateStatus::Cancelled;
            return;
        }
        const std::int64_t publicTime =
                branchTimeMs +
                static_cast<std::int64_t>(tickIndex + 1u) *
                        tickDurationMs;
        const std::int64_t suffixTime =
                publicTime - mutableFromTimeMs;
        while (eventCursor < eventCount) {
            const CudaSearchInputEvent event = CandidateInputAt(
                    baselineInputs, events, candidateInputValues,
                    compactInputOffsets,
                    compactRandomSteeringPipeline, eventCursor,
                    slot, candidateCount);
            if (event.timeMs > suffixTime) {
                break;
            }
            ApplyControlEvent(
                    controlState, event, mutableFromTimeMs);
            ++eventCursor;
        }
        CudaControlTick tick = baselineTicks[tickIndex];
        tick.controls = ControlsFromState(controlState);
        tick.stuntsInput =
                StuntsFromState(controlState, prestartDurationMs);
        ApplyControlPrefix(state, tick);
        if (!state.firstStep) {
            cuda::transition::PrepareStep(
                    state, tick,
                    static_cast<const
                            CudaPackedStaticConfigurationHeader *>(
                            configurationData));
        }
        state.vehicle.mobil.absorbContactEnabled = true;
        state.vehicle.mobil.physicsUpdatesEnabled =
                (tick.actionFlags &
                 CudaControlActionSuppressVehicleForceCallbacks) == 0u;
        for (std::uint32_t respawn = 0u;
             respawn < tick.respawnAtCheckpointCount; ++respawn) {
            if (cuda::transition::Respawn(
                        state,
                        static_cast<const
                                CudaPackedStaticConfigurationHeader *>(
                                configurationData))) {
                ++state.incrementalRespawnCount;
                if constexpr (SimulateStunts) {
                    cuda::stunts::ApplyRespawnPenalty(state.stunts);
                }
            }
        }
        const cuda::physics::Status physicsStatus =
                cuda::finish::StepAndRefine<false, false, true>(
                        static_cast<const CudaPackedSceneHeader *>(
                                sceneData),
                        static_cast<const
                                CudaPackedStaticConfigurationHeader *>(
                                configurationData),
                        state, tick, candidateScratch, refinement);
        if (physicsStatus != cuda::physics::Status::Success ||
            refinement.failed) {
            statuses[slot] =
                    DeviceCandidateStatus::UnsupportedPhysicsTransition;
            return;
        }
        if (refinement.present) {
            const std::uint64_t prestartNs =
                    static_cast<std::uint64_t>(
                            prestartDurationMs) *
                    1000000u;
            if (refinement.estimate.lowerBoundNs < prestartNs ||
                refinement.estimate.upperBoundNs < prestartNs) {
                statuses[slot] =
                        DeviceCandidateStatus::
                                UnsupportedPhysicsTransition;
                return;
            }
            sample.score = static_cast<double>(
                    refinement.estimate.upperBoundNs - prestartNs);
            sample.timeMs = sample.score / 1000000.0;
            return;
        }
        if constexpr (SimulateStunts) {
            const cuda::stunts::Status stuntStatus =
                    cuda::stunts::Update(state, tick);
            if (stuntStatus != cuda::stunts::Status::Success) {
                statuses[slot] =
                        DeviceCandidateStatus::CapacityExceeded;
                return;
            }
        }
        state.firstStep = false;
        ++state.controlCursor;
    }
    statuses[slot] =
            DeviceCandidateStatus::UnsupportedPhysicsTransition;
}

__global__ void CaptureSearchWinnerStateKernel(
        const void *sceneData,
        const void *configurationData,
        const CudaCandidateState *branchState,
        const DeviceControlState *mutableBoundaryControls,
        const CudaControlTick *baselineTicks,
        const DeviceSample *reducedBest,
        const cuda::finish::Refinement *finishRefinements,
        std::uint32_t tickDurationMs,
        std::uint32_t prestartDurationMs,
        std::int64_t branchTimeMs,
        std::int64_t mutableFromTimeMs,
        std::int64_t evaluationStartTimeMs,
        std::uint32_t eventCapacity,
        const CudaSearchInputEvent *baselineInputs,
        const CudaSearchInputEvent *candidateEvents,
        const std::int32_t *candidateInputValues,
        const std::uint32_t *compactInputOffsets,
        std::uint32_t compactInputCount,
        bool compactRandomSteeringPipeline,
        std::uint32_t candidateCount,
        const std::uint32_t *eventCounts,
        DeviceCandidateStatus *statuses,
        cuda::collision::CudaCollision *collisionScratch,
        cuda::collision::CudaCollision *shapeCollisionScratch,
        GmIso4 *shapeWorldScratch,
        GmBoxAligned *movingBoundsScratch,
        cuda::collision::CudaCollisionSurfaceHit *
                surfaceHitScratch,
        cuda::collision::CudaCollisionMeshRange *
                meshRangeScratch,
        std::uint32_t *meshCellScratch,
        std::uint32_t scratchStride,
        std::uint32_t shapeCapacity,
        CudaCandidateState *capturedWinnerState) {
    if (blockIdx.x != 0u || threadIdx.x != 0u) {
        return;
    }
    const DeviceSample winner = *reducedBest;
    if (!winner.valid ||
        winner.candidateSlot == InvalidCandidateSlot) {
        return;
    }
    const std::uint32_t slot = winner.candidateSlot;
    const CudaSearchInputEvent *events =
            candidateEvents == nullptr
            ? nullptr
            : candidateEvents +
                      static_cast<std::uint64_t>(slot) * eventCapacity;
    const std::uint32_t eventCount = eventCounts[slot];
    const std::uint32_t evaluationStartTick =
            static_cast<std::uint32_t>(
                    (evaluationStartTimeMs -
                     (branchTimeMs + tickDurationMs)) /
                    tickDurationMs);
    const std::uint32_t targetTick =
            evaluationStartTick + winner.evaluationTick;

    cuda::collision::CudaCollisionSearchScratch candidateScratch{
            0u,
            0u,
            0u,
            false,
            true,
            collisionScratch,
            shapeCollisionScratch,
            shapeWorldScratch,
            movingBoundsScratch,
            surfaceHitScratch,
            meshRangeScratch,
            meshCellScratch,
            slot,
            scratchStride,
            shapeCapacity};
    CudaCandidateState state = *branchState;
    state.candidateId =
            static_cast<std::uint32_t>(winner.candidateId);
    DeviceControlState controlState = *mutableBoundaryControls;
    std::uint32_t eventCursor = 0u;
    for (std::uint32_t tickIndex = 0u;
         tickIndex <= targetTick; ++tickIndex) {
        const std::int64_t publicTime =
                branchTimeMs +
                static_cast<std::int64_t>(tickIndex + 1u) *
                        tickDurationMs;
        const std::int64_t suffixTime =
                publicTime - mutableFromTimeMs;
        while (eventCursor < eventCount) {
            const CudaSearchInputEvent event = CandidateInputAt(
                    baselineInputs, events, candidateInputValues,
                    compactInputOffsets,
                    compactRandomSteeringPipeline, eventCursor,
                    slot, candidateCount);
            if (event.timeMs > suffixTime) {
                break;
            }
            ApplyControlEvent(
                    controlState, event, mutableFromTimeMs);
            ++eventCursor;
        }
        CudaControlTick tick = baselineTicks[tickIndex];
        tick.controls = ControlsFromState(controlState);
        tick.stuntsInput =
                StuntsFromState(controlState, prestartDurationMs);
        ApplyControlPrefix(state, tick);
        if (!state.firstStep) {
            cuda::transition::PrepareStep(
                    state, tick,
                    static_cast<const
                            CudaPackedStaticConfigurationHeader *>(
                            configurationData));
        }
        state.vehicle.mobil.absorbContactEnabled = true;
        state.vehicle.mobil.physicsUpdatesEnabled =
                (tick.actionFlags &
                 CudaControlActionSuppressVehicleForceCallbacks) == 0u;
        for (std::uint32_t respawn = 0u;
             respawn < tick.respawnAtCheckpointCount; ++respawn) {
            if (cuda::transition::Respawn(
                        state,
                        static_cast<const
                                CudaPackedStaticConfigurationHeader *>(
                                configurationData))) {
                ++state.incrementalRespawnCount;
                cuda::stunts::ApplyRespawnPenalty(
                        state.stunts);
            }
        }
        const cuda::physics::Status physicsStatus =
                cuda::physics::Step<false, false, true>(
                        static_cast<const CudaPackedSceneHeader *>(
                                sceneData),
                        static_cast<const
                                CudaPackedStaticConfigurationHeader *>(
                                configurationData),
                        state, candidateScratch);
        if (physicsStatus != cuda::physics::Status::Success) {
            statuses[slot] =
                    DeviceCandidateStatus::UnsupportedPhysicsTransition;
            return;
        }
        if (state.stuntsEnabled) {
            const cuda::stunts::Status stuntStatus =
                    cuda::stunts::Update(state, tick);
            if (stuntStatus != cuda::stunts::Status::Success) {
                statuses[slot] =
                        DeviceCandidateStatus::CapacityExceeded;
                return;
            }
        }
        state.firstStep = false;
        ++state.controlCursor;
    }
    cuda::collision::detail::CaptureReplacementOverflow(
            candidateScratch,
            state.collisionReplacementOverflow);
    if (finishRefinements != nullptr &&
        finishRefinements[slot].present) {
        const forevervalidator::FinishTimeEstimate &finishTime =
                finishRefinements[slot].estimate;
        state.finishTime.present = true;
        state.finishTime.value = {
                finishTime.lowerBoundNs,
                finishTime.upperBoundNs,
                finishTime.estimatedNs};
    }
    *capturedWinnerState = state;
}

__global__ void FinalizeSearchBatchKernel(
        const DeviceSample *reducedBest,
        const CudaCandidateState *capturedWinnerState,
        const DeviceSample *candidateBestSamples,
        const CudaSearchInputEvent *baselineInputs,
        const CudaSearchInputEvent *candidateEvents,
        const std::int32_t *candidateInputValues,
        const std::uint32_t *compactInputOffsets,
        std::uint32_t compactInputCount,
        bool compactRandomSteeringPipeline,
        const std::uint32_t *eventCounts,
        const std::uint32_t *mutationCounts,
        const DeviceCandidateStatus *statuses,
        const bool *activeCandidates,
        std::uint32_t candidateCount,
        std::uint32_t eventCapacity,
        std::uint32_t evaluationTickCount,
        bool maximize,
        bool baseline,
        DeviceSample *globalBestSample,
        CudaCandidateState *globalBestState,
        CudaSearchInputEvent *globalBestInputs,
        std::uint32_t *globalBestEventCount,
        std::uint32_t *globalBestMutationCount,
        DeviceBatchSummary *summary) {
    if (blockIdx.x != 0u || threadIdx.x != 0u) {
        return;
    }
    DeviceBatchSummary result;
    for (std::uint32_t slot = 0u; slot < candidateCount; ++slot) {
        if (statuses[slot] == DeviceCandidateStatus::Cancelled) {
            result.status = CudaSearchStatus::Cancelled;
        } else if (statuses[slot] ==
                   DeviceCandidateStatus::CapacityExceeded) {
            result.status = CudaSearchStatus::CapacityExceeded;
        } else if (statuses[slot] ==
                   DeviceCandidateStatus::UnsupportedPhysicsTransition) {
            result.status =
                    CudaSearchStatus::UnsupportedPhysicsTransition;
        }
        if (activeCandidates[slot]) {
            ++result.evaluatedCandidateCount;
            result.evaluatorCalls += evaluationTickCount;
        }
        result.totalMutationCount += mutationCounts[slot];
    }

    DeviceSample incumbent = candidateBestSamples[0];
    if (!baseline) {
        for (std::uint32_t slot = 0u;
             slot < candidateCount; ++slot) {
            const DeviceSample sample =
                    candidateBestSamples[slot + 1u];
            if (StrictlyBetter(sample, incumbent, maximize)) {
                ++result.mutationImprovementCount;
                incumbent = sample;
            }
        }
    }

    const DeviceSample winner = *reducedBest;
    if (winner.valid &&
        winner.candidateSlot != InvalidCandidateSlot) {
        const std::uint32_t slot = winner.candidateSlot;
        const DeviceSample candidateBest =
                candidateBestSamples[slot + 1u];
        if (candidateBest.valid) {
            *globalBestSample = candidateBest;
            *globalBestState = *capturedWinnerState;
            *globalBestEventCount = eventCounts[slot];
            *globalBestMutationCount = mutationCounts[slot];
            const CudaSearchInputEvent *materializedInputs =
                    candidateEvents == nullptr
                    ? nullptr
                    : candidateEvents +
                              static_cast<std::uint64_t>(slot) *
                                      eventCapacity;
            for (std::uint32_t index = 0u;
                 index < eventCounts[slot]; ++index) {
                globalBestInputs[index] = CandidateInputAt(
                        baselineInputs, materializedInputs,
                        candidateInputValues, compactInputOffsets,
                        compactRandomSteeringPipeline, index,
                        slot, candidateCount);
            }
            result.bestChanged = true;
        }
    }
    result.bestValid = globalBestSample->valid;
    result.bestMutation = globalBestSample->mutation;
    result.bestCandidateId = globalBestSample->candidateId;
    result.bestMutationCount = *globalBestMutationCount;
    result.globalEventCount = *globalBestEventCount;
    *summary = result;
}

}  // namespace

struct CudaSearchExecutor::Impl {
    struct SimulationKernelMetrics {
        std::uint32_t registersPerThread = 0u;
        std::uint64_t localBytesPerThread = 0u;
        std::uint32_t activeBlocksPerMultiprocessor = 0u;
        double theoreticalOccupancy = 0.0;
    };

    CudaSearchExecutorConfiguration configuration;
    std::vector<CudaSearchInputEvent> immutableInputPrefix;
    std::int64_t mutableFromTimeMs = 0;
    std::uint32_t timelineTickCount = 0u;
    std::uint32_t evaluationTickCount = 0u;
    std::uint32_t collisionShapeCount = 0u;
    std::uint64_t residentBytes = 0u;
    std::uint64_t winnerSelectionBytes = 0u;
    std::uint64_t initialUploadBytes = 0u;
    bool baselineEvaluated = false;
    bool baselineInputsCanonical = false;
    bool compactRandomSteeringPipeline = false;
    bool needsTemporaryEvents = true;
    bool needsPassBaselineEvents = true;
    bool needsEligibleIndices = true;
    std::uint32_t compactInputCount = 0u;
    std::uint32_t multiprocessorCount = 0u;
    SimulationKernelMetrics latencyKernelMetrics;
    SimulationKernelMetrics throughputKernelMetrics;

    DeviceAllocation<CudaCandidateState> branchState;
    DeviceAllocation<DeviceControlState> branchControls;
    DeviceAllocation<DeviceControlState> mutableBoundaryControls;
    DeviceAllocation<CudaControlTick> baselineTicks;
    DeviceAllocation<CudaSearchInputEvent> baselineInputs;
    DeviceAllocation<CudaSearchModifierConfiguration> modifiers;
    DeviceAllocation<double> smoothWeights;
    DeviceAllocation<CudaSearchEvaluatorConfiguration> evaluator;
    DeviceAllocation<CudaCandidateState> capturedWinnerState;
    DeviceAllocation<DeviceSample> candidateBestSamples;
    DeviceAllocation<cuda::finish::Refinement> finishRefinements;
    DeviceAllocation<std::uint32_t> randomStateWords;
    DeviceAllocation<CudaSearchInputEvent> candidateEvents;
    DeviceAllocation<std::uint32_t> compactInputIndices;
    DeviceAllocation<std::uint32_t> compactInputOffsets;
    DeviceAllocation<std::int32_t> candidateInputValues;
    DeviceAllocation<CudaSearchInputEvent> temporaryEvents;
    DeviceAllocation<CudaSearchInputEvent> passBaselineEvents;
    DeviceAllocation<std::uint32_t> eligibleIndices;
    DeviceAllocation<std::uint32_t> eventCounts;
    DeviceAllocation<std::uint32_t> mutationCounts;
    DeviceAllocation<DeviceCandidateStatus> statuses;
    DeviceAllocation<bool> activeCandidates;
    DeviceAllocation<DeviceSample> reducedBest;
    DeviceAllocation<std::byte> reductionTemporary;
    DeviceAllocation<cuda::collision::CudaCollision> collisionScratch;
    DeviceAllocation<cuda::collision::CudaCollision> shapeCollisionScratch;
    DeviceAllocation<GmIso4> shapeWorldScratch;
    DeviceAllocation<GmBoxAligned> movingBoundsScratch;
    DeviceAllocation<cuda::collision::CudaCollisionSurfaceHit>
            surfaceHitScratch;
    DeviceAllocation<cuda::collision::CudaCollisionMeshRange>
            meshRangeScratch;
    DeviceAllocation<std::uint32_t> meshCellScratch;
    MappedCancellation cancellation;
    DeviceAllocation<DeviceSample> globalBestSample;
    DeviceAllocation<CudaCandidateState> globalBestState;
    DeviceAllocation<CudaSearchInputEvent> globalBestInputs;
    DeviceAllocation<std::uint32_t> globalBestEventCount;
    DeviceAllocation<std::uint32_t> globalBestMutationCount;
    DeviceAllocation<DeviceBatchSummary> summary;

    void UpdateResidentBytes() {
        residentBytes = 0u;
#define ADD_BYTES(member) residentBytes += member.Bytes()
        ADD_BYTES(branchState);
        ADD_BYTES(branchControls);
        ADD_BYTES(mutableBoundaryControls);
        ADD_BYTES(baselineTicks);
        ADD_BYTES(baselineInputs);
        ADD_BYTES(modifiers);
        ADD_BYTES(smoothWeights);
        ADD_BYTES(evaluator);
        ADD_BYTES(capturedWinnerState);
        ADD_BYTES(candidateBestSamples);
        ADD_BYTES(finishRefinements);
        ADD_BYTES(randomStateWords);
        ADD_BYTES(candidateEvents);
        ADD_BYTES(compactInputIndices);
        ADD_BYTES(compactInputOffsets);
        ADD_BYTES(candidateInputValues);
        ADD_BYTES(temporaryEvents);
        ADD_BYTES(passBaselineEvents);
        ADD_BYTES(eligibleIndices);
        ADD_BYTES(eventCounts);
        ADD_BYTES(mutationCounts);
        ADD_BYTES(statuses);
        ADD_BYTES(activeCandidates);
        ADD_BYTES(reducedBest);
        ADD_BYTES(reductionTemporary);
        ADD_BYTES(collisionScratch);
        ADD_BYTES(shapeCollisionScratch);
        ADD_BYTES(shapeWorldScratch);
        ADD_BYTES(movingBoundsScratch);
        ADD_BYTES(surfaceHitScratch);
        ADD_BYTES(meshRangeScratch);
        ADD_BYTES(meshCellScratch);
        ADD_BYTES(cancellation);
        ADD_BYTES(globalBestSample);
        ADD_BYTES(globalBestState);
        ADD_BYTES(globalBestInputs);
        ADD_BYTES(globalBestEventCount);
        ADD_BYTES(globalBestMutationCount);
        ADD_BYTES(summary);
#undef ADD_BYTES
        winnerSelectionBytes =
                candidateBestSamples.Bytes() +
                reducedBest.Bytes() +
                reductionTemporary.Bytes();
    }

    template <std::uint32_t MinimumBlocksPerSm>
    const void *SimulationKernel() const {
        return configuration.branchState.stuntsEnabled
                ? reinterpret_cast<const void *>(
                          SimulateSearchCandidatesKernel<
                                  CudaCandidateState,
                                  true,
                                  MinimumBlocksPerSm>)
                : reinterpret_cast<const void *>(
                          SimulateSearchCandidatesKernel<
                                  CudaCandidatePhysicsState,
                                  false,
                                  MinimumBlocksPerSm>);
    }

    bool LoadSimulationKernelMetrics(
            const void *kernel,
            const cudaDeviceProp &properties,
            SimulationKernelMetrics *metrics,
            std::string *diagnostic) {
        cudaFuncAttributes attributes{};
        cudaError_t error =
                cudaFuncGetAttributes(&attributes, kernel);
        if (error != cudaSuccess) {
            if (diagnostic != nullptr) {
                *diagnostic = CudaFailure(
                        "querying CUDA simulation kernel attributes",
                        error);
            }
            return false;
        }
        int activeBlocks = 0;
        error = cudaOccupancyMaxActiveBlocksPerMultiprocessor(
                &activeBlocks, kernel, SimulationBlockSize, 0u);
        if (error != cudaSuccess) {
            if (diagnostic != nullptr) {
                *diagnostic = CudaFailure(
                        "querying CUDA simulation occupancy", error);
            }
            return false;
        }
        metrics->registersPerThread =
                static_cast<std::uint32_t>(attributes.numRegs);
        metrics->localBytesPerThread =
                static_cast<std::uint64_t>(
                        attributes.localSizeBytes);
        metrics->activeBlocksPerMultiprocessor =
                static_cast<std::uint32_t>(activeBlocks);
        metrics->theoreticalOccupancy =
                properties.maxThreadsPerMultiProcessor == 0
                ? 0.0
                : static_cast<double>(
                          activeBlocks * SimulationBlockSize) /
                          properties.maxThreadsPerMultiProcessor;
        if (diagnostic != nullptr) {
            diagnostic->clear();
        }
        return true;
    }

    bool LoadSimulationKernelMetrics(std::string *diagnostic) {
        int device = 0;
        cudaDeviceProp properties{};
        cudaError_t error = cudaGetDevice(&device);
        if (error == cudaSuccess) {
            error = cudaGetDeviceProperties(&properties, device);
        }
        if (error != cudaSuccess) {
            if (diagnostic != nullptr) {
                *diagnostic = CudaFailure(
                        "querying CUDA device properties", error);
            }
            return false;
        }
        multiprocessorCount =
                static_cast<std::uint32_t>(
                        properties.multiProcessorCount);
        return LoadSimulationKernelMetrics(
                       SimulationKernel<
                               LatencyKernelMinimumBlocksPerSm>(),
                       properties,
                       &latencyKernelMetrics,
                       diagnostic) &&
               LoadSimulationKernelMetrics(
                       SimulationKernel<
                               ThroughputKernelMinimumBlocksPerSm>(),
                       properties,
                       &throughputKernelMetrics,
                       diagnostic);
    }

    bool ReserveBatchCapacity(
            std::uint32_t candidateCount,
            std::string *diagnostic) {
        if (candidateCount <= configuration.maximumBatchSize) {
            if (diagnostic != nullptr) {
                diagnostic->clear();
            }
            return true;
        }
        const std::uint64_t eventSlots64 =
                static_cast<std::uint64_t>(candidateCount) *
                configuration.maximumEventCount;
        const std::uint64_t compactValueSlots64 =
                static_cast<std::uint64_t>(candidateCount) *
                compactInputCount;
        const std::uint64_t winnerSlots64 =
                1u + static_cast<std::uint64_t>(candidateCount);
        const std::uint64_t collisionSlots64 =
                static_cast<std::uint64_t>(candidateCount) *
                cuda::collision::CollisionCapacity;
        const std::uint64_t shapeCollisionSlots64 =
                static_cast<std::uint64_t>(candidateCount) *
                cuda::collision::ShapeCollisionCapacity;
        const std::uint64_t shapeQuerySlots64 =
                static_cast<std::uint64_t>(candidateCount) *
                collisionShapeCount;
        const std::uint64_t surfaceHitSlots64 =
                static_cast<std::uint64_t>(candidateCount) *
                cuda::collision::SurfaceHitCapacity;
        const std::uint64_t meshRangeSlots64 =
                surfaceHitSlots64;
        const std::uint64_t meshCellSlots64 =
                static_cast<std::uint64_t>(candidateCount) *
                cuda::collision::MeshCellHitCapacity;
        if (eventSlots64 >
                    std::numeric_limits<std::size_t>::max() ||
            compactValueSlots64 >
                    std::numeric_limits<std::size_t>::max() ||
            winnerSlots64 >
                    std::numeric_limits<std::size_t>::max() ||
            collisionSlots64 >
                    std::numeric_limits<std::size_t>::max() ||
            shapeCollisionSlots64 >
                    std::numeric_limits<std::size_t>::max() ||
            shapeQuerySlots64 >
                    std::numeric_limits<std::size_t>::max() ||
            surfaceHitSlots64 >
                    std::numeric_limits<std::size_t>::max() ||
            meshRangeSlots64 >
                    std::numeric_limits<std::size_t>::max() ||
            meshCellSlots64 >
                    std::numeric_limits<std::size_t>::max()) {
            if (diagnostic != nullptr) {
                *diagnostic =
                        "CUDA search calibration buffer dimensions overflow";
            }
            return false;
        }

        const std::size_t candidates = candidateCount;
        const std::size_t eventSlots =
                static_cast<std::size_t>(eventSlots64);
        const std::size_t compactValueSlots =
                static_cast<std::size_t>(compactValueSlots64);
        const std::size_t winnerSlots =
                static_cast<std::size_t>(winnerSlots64);
        const std::size_t collisionSlots =
                static_cast<std::size_t>(collisionSlots64);
        const std::size_t shapeCollisionSlots =
                static_cast<std::size_t>(shapeCollisionSlots64);
        const std::size_t shapeQuerySlots =
                static_cast<std::size_t>(shapeQuerySlots64);
        const std::size_t surfaceHitSlots =
                static_cast<std::size_t>(surfaceHitSlots64);
        const std::size_t meshRangeSlots =
                static_cast<std::size_t>(meshRangeSlots64);
        const std::size_t meshCellSlots =
                static_cast<std::size_t>(meshCellSlots64);
        DeviceAllocation<DeviceSample> nextCandidateBestSamples;
        DeviceAllocation<cuda::finish::Refinement>
                nextFinishRefinements;
        DeviceAllocation<std::uint32_t> nextRandomStateWords;
        DeviceAllocation<CudaSearchInputEvent> nextCandidateEvents;
        DeviceAllocation<std::int32_t> nextCandidateInputValues;
        DeviceAllocation<CudaSearchInputEvent> nextTemporaryEvents;
        DeviceAllocation<CudaSearchInputEvent> nextPassBaselineEvents;
        DeviceAllocation<std::uint32_t> nextEligibleIndices;
        DeviceAllocation<std::uint32_t> nextEventCounts;
        DeviceAllocation<std::uint32_t> nextMutationCounts;
        DeviceAllocation<DeviceCandidateStatus> nextStatuses;
        DeviceAllocation<bool> nextActiveCandidates;
        DeviceAllocation<std::byte> nextReductionTemporary;
        DeviceAllocation<cuda::collision::CudaCollision>
                nextCollisionScratch;
        DeviceAllocation<cuda::collision::CudaCollision>
                nextShapeCollisionScratch;
        DeviceAllocation<GmIso4> nextShapeWorldScratch;
        DeviceAllocation<GmBoxAligned> nextMovingBoundsScratch;
        DeviceAllocation<cuda::collision::CudaCollisionSurfaceHit>
                nextSurfaceHitScratch;
        DeviceAllocation<cuda::collision::CudaCollisionMeshRange>
                nextMeshRangeScratch;
        DeviceAllocation<std::uint32_t> nextMeshCellScratch;
        if (!nextCandidateBestSamples.Allocate(winnerSlots) ||
            !nextFinishRefinements.Allocate(
                    configuration.evaluator.kind ==
                                    CudaSearchEvaluatorKind::FinishTime
                            ? candidates
                            : 0u) ||
            !nextRandomStateWords.Allocate(candidates * 624u) ||
            !nextCandidateEvents.Allocate(
                    compactRandomSteeringPipeline ? 0u : eventSlots) ||
            !nextCandidateInputValues.Allocate(
                    compactRandomSteeringPipeline
                            ? compactValueSlots
                            : 0u) ||
            !nextTemporaryEvents.Allocate(
                    needsTemporaryEvents ? eventSlots : 0u) ||
            !nextPassBaselineEvents.Allocate(
                    needsPassBaselineEvents ? eventSlots : 0u) ||
            !nextEligibleIndices.Allocate(
                    needsEligibleIndices ? eventSlots : 0u) ||
            !nextEventCounts.Allocate(candidates) ||
            !nextMutationCounts.Allocate(candidates) ||
            !nextStatuses.Allocate(candidates) ||
            !nextActiveCandidates.Allocate(candidates) ||
            !nextCollisionScratch.Allocate(collisionSlots) ||
            !nextShapeCollisionScratch.Allocate(
                    shapeCollisionSlots) ||
            !nextShapeWorldScratch.Allocate(shapeQuerySlots) ||
            !nextMovingBoundsScratch.Allocate(shapeQuerySlots) ||
            !nextSurfaceHitScratch.Allocate(surfaceHitSlots) ||
            !nextMeshRangeScratch.Allocate(meshRangeSlots) ||
            !nextMeshCellScratch.Allocate(meshCellSlots)) {
            static_cast<void>(cudaGetLastError());
            if (diagnostic != nullptr) {
                *diagnostic =
                        "CUDA calibration could not reserve a larger real batch";
            }
            return false;
        }

        std::size_t reductionBytes = 0u;
        const cudaError_t error = cub::DeviceReduce::Reduce(
                nullptr, reductionBytes,
                nextCandidateBestSamples.Get(), reducedBest.Get(),
                winnerSlots,
                BetterSample{
                        configuration.evaluator.kind ==
                                CudaSearchEvaluatorKind::Velocity},
                DeviceSample{});
        if (error != cudaSuccess ||
            !nextReductionTemporary.Allocate(reductionBytes)) {
            static_cast<void>(cudaGetLastError());
            if (diagnostic != nullptr) {
                *diagnostic = error != cudaSuccess
                        ? CudaFailure(
                                  "sizing calibrated CUDA winner reduction",
                                  error)
                        : "CUDA calibration winner reduction allocation failed";
            }
            return false;
        }

        candidateBestSamples = std::move(nextCandidateBestSamples);
        finishRefinements = std::move(nextFinishRefinements);
        randomStateWords = std::move(nextRandomStateWords);
        candidateEvents = std::move(nextCandidateEvents);
        candidateInputValues =
                std::move(nextCandidateInputValues);
        temporaryEvents = std::move(nextTemporaryEvents);
        passBaselineEvents = std::move(nextPassBaselineEvents);
        eligibleIndices = std::move(nextEligibleIndices);
        eventCounts = std::move(nextEventCounts);
        mutationCounts = std::move(nextMutationCounts);
        statuses = std::move(nextStatuses);
        activeCandidates = std::move(nextActiveCandidates);
        reductionTemporary = std::move(nextReductionTemporary);
        collisionScratch = std::move(nextCollisionScratch);
        shapeCollisionScratch =
                std::move(nextShapeCollisionScratch);
        shapeWorldScratch = std::move(nextShapeWorldScratch);
        movingBoundsScratch =
                std::move(nextMovingBoundsScratch);
        surfaceHitScratch = std::move(nextSurfaceHitScratch);
        meshRangeScratch = std::move(nextMeshRangeScratch);
        meshCellScratch = std::move(nextMeshCellScratch);
        configuration.maximumBatchSize = candidateCount;
        UpdateResidentBytes();
        if (diagnostic != nullptr) {
            diagnostic->clear();
        }
        return true;
    }

    CudaSearchBatchExecution Execute(
            std::uint64_t firstCandidateId,
            std::uint32_t candidateCount,
            bool baseline,
            const std::function<bool()> &cancellationRequested) noexcept {
        CudaSearchBatchExecution result;
        result.firstCandidateId = firstCandidateId;
        result.candidateCount = candidateCount;
        result.residentDeviceBytes = residentBytes;
        result.candidateInputDeviceBytes =
                candidateEvents.Bytes() +
                candidateInputValues.Bytes();
        result.mutationScratchDeviceBytes =
                randomStateWords.Bytes() +
                temporaryEvents.Bytes() +
                passBaselineEvents.Bytes() +
                eligibleIndices.Bytes();
        result.mutationDeviceBytes =
                baselineInputs.Bytes() +
                modifiers.Bytes() +
                smoothWeights.Bytes() +
                candidateEvents.Bytes() +
                compactInputIndices.Bytes() +
                compactInputOffsets.Bytes() +
                candidateInputValues.Bytes() +
                result.mutationScratchDeviceBytes +
                eventCounts.Bytes() +
                mutationCounts.Bytes() +
                statuses.Bytes() +
                activeCandidates.Bytes();
        result.winnerSelectionDeviceBytes = winnerSelectionBytes;
        if ((!baseline && !baselineEvaluated) ||
            candidateCount == 0u ||
            candidateCount > configuration.maximumBatchSize) {
            result.status = CudaSearchStatus::InvalidArgument;
            result.diagnostic = !baselineEvaluated && !baseline
                    ? "CUDA baseline must be evaluated before mutation batches"
                    : "invalid CUDA search batch size";
            return result;
        }
        const std::size_t winnerCount =
                static_cast<std::size_t>(candidateCount) + 1u;
        bool cancelled = false;
        if (cancellationRequested) {
            try {
                cancelled = cancellationRequested();
            } catch (...) {
                cancelled = true;
            }
        }
        *cancellation.Host() = cancelled ? 1u : 0u;
        std::atomic_thread_fence(std::memory_order_seq_cst);
        cudaError_t error = cudaSuccess;

        Event started;
        Event winnerInitialized;
        Event mutationsGenerated;
        Event simulationFinished;
        Event finishRefined;
        Event winnerReduced;
        Event winnerStateCaptured;
        Event finished;
        if (!started.Valid() || !winnerInitialized.Valid() ||
            !mutationsGenerated.Valid() ||
            !simulationFinished.Valid() ||
            !finishRefined.Valid() ||
            !winnerReduced.Valid() ||
            !winnerStateCaptured.Valid() || !finished.Valid()) {
            result.status = CudaSearchStatus::DeviceFailure;
            result.diagnostic = "CUDA search event creation failed";
            return result;
        }
        cudaEventRecord(started.Get());
        constexpr std::uint32_t blockSize = 128u;
        SeedCandidateBestSamplesKernel<<<1u, 1u>>>(
                candidateBestSamples.Get(), globalBestSample.Get());
        cudaEventRecord(winnerInitialized.Get());
        const std::uint32_t candidateBlocks =
                (candidateCount - 1u) / blockSize + 1u;
        GenerateSearchCandidatesKernel<<<candidateBlocks, blockSize>>>(
                baselineInputs.Get(),
                static_cast<std::uint32_t>(
                        configuration.baselineInputs.size()),
                modifiers.Get(),
                static_cast<std::uint32_t>(
                        configuration.modifiers.size()),
                smoothWeights.Get(),
                mutableBoundaryControls.Get(),
                configuration.tickDurationMs,
                firstCandidateId,
                candidateCount,
                baseline,
                configuration.useLegacyMutationPipelineForTesting,
                baselineInputsCanonical,
                compactRandomSteeringPipeline,
                static_cast<std::uint32_t>(
                        configuration.maximumEventCount),
                compactInputIndices.Get(),
                compactInputCount,
                candidateInputValues.Get(),
                candidateBestSamples.Get(),
                randomStateWords.Get(),
                candidateEvents.Get(),
                temporaryEvents.Get(),
                passBaselineEvents.Get(),
                eligibleIndices.Get(),
                eventCounts.Get(),
                mutationCounts.Get(),
                statuses.Get(),
                activeCandidates.Get(),
                cancellation.Get());
        cudaEventRecord(mutationsGenerated.Get());
        const std::uint32_t simulationBlocks =
                (candidateCount - 1u) / SimulationBlockSize + 1u;
        // Pay the throughput kernel's spill cost only when the latency
        // kernel would need another full resident wave.
        const bool useThroughputKernel =
                static_cast<std::uint64_t>(simulationBlocks) >
                static_cast<std::uint64_t>(
                        latencyKernelMetrics.
                                activeBlocksPerMultiprocessor) *
                        multiprocessorCount;
        const SimulationKernelMetrics &simulationMetrics =
                useThroughputKernel
                ? throughputKernelMetrics
                : latencyKernelMetrics;
        const auto launchSimulation = [&](auto stateType,
                                          auto simulateStunts) {
            using State = decltype(stateType);
            constexpr bool SimulateStunts =
                    decltype(simulateStunts)::value;
            const auto launch = [&](auto minimumBlocks) {
                constexpr std::uint32_t MinimumBlocksPerSm =
                        decltype(minimumBlocks)::value;
                SimulateSearchCandidatesKernel<
                        State,
                        SimulateStunts,
                        MinimumBlocksPerSm>
                        <<<simulationBlocks, SimulationBlockSize>>>(
                        configuration.deviceScene,
                        configuration.deviceStaticConfiguration,
                        branchState.Get(),
                        mutableBoundaryControls.Get(),
                        baselineTicks.Get(),
                        timelineTickCount,
                        evaluator.Get(),
                        configuration.tickDurationMs,
                        configuration.prestartDurationMs,
                        configuration.branchTimeMs,
                        mutableFromTimeMs,
                        configuration.evaluationStartTimeMs,
                        evaluationTickCount,
                        firstCandidateId,
                        candidateCount,
                        baseline,
                        static_cast<std::uint32_t>(
                                configuration.maximumEventCount),
                        candidateBestSamples.Get(),
                        baselineInputs.Get(),
                        candidateEvents.Get(),
                        candidateInputValues.Get(),
                        compactInputOffsets.Get(),
                        compactInputCount,
                        compactRandomSteeringPipeline,
                        eventCounts.Get(),
                        statuses.Get(),
                        activeCandidates.Get(),
                        collisionScratch.Get(),
                        shapeCollisionScratch.Get(),
                        shapeWorldScratch.Get(),
                        movingBoundsScratch.Get(),
                        surfaceHitScratch.Get(),
                        meshRangeScratch.Get(),
                        meshCellScratch.Get(),
                        configuration.maximumBatchSize,
                        collisionShapeCount,
                        cancellation.Get());
            };
            if (useThroughputKernel) {
                launch(std::integral_constant<
                       std::uint32_t,
                       ThroughputKernelMinimumBlocksPerSm>{});
            } else {
                launch(std::integral_constant<
                       std::uint32_t,
                       LatencyKernelMinimumBlocksPerSm>{});
            }
        };
        if (configuration.branchState.stuntsEnabled) {
            launchSimulation(
                    CudaCandidateState{},
                    std::true_type{});
        } else {
            launchSimulation(
                    CudaCandidatePhysicsState{},
                    std::false_type{});
        }
        cudaEventRecord(simulationFinished.Get());
        if (configuration.evaluator.kind ==
            CudaSearchEvaluatorKind::FinishTime) {
            const auto launchFinishRefinement =
                    [&](auto stateType, auto simulateStunts) {
                using State = decltype(stateType);
                constexpr bool SimulateStunts =
                        decltype(simulateStunts)::value;
                RefineSearchFinishTimesKernel<State, SimulateStunts>
                        <<<simulationBlocks, SimulationBlockSize>>>(
                        configuration.deviceScene,
                        configuration.deviceStaticConfiguration,
                        branchState.Get(),
                        mutableBoundaryControls.Get(),
                        baselineTicks.Get(),
                        timelineTickCount,
                        configuration.tickDurationMs,
                        configuration.prestartDurationMs,
                        configuration.branchTimeMs,
                        mutableFromTimeMs,
                        static_cast<std::uint32_t>(
                                configuration.maximumEventCount),
                        candidateBestSamples.Get(),
                        finishRefinements.Get(),
                        baselineInputs.Get(),
                        candidateEvents.Get(),
                        candidateInputValues.Get(),
                        compactInputOffsets.Get(),
                        compactInputCount,
                        compactRandomSteeringPipeline,
                        eventCounts.Get(),
                        statuses.Get(),
                        activeCandidates.Get(),
                        collisionScratch.Get(),
                        shapeCollisionScratch.Get(),
                        shapeWorldScratch.Get(),
                        movingBoundsScratch.Get(),
                        surfaceHitScratch.Get(),
                        meshRangeScratch.Get(),
                        meshCellScratch.Get(),
                        configuration.maximumBatchSize,
                        collisionShapeCount,
                        cancellation.Get(),
                        candidateCount);
            };
            if (configuration.branchState.stuntsEnabled) {
                launchFinishRefinement(
                        CudaCandidateState{}, std::true_type{});
            } else {
                launchFinishRefinement(
                        CudaCandidatePhysicsState{}, std::false_type{});
            }
        }
        cudaEventRecord(finishRefined.Get());
        std::size_t temporaryBytes = reductionTemporary.Bytes();
        error = cub::DeviceReduce::Reduce(
                reductionTemporary.Get(), temporaryBytes,
                candidateBestSamples.Get(), reducedBest.Get(),
                winnerCount,
                BetterSample{
                        configuration.evaluator.kind ==
                                CudaSearchEvaluatorKind::Velocity},
                DeviceSample{});
        if (error != cudaSuccess) {
            result.status = CudaSearchStatus::DeviceFailure;
            result.diagnostic =
                    CudaFailure("launching CUDA winner reduction", error);
            return result;
        }
        cudaEventRecord(winnerReduced.Get());
        CaptureSearchWinnerStateKernel<<<1u, 1u>>>(
                configuration.deviceScene,
                configuration.deviceStaticConfiguration,
                branchState.Get(),
                mutableBoundaryControls.Get(),
                baselineTicks.Get(),
                reducedBest.Get(),
                configuration.evaluator.kind ==
                                CudaSearchEvaluatorKind::FinishTime
                        ? finishRefinements.Get()
                        : nullptr,
                configuration.tickDurationMs,
                configuration.prestartDurationMs,
                configuration.branchTimeMs,
                mutableFromTimeMs,
                configuration.evaluationStartTimeMs,
                static_cast<std::uint32_t>(
                        configuration.maximumEventCount),
                baselineInputs.Get(),
                candidateEvents.Get(),
                candidateInputValues.Get(),
                compactInputOffsets.Get(),
                compactInputCount,
                compactRandomSteeringPipeline,
                candidateCount,
                eventCounts.Get(),
                statuses.Get(),
                collisionScratch.Get(),
                shapeCollisionScratch.Get(),
                shapeWorldScratch.Get(),
                movingBoundsScratch.Get(),
                surfaceHitScratch.Get(),
                meshRangeScratch.Get(),
                meshCellScratch.Get(),
                configuration.maximumBatchSize,
                collisionShapeCount,
                capturedWinnerState.Get());
        cudaEventRecord(winnerStateCaptured.Get());
        FinalizeSearchBatchKernel<<<1u, 1u>>>(
                reducedBest.Get(),
                capturedWinnerState.Get(),
                candidateBestSamples.Get(),
                baselineInputs.Get(),
                candidateEvents.Get(),
                candidateInputValues.Get(),
                compactInputOffsets.Get(),
                compactInputCount,
                compactRandomSteeringPipeline,
                eventCounts.Get(),
                mutationCounts.Get(),
                statuses.Get(),
                activeCandidates.Get(),
                candidateCount,
                static_cast<std::uint32_t>(
                        configuration.maximumEventCount),
                evaluationTickCount,
                configuration.evaluator.kind ==
                        CudaSearchEvaluatorKind::Velocity,
                baseline,
                globalBestSample.Get(),
                globalBestState.Get(),
                globalBestInputs.Get(),
                globalBestEventCount.Get(),
                globalBestMutationCount.Get(),
                summary.Get());
        error = cudaGetLastError();
        if (error != cudaSuccess) {
            result.status = CudaSearchStatus::DeviceFailure;
            result.diagnostic =
                    CudaFailure("launching CUDA search kernels", error);
            return result;
        }
        cudaEventRecord(finished.Get());
        while ((error = cudaEventQuery(finished.Get())) ==
               cudaErrorNotReady) {
            if (!cancelled && cancellationRequested) {
                try {
                    cancelled = cancellationRequested();
                } catch (...) {
                    cancelled = true;
                }
                if (cancelled) {
                    *cancellation.Host() = 1u;
                    std::atomic_thread_fence(
                            std::memory_order_seq_cst);
                }
            }
            std::this_thread::sleep_for(
                    std::chrono::milliseconds(1));
        }
        if (error != cudaSuccess) {
            result.status = CudaSearchStatus::DeviceFailure;
            result.diagnostic =
                    CudaFailure("synchronizing CUDA search batch", error);
            return result;
        }
        float milliseconds = 0.0f;
        cudaEventElapsedTime(&milliseconds, started.Get(), finished.Get());
        result.kernelMilliseconds = milliseconds;
        cudaEventElapsedTime(
                &milliseconds, started.Get(), winnerInitialized.Get());
        result.scoreInitializationKernelMilliseconds = milliseconds;
        cudaEventElapsedTime(
                &milliseconds,
                winnerInitialized.Get(),
                mutationsGenerated.Get());
        result.mutationKernelMilliseconds = milliseconds;
        cudaEventElapsedTime(
                &milliseconds,
                mutationsGenerated.Get(),
                simulationFinished.Get());
        result.simulationKernelMilliseconds = milliseconds;
        cudaEventElapsedTime(
                &milliseconds,
                simulationFinished.Get(),
                finishRefined.Get());
        result.finishRefinementKernelMilliseconds = milliseconds;
        cudaEventElapsedTime(
                &milliseconds,
                finishRefined.Get(),
                finished.Get());
        result.winnerKernelMilliseconds = milliseconds;
        cudaEventElapsedTime(
                &milliseconds,
                finishRefined.Get(),
                winnerReduced.Get());
        result.winnerReductionKernelMilliseconds = milliseconds;
        cudaEventElapsedTime(
                &milliseconds,
                winnerReduced.Get(),
                winnerStateCaptured.Get());
        result.winnerStateCaptureKernelMilliseconds = milliseconds;
        cudaEventElapsedTime(
                &milliseconds,
                winnerStateCaptured.Get(),
                finished.Get());
        result.finalizationKernelMilliseconds = milliseconds;
        result.simulationThreadsPerBlock = SimulationBlockSize;
        result.simulationRegistersPerThread =
                simulationMetrics.registersPerThread;
        result.simulationLocalBytesPerThread =
                simulationMetrics.localBytesPerThread;
        result.simulationActiveBlocksPerMultiprocessor =
                simulationMetrics.activeBlocksPerMultiprocessor;
        result.simulationTheoreticalOccupancy =
                simulationMetrics.theoreticalOccupancy;

        DeviceBatchSummary hostSummary;
        error = cudaMemcpy(
                &hostSummary, summary.Get(), sizeof(hostSummary),
                cudaMemcpyDeviceToHost);
        if (error != cudaSuccess) {
            result.status = CudaSearchStatus::DeviceFailure;
            result.diagnostic =
                    CudaFailure("copying CUDA search summary", error);
            return result;
        }
        result.deviceToHostBytes += sizeof(hostSummary);
        result.status = hostSummary.status;
        result.evaluatedCandidateCount =
                hostSummary.evaluatedCandidateCount;
        result.evaluatorCalls = hostSummary.evaluatorCalls;
        result.totalMutationCount =
                hostSummary.totalMutationCount;
        result.mutationImprovementCount =
                baseline ? 0u
                         : hostSummary.mutationImprovementCount;
        result.bestChanged = hostSummary.bestChanged;
        if (hostSummary.bestValid) {
            result.best.valid = true;
            result.best.mutation = hostSummary.bestMutation;
            result.best.candidateId = hostSummary.bestCandidateId;
            result.best.mutationCount =
                    hostSummary.bestMutationCount;
            DeviceSample bestSample;
            error = cudaMemcpy(
                    &bestSample, globalBestSample.Get(),
                    sizeof(bestSample), cudaMemcpyDeviceToHost);
            if (error == cudaSuccess) {
                error = cudaMemcpy(
                        &result.best.state, globalBestState.Get(),
                        sizeof(result.best.state),
                        cudaMemcpyDeviceToHost);
            }
            if (error != cudaSuccess) {
                result.status = CudaSearchStatus::DeviceFailure;
                result.diagnostic =
                        CudaFailure("copying CUDA winning state", error);
                return result;
            }
            result.best.score = bestSample.score;
            result.best.evaluationTick = bestSample.evaluationTick;
            result.best.timeMs = bestSample.timeMs;
            result.best.detail0 = bestSample.detail0;
            result.best.detail1 = bestSample.detail1;
            result.best.inputs = immutableInputPrefix;
            const std::size_t suffixOffset =
                    result.best.inputs.size();
            result.best.inputs.resize(
                    suffixOffset + hostSummary.globalEventCount);
            if (hostSummary.globalEventCount != 0u) {
                error = cudaMemcpy(
                        result.best.inputs.data() + suffixOffset,
                        globalBestInputs.Get(),
                        hostSummary.globalEventCount *
                                sizeof(CudaSearchInputEvent),
                        cudaMemcpyDeviceToHost);
                if (error != cudaSuccess) {
                    result.status = CudaSearchStatus::DeviceFailure;
                    result.diagnostic =
                            CudaFailure(
                                    "copying CUDA winning inputs", error);
                    return result;
                }
                for (std::size_t index = suffixOffset;
                     index < result.best.inputs.size(); ++index) {
                    result.best.inputs[index] =
                            cuda_search_detail::AbsoluteSuffixEvent(
                                    result.best.inputs[index],
                                    mutableFromTimeMs);
                }
            }
            result.deviceToHostBytes += sizeof(bestSample) +
                    sizeof(result.best.state) +
                    hostSummary.globalEventCount *
                            sizeof(CudaSearchInputEvent);
        }
        if (baseline && result.status == CudaSearchStatus::Success) {
            baselineEvaluated = true;
        }
        if (result.status != CudaSearchStatus::Success &&
            result.diagnostic.empty()) {
            result.diagnostic =
                    std::string("CUDA search batch status: ") +
                    CudaSearchStatusName(result.status);
        }
        return result;
    }
};

CudaSearchExecutor::CudaSearchExecutor(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}
CudaSearchExecutor::~CudaSearchExecutor() = default;
CudaSearchExecutor::CudaSearchExecutor(CudaSearchExecutor &&) noexcept =
        default;
CudaSearchExecutor &CudaSearchExecutor::operator=(
        CudaSearchExecutor &&) noexcept = default;

std::unique_ptr<CudaSearchExecutor> CudaSearchExecutor::Create(
        const CudaSearchExecutorConfiguration &configuration,
        std::string *diagnostic) noexcept {
    try {
        if (configuration.deviceScene == nullptr ||
            configuration.deviceStaticConfiguration == nullptr ||
            configuration.maximumBatchSize == 0u ||
            configuration.tickDurationMs == 0u ||
            configuration.maximumEventCount <
                    configuration.baselineInputs.size() ||
            configuration.maximumEventCount > UINT32_MAX ||
            configuration.baselineTicks.empty() ||
            configuration.modifiers.empty() ||
            configuration.branchTimeMs < 0 ||
            configuration.branchTimeMs >
                    INT32_MAX - configuration.tickDurationMs ||
            configuration.evaluationStartTimeMs <
                    configuration.branchTimeMs +
                            configuration.tickDurationMs ||
            configuration.evaluationEndTimeMs <
                    configuration.evaluationStartTimeMs) {
            if (diagnostic != nullptr) {
                *diagnostic = "invalid CUDA search executor configuration";
            }
            return {};
        }
        const std::uint64_t evaluationTicks =
                static_cast<std::uint64_t>(
                        configuration.evaluationEndTimeMs -
                        configuration.evaluationStartTimeMs) /
                        configuration.tickDurationMs +
                1u;
        if (evaluationTicks == 0u ||
            evaluationTicks > UINT32_MAX) {
            if (diagnostic != nullptr) {
                *diagnostic = "CUDA evaluation timeline is too large";
            }
            return {};
        }
        CudaPackedStaticConfigurationHeader packedConfiguration{};
        const cudaError_t configurationCopyError = cudaMemcpy(
                &packedConfiguration,
                configuration.deviceStaticConfiguration,
                sizeof(packedConfiguration),
                cudaMemcpyDeviceToHost);
        if (configurationCopyError != cudaSuccess ||
            packedConfiguration.magic !=
                    CudaPackedStaticConfigurationHeader::Magic ||
            packedConfiguration.schemaVersion !=
                    CudaPackedStaticConfigurationHeader::
                            SchemaVersion) {
            if (diagnostic != nullptr) {
                *diagnostic = configurationCopyError != cudaSuccess
                        ? CudaFailure(
                                  "reading CUDA static configuration",
                                  configurationCopyError)
                        : "invalid CUDA static configuration header";
            }
            return {};
        }
        const std::int64_t mutableFromTimeMs =
                configuration.branchTimeMs +
                configuration.tickDurationMs;
        cuda_search_detail::SearchInputPartition inputPartition;
        if (!cuda_search_detail::PartitionSearchInputs(
                    configuration.baselineInputs,
                    configuration.branchTimeMs,
                    mutableFromTimeMs,
                    &inputPartition)) {
            if (diagnostic != nullptr) {
                *diagnostic =
                        "CUDA search inputs cannot be partitioned at the mutable boundary";
            }
            return {};
        }
        CudaSearchExecutorConfiguration preparedConfiguration =
                configuration;
        preparedConfiguration.baselineInputs =
                inputPartition.mutableSuffix;
        preparedConfiguration.maximumEventCount -=
                inputPartition.immutablePrefix.size();
        for (CudaSearchModifierConfiguration &modifier :
             preparedConfiguration.modifiers) {
            if (modifier.window.maximumTimeMs < mutableFromTimeMs) {
                if (diagnostic != nullptr) {
                    *diagnostic =
                            "CUDA modifier window does not intersect the mutable suffix";
                }
                return {};
            }
            modifier.window.minimumTimeMs =
                    std::max(
                            modifier.window.minimumTimeMs,
                            mutableFromTimeMs) -
                    mutableFromTimeMs;
            modifier.window.maximumTimeMs -= mutableFromTimeMs;
        }
        const std::uint64_t candidateEvents =
                static_cast<std::uint64_t>(
                        preparedConfiguration.maximumBatchSize) *
                preparedConfiguration.maximumEventCount;
        const std::uint64_t winnerSampleCount =
                1u + static_cast<std::uint64_t>(
                             preparedConfiguration.maximumBatchSize);
        const std::uint64_t collisionCount =
                static_cast<std::uint64_t>(
                        preparedConfiguration.maximumBatchSize) *
                cuda::collision::CollisionCapacity;
        const std::uint64_t shapeCollisionCount =
                static_cast<std::uint64_t>(
                        preparedConfiguration.maximumBatchSize) *
                cuda::collision::ShapeCollisionCapacity;
        const std::uint64_t shapeQueryCount =
                static_cast<std::uint64_t>(
                        preparedConfiguration.maximumBatchSize) *
                packedConfiguration.collisionShapes.count;
        const std::uint64_t surfaceHitCount =
                static_cast<std::uint64_t>(
                        preparedConfiguration.maximumBatchSize) *
                cuda::collision::SurfaceHitCapacity;
        const std::uint64_t meshRangeCount =
                surfaceHitCount;
        const std::uint64_t meshCellCount =
                static_cast<std::uint64_t>(
                        preparedConfiguration.maximumBatchSize) *
                cuda::collision::MeshCellHitCapacity;
        if (candidateEvents >
                    std::numeric_limits<std::size_t>::max() ||
            winnerSampleCount >
                    std::numeric_limits<std::size_t>::max() ||
            collisionCount >
                    std::numeric_limits<std::size_t>::max() ||
            shapeCollisionCount >
                    std::numeric_limits<std::size_t>::max() ||
            shapeQueryCount >
                    std::numeric_limits<std::size_t>::max() ||
            surfaceHitCount >
                    std::numeric_limits<std::size_t>::max() ||
            meshRangeCount >
                    std::numeric_limits<std::size_t>::max() ||
            meshCellCount >
                    std::numeric_limits<std::size_t>::max()) {
            if (diagnostic != nullptr) {
                *diagnostic = "CUDA search buffer dimensions overflow";
            }
            return {};
        }

        auto impl = std::make_unique<Impl>();
        impl->configuration = preparedConfiguration;
        impl->immutableInputPrefix =
                std::move(inputPartition.immutablePrefix);
        impl->mutableFromTimeMs = mutableFromTimeMs;
        impl->timelineTickCount = static_cast<std::uint32_t>(
                preparedConfiguration.baselineTicks.size());
        impl->evaluationTickCount =
                static_cast<std::uint32_t>(evaluationTicks);
        impl->collisionShapeCount =
                packedConfiguration.collisionShapes.count;
        impl->baselineInputsCanonical = CanonicalBaselineInputs(
                preparedConfiguration.baselineInputs,
                0);
        impl->compactRandomSteeringPipeline =
                !preparedConfiguration.useLegacyMutationPipelineForTesting &&
                impl->baselineInputsCanonical &&
                std::all_of(
                        preparedConfiguration.modifiers.begin(),
                        preparedConfiguration.modifiers.end(),
                        [&](const CudaSearchModifierConfiguration &modifier) {
                            return modifier.kind ==
                                           CudaSearchModifierKind::
                                                   RandomSteering &&
                                    modifier.window.minimumTimeMs >=
                                            0;
                        });
        std::vector<std::uint32_t> compactInputIndices;
        std::vector<std::uint32_t> compactInputOffsets;
        if (impl->compactRandomSteeringPipeline) {
            compactInputIndices.reserve(
                    preparedConfiguration.baselineInputs.size());
            compactInputOffsets.assign(
                    preparedConfiguration.baselineInputs.size(), UINT32_MAX);
            for (std::size_t inputIndex = 0u;
                 inputIndex < preparedConfiguration.baselineInputs.size();
                 ++inputIndex) {
                const CudaSearchInputEvent &input =
                        preparedConfiguration.baselineInputs[inputIndex];
                const bool selected =
                        input.action == 4u && input.valueKind == 2u &&
                        std::any_of(
                                preparedConfiguration.modifiers.begin(),
                                preparedConfiguration.modifiers.end(),
                                [&](const CudaSearchModifierConfiguration
                                            &modifier) {
                                    return input.timeMs >=
                                                    modifier.window.
                                                            minimumTimeMs &&
                                            input.timeMs <=
                                                    modifier.window.
                                                            maximumTimeMs;
                                });
                if (!selected) {
                    continue;
                }
                compactInputOffsets[inputIndex] =
                        static_cast<std::uint32_t>(
                                compactInputIndices.size());
                compactInputIndices.push_back(
                        static_cast<std::uint32_t>(inputIndex));
            }
        }
        impl->compactInputCount =
                static_cast<std::uint32_t>(
                        compactInputIndices.size());
        impl->needsTemporaryEvents =
                !impl->compactRandomSteeringPipeline;
        impl->needsPassBaselineEvents =
                preparedConfiguration.useLegacyMutationPipelineForTesting ||
                !impl->baselineInputsCanonical ||
                std::any_of(
                        preparedConfiguration.modifiers.begin(),
                        preparedConfiguration.modifiers.end(),
                        [&](const CudaSearchModifierConfiguration &modifier) {
                            const bool heldInsertion =
                                    modifier.kind ==
                                            CudaSearchModifierKind::
                                                    InputInsertion &&
                                    ((modifier.steering.enabled != 0u &&
                                      modifier.steering.maximumHoldMs > 0) ||
                                     (modifier.accelerate.enabled != 0u &&
                                      modifier.accelerate.maximumHoldMs > 0) ||
                                     (modifier.brake.enabled != 0u &&
                                      modifier.brake.maximumHoldMs > 0));
                            return heldInsertion ||
                                    modifier.window.minimumTimeMs <
                                            0;
                        });
        impl->needsEligibleIndices =
                preparedConfiguration.useLegacyMutationPipelineForTesting ||
                std::any_of(
                        preparedConfiguration.modifiers.begin(),
                        preparedConfiguration.modifiers.end(),
                        [](const CudaSearchModifierConfiguration &modifier) {
                            return modifier.kind ==
                                            CudaSearchModifierKind::
                                                    ExistingEvent ||
                                    modifier.kind ==
                                            CudaSearchModifierKind::
                                                    InputDeletion;
                        });
        const std::size_t candidates =
                preparedConfiguration.maximumBatchSize;
        const std::size_t eventSlots =
                static_cast<std::size_t>(candidateEvents);
        const std::size_t winnerSampleSlots =
                static_cast<std::size_t>(winnerSampleCount);
        const std::size_t collisionSlots =
                static_cast<std::size_t>(collisionCount);
        const std::size_t shapeCollisionSlots =
                static_cast<std::size_t>(shapeCollisionCount);
        const std::size_t shapeQuerySlots =
                static_cast<std::size_t>(shapeQueryCount);
        const std::size_t surfaceHitSlots =
                static_cast<std::size_t>(surfaceHitCount);
        const std::size_t meshRangeSlots =
                static_cast<std::size_t>(meshRangeCount);
        const std::size_t meshCellSlots =
                static_cast<std::size_t>(meshCellCount);
        const std::size_t compactValueSlots =
                impl->compactRandomSteeringPipeline
                ? candidates * impl->compactInputCount
                : 0u;
        if (!impl->branchState.Allocate(1u) ||
            !impl->branchControls.Allocate(1u) ||
            !impl->mutableBoundaryControls.Allocate(1u) ||
            !impl->baselineTicks.Allocate(
                    preparedConfiguration.baselineTicks.size()) ||
            !impl->baselineInputs.Allocate(
                    preparedConfiguration.baselineInputs.size()) ||
            !impl->modifiers.Allocate(
                    preparedConfiguration.modifiers.size()) ||
            !impl->smoothWeights.Allocate(
                    preparedConfiguration.smoothWeights.size()) ||
            !impl->evaluator.Allocate(1u) ||
            !impl->capturedWinnerState.Allocate(1u) ||
            !impl->candidateBestSamples.Allocate(winnerSampleSlots) ||
            !impl->finishRefinements.Allocate(
                    preparedConfiguration.evaluator.kind ==
                                    CudaSearchEvaluatorKind::FinishTime
                            ? candidates
                            : 0u) ||
            !impl->randomStateWords.Allocate(candidates * 624u) ||
            !impl->candidateEvents.Allocate(
                    impl->compactRandomSteeringPipeline
                            ? 0u
                            : eventSlots) ||
            !impl->compactInputIndices.Allocate(
                    compactInputIndices.size()) ||
            !impl->compactInputOffsets.Allocate(
                    impl->compactRandomSteeringPipeline
                            ? compactInputOffsets.size()
                            : 0u) ||
            !impl->candidateInputValues.Allocate(
                    compactValueSlots) ||
            !impl->temporaryEvents.Allocate(
                    impl->needsTemporaryEvents ? eventSlots : 0u) ||
            !impl->passBaselineEvents.Allocate(
                    impl->needsPassBaselineEvents ? eventSlots : 0u) ||
            !impl->eligibleIndices.Allocate(
                    impl->needsEligibleIndices ? eventSlots : 0u) ||
            !impl->eventCounts.Allocate(candidates) ||
            !impl->mutationCounts.Allocate(candidates) ||
            !impl->statuses.Allocate(candidates) ||
            !impl->activeCandidates.Allocate(candidates) ||
            !impl->reducedBest.Allocate(1u) ||
            !impl->collisionScratch.Allocate(collisionSlots) ||
            !impl->shapeCollisionScratch.Allocate(
                    shapeCollisionSlots) ||
            !impl->shapeWorldScratch.Allocate(shapeQuerySlots) ||
            !impl->movingBoundsScratch.Allocate(shapeQuerySlots) ||
            !impl->surfaceHitScratch.Allocate(surfaceHitSlots) ||
            !impl->meshRangeScratch.Allocate(meshRangeSlots) ||
            !impl->meshCellScratch.Allocate(meshCellSlots) ||
            !impl->cancellation.Allocate() ||
            !impl->globalBestSample.Allocate(1u) ||
            !impl->globalBestState.Allocate(1u) ||
            !impl->globalBestInputs.Allocate(
                    preparedConfiguration.maximumEventCount) ||
            !impl->globalBestEventCount.Allocate(1u) ||
            !impl->globalBestMutationCount.Allocate(1u) ||
            !impl->summary.Allocate(1u)) {
            if (diagnostic != nullptr) {
                *diagnostic = "CUDA resident search allocation failed";
            }
            return {};
        }
        std::size_t reductionBytes = 0u;
        cudaError_t error = cub::DeviceReduce::Reduce(
                nullptr, reductionBytes,
                impl->candidateBestSamples.Get(),
                impl->reducedBest.Get(),
                winnerSampleSlots,
                BetterSample{
                        preparedConfiguration.evaluator.kind ==
                                CudaSearchEvaluatorKind::Velocity},
                DeviceSample{});
        if (error != cudaSuccess ||
            !impl->reductionTemporary.Allocate(reductionBytes)) {
            if (diagnostic != nullptr) {
                *diagnostic =
                        error != cudaSuccess
                        ? CudaFailure(
                                  "sizing CUDA winner reduction", error)
                        : "CUDA winner reduction allocation failed";
            }
            return {};
        }

#define UPLOAD(allocation, source, label)                                    \
        do {                                                                  \
            if (!(source).empty()) {                                          \
                error = cudaMemcpy(                                           \
                        (allocation).Get(), (source).data(),                   \
                        (source).size() * sizeof((source)[0]),                 \
                        cudaMemcpyHostToDevice);                               \
                if (error != cudaSuccess) {                                   \
                    if (diagnostic != nullptr) {                              \
                        *diagnostic = CudaFailure(label, error);               \
                    }                                                         \
                    return {};                                                \
                }                                                             \
                impl->initialUploadBytes +=                                   \
                        (source).size() * sizeof((source)[0]);                 \
            }                                                                 \
        } while (false)
        error = cudaMemcpy(
                impl->branchState.Get(),
                &preparedConfiguration.branchState,
                sizeof(preparedConfiguration.branchState),
                cudaMemcpyHostToDevice);
        if (error != cudaSuccess) {
            if (diagnostic != nullptr) {
                *diagnostic =
                        CudaFailure("uploading CUDA branch state", error);
            }
            return {};
        }
        impl->initialUploadBytes += sizeof(preparedConfiguration.branchState);
        error = cudaMemcpy(
                impl->branchControls.Get(),
                &inputPartition.branchControls,
                sizeof(inputPartition.branchControls),
                cudaMemcpyHostToDevice);
        if (error == cudaSuccess) {
            error = cudaMemcpy(
                    impl->mutableBoundaryControls.Get(),
                    &inputPartition.mutableBoundaryControls,
                    sizeof(inputPartition.mutableBoundaryControls),
                    cudaMemcpyHostToDevice);
        }
        if (error != cudaSuccess) {
            if (diagnostic != nullptr) {
                *diagnostic =
                        CudaFailure("uploading CUDA branch controls", error);
            }
            return {};
        }
        impl->initialUploadBytes +=
                sizeof(inputPartition.branchControls) +
                sizeof(inputPartition.mutableBoundaryControls);
        UPLOAD(impl->baselineTicks,
               preparedConfiguration.baselineTicks,
               "uploading CUDA baseline ticks");
        UPLOAD(impl->baselineInputs,
               preparedConfiguration.baselineInputs,
               "uploading CUDA baseline inputs");
        UPLOAD(impl->modifiers,
               preparedConfiguration.modifiers,
               "uploading CUDA modifier configuration");
        UPLOAD(impl->smoothWeights,
               preparedConfiguration.smoothWeights,
               "uploading CUDA smooth weights");
        UPLOAD(impl->compactInputIndices,
               compactInputIndices,
               "uploading CUDA compact input indices");
        UPLOAD(impl->compactInputOffsets,
               compactInputOffsets,
               "uploading CUDA compact input offsets");
#undef UPLOAD
        error = cudaMemcpy(
                impl->evaluator.Get(),
                &preparedConfiguration.evaluator,
                sizeof(preparedConfiguration.evaluator),
                cudaMemcpyHostToDevice);
        if (error != cudaSuccess) {
            if (diagnostic != nullptr) {
                *diagnostic = CudaFailure(
                        "uploading CUDA evaluator configuration",
                        error);
            }
            return {};
        }
        impl->initialUploadBytes += sizeof(preparedConfiguration.evaluator);
        error = cudaMemset(
                impl->globalBestSample.Get(), 0,
                impl->globalBestSample.Bytes());
        if (error == cudaSuccess) {
            error = cudaMemset(
                    impl->globalBestEventCount.Get(), 0,
                    impl->globalBestEventCount.Bytes());
        }
        if (error == cudaSuccess) {
            error = cudaMemset(
                    impl->globalBestMutationCount.Get(), 0,
                    impl->globalBestMutationCount.Bytes());
        }
        if (error != cudaSuccess) {
            if (diagnostic != nullptr) {
                *diagnostic =
                        CudaFailure("initializing CUDA search state", error);
            }
            return {};
        }

        if (!impl->LoadSimulationKernelMetrics(diagnostic)) {
            return {};
        }
        impl->UpdateResidentBytes();
        if (diagnostic != nullptr) {
            diagnostic->clear();
        }
        return std::unique_ptr<CudaSearchExecutor>(
                new CudaSearchExecutor(std::move(impl)));
    } catch (const std::bad_alloc &) {
        if (diagnostic != nullptr) {
            *diagnostic = "CUDA search host allocation failed";
        }
        return {};
    } catch (...) {
        if (diagnostic != nullptr) {
            *diagnostic = "unexpected CUDA search creation failure";
        }
        return {};
    }
}

CudaSearchBatchExecution CudaSearchExecutor::EvaluateBaseline() noexcept {
    return EvaluateBaseline(std::function<bool()>{});
}

CudaSearchBatchExecution CudaSearchExecutor::EvaluateBaseline(
        const std::function<bool()> &cancellationRequested) noexcept {
    if (!impl_ || impl_->baselineEvaluated) {
        CudaSearchBatchExecution result;
        result.status = CudaSearchStatus::InvalidArgument;
        result.diagnostic = !impl_
                ? "CUDA search executor is invalid"
                : "CUDA baseline was already evaluated";
        return result;
    }
    CudaSearchBatchExecution result =
            impl_->Execute(0u, 1u, true, cancellationRequested);
    result.hostToDeviceBytes += impl_->initialUploadBytes;
    return result;
}

CudaSearchBatchExecution CudaSearchExecutor::RunBatch(
        std::uint64_t firstCandidateId,
        std::uint32_t candidateCount,
        bool cancellationRequested) noexcept {
    const std::function<bool()> probe = cancellationRequested
            ? std::function<bool()>([] { return true; })
            : std::function<bool()>{};
    return RunBatch(firstCandidateId, candidateCount, probe);
}

CudaSearchBatchExecution CudaSearchExecutor::RunBatch(
        std::uint64_t firstCandidateId,
        std::uint32_t candidateCount,
        const std::function<bool()> &cancellationRequested) noexcept {
    if (!impl_) {
        CudaSearchBatchExecution result;
        result.status = CudaSearchStatus::InvalidArgument;
        result.diagnostic = "CUDA search executor is invalid";
        return result;
    }
    if (candidateCount != 0u &&
        firstCandidateId >
                std::numeric_limits<std::uint64_t>::max() -
                        (candidateCount - 1u)) {
        CudaSearchBatchExecution result;
        result.status = CudaSearchStatus::InvalidArgument;
        result.firstCandidateId = firstCandidateId;
        result.candidateCount = candidateCount;
        result.diagnostic = "CUDA candidate ID range overflow";
        return result;
    }
    return impl_->Execute(
            firstCandidateId, candidateCount, false,
            cancellationRequested);
}

bool CudaSearchExecutor::ReserveBatchCapacity(
        std::uint32_t candidateCount,
        std::string *diagnostic) noexcept {
    try {
        if (!impl_ || candidateCount == 0u) {
            if (diagnostic != nullptr) {
                *diagnostic = !impl_
                        ? "CUDA search executor is invalid"
                        : "CUDA batch capacity must be positive";
            }
            return false;
        }
        return impl_->ReserveBatchCapacity(candidateCount, diagnostic);
    } catch (const std::bad_alloc &) {
        if (diagnostic != nullptr) {
            *diagnostic =
                    "CUDA calibration host allocation failed";
        }
        return false;
    } catch (...) {
        if (diagnostic != nullptr) {
            *diagnostic =
                    "unexpected CUDA calibration allocation failure";
        }
        return false;
    }
}

std::uint32_t CudaSearchExecutor::BatchCapacity() const noexcept {
    return impl_ ? impl_->configuration.maximumBatchSize : 0u;
}

}  // namespace forevervalidator::simulation
