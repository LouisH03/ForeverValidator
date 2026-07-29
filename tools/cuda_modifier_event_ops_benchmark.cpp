#include "simulation/backends/cuda/cuda_modifier_event_ops.cuh"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
using forevervalidator::simulation::CudaSearchInputEvent;
namespace modifier_ops =
        forevervalidator::simulation::cuda_search_modifier_detail;

template<typename Operation>
double NanosecondsPerIteration(
        std::uint32_t iterations,
        Operation operation) {
    const auto begin = Clock::now();
    for (std::uint32_t iteration = 0u;
         iteration < iterations; ++iteration) {
        operation(iteration);
    }
    const auto end = Clock::now();
    return std::chrono::duration<double, std::nano>(
                   end - begin)
                   .count() /
            iterations;
}

std::int32_t FullScanState(
        const CudaSearchInputEvent *events,
        std::uint32_t count,
        std::int64_t timeMs) {
    std::int32_t result = 0;
    std::int64_t bestTime = INT64_MIN;
    for (std::uint32_t index = 0u; index < count; ++index) {
        const CudaSearchInputEvent &event = events[index];
        if (event.action == 4u && event.valueKind == 2u &&
            event.timeMs <= timeMs && event.timeMs >= bestTime) {
            result = event.value;
            bestTime = event.timeMs;
        }
    }
    return result;
}

std::uint32_t FullScanWindow(
        const CudaSearchInputEvent *events,
        std::uint32_t count,
        std::int64_t minimumTimeMs,
        std::int64_t maximumTimeMs) {
    std::uint32_t result = 0u;
    for (std::uint32_t index = 0u; index < count; ++index) {
        if (events[index].timeMs >= minimumTimeMs &&
            events[index].timeMs <= maximumTimeMs) {
            ++result;
        }
    }
    return result;
}

std::int32_t LegacyRemoveRange(
        CudaSearchInputEvent *events,
        std::uint32_t *count,
        std::int64_t start,
        std::int64_t end) {
    const std::int32_t state =
            FullScanState(events, *count, start);
    std::uint32_t destination = 0u;
    for (std::uint32_t index = 0u; index < *count; ++index) {
        const CudaSearchInputEvent event = events[index];
        if (event.action == 4u &&
            event.timeMs >= start && event.timeMs <= end) {
            continue;
        }
        events[destination++] = event;
    }
    *count = destination;
    return state;
}

void LegacyDelete(
        CudaSearchInputEvent *events,
        std::uint32_t *count,
        std::uint32_t *scratch,
        std::uint32_t requested) {
    for (std::uint32_t removal = 0u;
         removal < requested; ++removal) {
        const std::uint32_t eligibleCount =
                modifier_ops::CollectDeletionEligible(
                        events, *count, scratch,
                        0, INT64_MAX, 0u, true);
        if (eligibleCount == 0u) {
            return;
        }
        const std::uint32_t selected =
                scratch[(removal * 17u + 3u) % eligibleCount];
        for (std::uint32_t index = selected + 1u;
             index < *count; ++index) {
            events[index - 1u] = events[index];
        }
        --*count;
    }
}

void OptimizedDelete(
        CudaSearchInputEvent *events,
        std::uint32_t *count,
        std::uint32_t *scratch,
        std::uint32_t requested) {
    const std::uint32_t initialEligibleCount =
            modifier_ops::CollectDeletionEligible(
                    events, *count, scratch,
                    0, INT64_MAX, 0u, true);
    std::uint32_t remaining = initialEligibleCount;
    for (std::uint32_t removal = 0u;
         removal < requested && remaining != 0u; ++removal) {
        modifier_ops::SelectDeletionRank(
                scratch, &remaining,
                (removal * 17u + 3u) % remaining);
    }
    modifier_ops::CompactSelectedDeletionTail(
            events, count, scratch,
            remaining, initialEligibleCount);
}

void PrintPair(const std::string &name,
               double legacy,
               double optimized) {
    std::cout << std::left << std::setw(28) << name
              << std::right << std::setw(14) << legacy
              << std::setw(14) << optimized
              << std::setw(12) << legacy / optimized << '\n';
}

}  // namespace

int main(int argc, char **argv) {
    const std::uint32_t eventCount =
            argc >= 2
            ? static_cast<std::uint32_t>(std::stoul(argv[1]))
            : 1000u;
    const std::uint32_t iterations =
            argc >= 3
            ? static_cast<std::uint32_t>(std::stoul(argv[2]))
            : 100000u;
    if (eventCount == 0u || iterations == 0u) {
        std::cerr << "event count and iterations must be positive\n";
        return 1;
    }

    std::vector<CudaSearchInputEvent> events;
    events.reserve(eventCount);
    for (std::uint32_t index = 0u; index < eventCount; ++index) {
        const std::uint32_t action =
                index % 4u == 0u ? 4u
                : index % 4u == 1u ? 1u
                : index % 4u == 2u ? 3u
                                   : 2u;
        events.push_back(
                {static_cast<std::int32_t>(index * 10u),
                 action, action == 4u ? 2u : 1u,
                 static_cast<std::int32_t>(index * 97u)});
    }

    volatile std::uint64_t checksum = 0u;
    const std::int64_t windowBegin =
            static_cast<std::int64_t>(eventCount * 4u);
    const std::int64_t windowEnd = windowBegin +
            static_cast<std::int64_t>(eventCount);
    const double windowLegacy = NanosecondsPerIteration(
            iterations, [&](std::uint32_t) {
                checksum += FullScanWindow(
                        events.data(), eventCount,
                        windowBegin, windowEnd);
            });
    const double windowOptimized = NanosecondsPerIteration(
            iterations, [&](std::uint32_t) {
                checksum += modifier_ops::UpperBoundTime(
                                    events.data(), eventCount,
                                    windowEnd) -
                        modifier_ops::LowerBoundTime(
                                events.data(), eventCount,
                                windowBegin);
            });
    const double stateLegacy = NanosecondsPerIteration(
            iterations, [&](std::uint32_t iteration) {
                checksum += FullScanState(
                        events.data(), eventCount,
                        (iteration % eventCount) * 10u);
            });
    const double stateOptimized = NanosecondsPerIteration(
            iterations, [&](std::uint32_t iteration) {
                checksum += modifier_ops::ChannelStateAt(
                        events.data(), eventCount, 4u, 2u,
                        (iteration % eventCount) * 10u, true);
            });
    std::vector<CudaSearchInputEvent> working(eventCount);
    std::vector<std::uint32_t> scratch(eventCount);
    const std::uint32_t structuralIterations =
            std::max(1u, iterations / 100u);
    const double insertionLegacy = NanosecondsPerIteration(
            structuralIterations, [&](std::uint32_t) {
                std::copy(
                        events.begin(), events.end(),
                        working.begin());
                std::uint32_t count = eventCount;
                checksum += LegacyRemoveRange(
                        working.data(), &count,
                        windowBegin, windowEnd);
                checksum += count;
            });
    const double insertionOptimized = NanosecondsPerIteration(
            structuralIterations, [&](std::uint32_t) {
                std::copy(
                        events.begin(), events.end(),
                        working.begin());
                std::uint32_t count = eventCount;
                checksum +=
                        modifier_ops::
                                RemoveActionRangeAndReadState(
                                        working.data(), &count,
                                        4u, 2u,
                                        windowBegin, windowEnd);
                checksum += count;
            });
    constexpr std::uint32_t DeletionCount = 32u;
    const double deletionLegacy = NanosecondsPerIteration(
            structuralIterations, [&](std::uint32_t) {
                std::copy(
                        events.begin(), events.end(),
                        working.begin());
                std::uint32_t count = eventCount;
                LegacyDelete(
                        working.data(), &count,
                        scratch.data(), DeletionCount);
                checksum += count;
            });
    const double deletionOptimized = NanosecondsPerIteration(
            structuralIterations, [&](std::uint32_t) {
                std::copy(
                        events.begin(), events.end(),
                        working.begin());
                std::uint32_t count = eventCount;
                OptimizedDelete(
                        working.data(), &count,
                        scratch.data(), DeletionCount);
                checksum += count;
            });

    std::cout << "events=" << eventCount
              << " iterations=" << iterations
              << " checksum=" << checksum << '\n';
    std::cout << std::left << std::setw(28) << "operation"
              << std::right << std::setw(14) << "full_scan_ns"
              << std::setw(14) << "indexed_ns"
              << std::setw(12) << "speedup" << '\n';
    PrintPair(
            "modifier window lookup",
            windowLegacy, windowOptimized);
    PrintPair(
            "steering state lookup",
            stateLegacy, stateOptimized);
    PrintPair(
            "insertion range replace",
            insertionLegacy, insertionOptimized);
    PrintPair(
            "deletion (32 events)",
            deletionLegacy, deletionOptimized);
    return 0;
}
