#include <algorithm>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

#include "simulation/backends/cuda/cuda_candidate_events.cuh"

namespace {

using forevervalidator::simulation::CudaSearchInputEvent;
namespace candidate_events =
        forevervalidator::simulation::cuda::candidate_events;

bool Same(const CudaSearchInputEvent &left,
          const CudaSearchInputEvent &right) {
    return left.timeMs == right.timeMs &&
            left.action == right.action &&
            left.valueKind == right.valueKind &&
            left.value == right.value;
}

std::vector<CudaSearchInputEvent> LegacyNormalize(
        std::vector<CudaSearchInputEvent> events,
        const std::vector<CudaSearchInputEvent> &baseline,
        std::int64_t mutableFromTimeMs) {
    std::vector<CudaSearchInputEvent> sorted;
    for (CudaSearchInputEvent value : events) {
        if (value.timeMs < mutableFromTimeMs) {
            continue;
        }
        value.timeMs = std::max(value.timeMs, 0);
        if (value.valueKind == 2u) {
            value.value = std::clamp(value.value, -65536, 65536);
        } else if (value.valueKind == 1u) {
            value.value = value.value == 0 ? 0 : 1;
        }
        const auto insertion = std::upper_bound(
                sorted.begin(), sorted.end(), value.timeMs,
                [](std::int32_t time,
                   const CudaSearchInputEvent &event) {
                    return time < event.timeMs;
                });
        sorted.insert(insertion, value);
    }

    std::vector<CudaSearchInputEvent> suffix;
    for (const CudaSearchInputEvent &value : sorted) {
        const auto duplicate = std::find_if(
                suffix.rbegin(), suffix.rend(),
                [&](const CudaSearchInputEvent &candidate) {
                    return candidate.timeMs == value.timeMs &&
                            candidate.action == value.action;
                });
        if (duplicate == suffix.rend()) {
            suffix.push_back(value);
        } else {
            *duplicate = value;
        }
    }

    std::vector<CudaSearchInputEvent> result;
    for (const CudaSearchInputEvent &value : baseline) {
        if (value.timeMs < mutableFromTimeMs) {
            result.push_back(value);
        }
    }
    result.insert(result.end(), suffix.begin(), suffix.end());
    return result;
}

bool DifferentialNormalization() {
    std::mt19937 random(0x4f87a221u);
    for (std::uint32_t trial = 0u; trial < 20000u; ++trial) {
        const std::uint32_t count = random() % 96u;
        const std::uint32_t baselineCount = random() % 24u;
        const std::int64_t boundary =
                static_cast<std::int32_t>(random() % 60u) - 10;
        std::vector<CudaSearchInputEvent> events;
        std::vector<CudaSearchInputEvent> baseline;
        events.reserve(128u);
        baseline.reserve(32u);
        for (std::uint32_t index = 0u;
             index < baselineCount; ++index) {
            baseline.push_back({
                    static_cast<std::int32_t>(random() % 90u) - 20,
                    static_cast<std::uint32_t>(random() % 9u),
                    static_cast<std::uint32_t>(
                            1u + random() % 2u),
                    static_cast<std::int32_t>(random() % 180000u) -
                            90000});
        }
        for (std::uint32_t index = 0u; index < count; ++index) {
            events.push_back({
                    static_cast<std::int32_t>(random() % 90u) - 20,
                    static_cast<std::uint32_t>(random() % 9u),
                    static_cast<std::uint32_t>(
                            1u + random() % 2u),
                    static_cast<std::int32_t>(random() % 180000u) -
                            90000});
        }

        const std::vector<CudaSearchInputEvent> expected =
                LegacyNormalize(events, baseline, boundary);
        events.resize(128u);
        std::vector<CudaSearchInputEvent> scratch(128u);
        const std::uint32_t actualCount =
                candidate_events::NormalizeWithPrefix(
                        events.data(), count, scratch.data(),
                        baseline.data(),
                        static_cast<std::uint32_t>(baseline.size()),
                        boundary,
                        static_cast<std::uint32_t>(events.size()));
        if (actualCount != expected.size()) {
            std::cerr << "normalization count mismatch at trial "
                      << trial << "\n";
            return false;
        }
        for (std::uint32_t index = 0u;
             index < actualCount; ++index) {
            if (!Same(events[index], expected[index])) {
                std::cerr << "normalization value mismatch at trial "
                          << trial << ", index " << index << "\n";
                return false;
            }
        }
    }
    return true;
}

bool CompactEdits() {
    constexpr std::uint32_t candidates = 32u;
    constexpr std::uint32_t editCapacity = 8u;
    std::vector<std::uint32_t> counts(candidates);
    std::vector<std::uint32_t> erasedCounts(candidates);
    std::vector<std::uint32_t> outputIndices(
            candidates * editCapacity);
    std::vector<std::int32_t> times(candidates * editCapacity);
    std::vector<std::uint32_t> actions(candidates * editCapacity);
    std::vector<std::uint32_t> valueKinds(candidates * editCapacity);
    std::vector<std::int32_t> values(candidates * editCapacity);
    std::vector<std::uint32_t> erasedSourceIndices(
            candidates * editCapacity);
    candidate_events::CoalescedEditStorage storage{
            counts.data(), erasedCounts.data(),
            outputIndices.data(), times.data(),
            actions.data(), valueKinds.data(), values.data(),
            erasedSourceIndices.data(),
            nullptr, nullptr, nullptr,
            candidates, editCapacity, editCapacity};

    const CudaSearchInputEvent baseline[]{
            {10, 4u, 2u, 100},
            {20, 1u, 1u, 1},
            {30, 4u, 2u, 300}};
    const auto suffix = candidate_events::SuffixFrom(
            baseline, 3u, 10);
    candidate_events::EditWriter writer(storage, 7u);
    if (!writer.Replace(
                0u, {10, 4u, 2u, -50}) ||
        !writer.Insert(1u, {15, 3u, 1u, 1}) ||
        !writer.Erase(0u) ||
        !writer.Erase(1u)) {
        return false;
    }
    candidate_events::CandidateView view{
            suffix, storage, 7u, 3u};
    std::vector<CudaSearchInputEvent> materialized(8u);
    if (!candidate_events::Materialize(
                view, materialized.data(),
                static_cast<std::uint32_t>(materialized.size())) ||
        !Same(materialized[0], {10, 4u, 2u, -50}) ||
        !Same(materialized[1], {15, 3u, 1u, 1}) ||
        !Same(materialized[2], {30, 4u, 2u, 300})) {
        return false;
    }

    // Ordinal-major indexing is contiguous across a warp.
    return candidate_events::EditOffset(storage, 0u, 3u) + 31u ==
            candidate_events::EditOffset(storage, 31u, 3u);
}

bool ReorderedReplacement() {
    constexpr std::uint32_t candidates = 4u;
    std::uint32_t counts[candidates]{};
    std::uint32_t erasedCounts[candidates]{};
    std::uint32_t outputIndices[candidates * 2u]{};
    std::int32_t times[candidates * 2u]{};
    std::uint32_t actions[candidates * 2u]{};
    std::uint32_t valueKinds[candidates * 2u]{};
    std::int32_t values[candidates * 2u]{};
    std::uint32_t erasedSourceIndices[candidates * 2u]{};
    candidate_events::CoalescedEditStorage storage{
            counts, erasedCounts, outputIndices,
            times, actions, valueKinds, values, erasedSourceIndices,
            nullptr, nullptr, nullptr,
            candidates, 2u, 2u};
    const CudaSearchInputEvent baseline[]{
            {10, 4u, 2u, 1},
            {20, 1u, 1u, 1},
            {30, 4u, 2u, 3}};
    candidate_events::EditWriter writer(storage, 2u);
    writer.Replace(0u, {5, 4u, 2u, 75});
    writer.Erase(2u);
    candidate_events::CandidateView view{
            {baseline, 3u, 10}, storage, 2u, 3u};
    candidate_events::CandidateCursor cursor(view);
    CudaSearchInputEvent events[3]{};
    return cursor.Next(events) &&
            cursor.Next(events + 1u) &&
            cursor.Next(events + 2u) &&
            Same(events[0], {5, 4u, 2u, 75}) &&
            Same(events[1], baseline[0]) &&
            Same(events[2], baseline[1]);
}

bool PackedEdits() {
    constexpr std::uint32_t candidates = 3u;
    constexpr std::uint32_t capacity = 2u;
    std::uint32_t counts[candidates]{};
    std::uint32_t erasedCounts[candidates]{};
    std::uint32_t packedOutputActions[candidates * capacity]{};
    std::int32_t times[candidates * capacity]{};
    std::uint8_t packedValueKinds[candidates * capacity]{};
    std::int32_t values[candidates * capacity]{};
    std::uint16_t packedErasedSources[candidates * capacity]{};
    candidate_events::CoalescedEditStorage storage;
    storage.counts = counts;
    storage.erasedCounts = erasedCounts;
    storage.times = times;
    storage.values = values;
    storage.packedOutputActions = packedOutputActions;
    storage.packedValueKinds = packedValueKinds;
    storage.packedErasedSourceIndices = packedErasedSources;
    storage.candidateStride = candidates;
    storage.editCapacity = capacity;
    storage.eraseCapacity = capacity;

    const CudaSearchInputEvent baseline[]{
            {10, 4u, 2u, 10},
            {20, 1u, 1u, 1}};
    candidate_events::EditWriter writer(storage, 1u);
    if (!writer.Insert(0u, {5, 3u, 1u, 1}) ||
        !writer.Erase(1u)) {
        return false;
    }
    CudaSearchInputEvent result[2]{};
    return candidate_events::Materialize(
                   {{baseline, 2u, 0}, storage, 1u, 2u},
                   result, 2u) &&
            Same(result[0], {5, 3u, 1u, 1}) &&
            Same(result[1], baseline[0]);
}

bool EqualTimeStableLastWrite() {
    std::vector<CudaSearchInputEvent> events{
            {20, UINT32_MAX, 2u, 1},
            {20, 7u, 1u, 1},
            {20, UINT32_MAX, 2u, 2},
            {20, 3u, 1u, 0},
            {20, 7u, 1u, 0}};
    std::vector<CudaSearchInputEvent> scratch(events.size());
    const std::uint32_t count = candidate_events::NormalizeSuffix(
            events.data(),
            static_cast<std::uint32_t>(events.size()),
            scratch.data());
    return count == 3u &&
            Same(events[0], {20, UINT32_MAX, 2u, 2}) &&
            Same(events[1], {20, 7u, 1u, 0}) &&
            Same(events[2], {20, 3u, 1u, 0});
}

}  // namespace

int main() {
    if (!DifferentialNormalization() ||
        !CompactEdits() ||
        !ReorderedReplacement() ||
        !PackedEdits() ||
        !EqualTimeStableLastWrite()) {
        return 1;
    }
    std::cout << "CUDA candidate event tests passed\n";
    return 0;
}
