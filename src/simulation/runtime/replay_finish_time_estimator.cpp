#include "simulation/runtime/replay_finish_time_estimator.h"

#include <cmath>
#include <limits>

namespace {

constexpr double NanosecondsPerSecond = 1000000000.0;
constexpr std::uint64_t NanosecondsPerMillisecond = 1000000u;

}  // namespace

std::optional<forevervalidator::FinishTimeEstimate>
RefineReplayFinishTime(const ReplayFinishSubstep &substep,
                       const ReplayFinishProbe &probe) {
    if (!probe || !std::isfinite(substep.substepStartSeconds) ||
        substep.substepStartSeconds < 0.0 ||
        !std::isfinite(substep.substepDurationSeconds) ||
        !(substep.substepDurationSeconds > 0.0f) ||
        substep.tickTimeMs < substep.tickPeriodMs) {
        return std::nullopt;
    }

    const std::uint64_t tickStartNs =
            static_cast<std::uint64_t>(
                    substep.tickTimeMs - substep.tickPeriodMs) *
            NanosecondsPerMillisecond;
    double lower = static_cast<double>(tickStartNs) +
                   substep.substepStartSeconds * NanosecondsPerSecond;
    double upper = lower +
                   static_cast<double>(substep.substepDurationSeconds) *
                           NanosecondsPerSecond;
    if (!std::isfinite(lower) || !std::isfinite(upper) ||
        lower < 0.0 || !(lower < upper) ||
        upper > static_cast<double>(
                        std::numeric_limits<std::uint64_t>::max() - 1u)) {
        return std::nullopt;
    }
    if (!probe(substep.substepDurationSeconds)) {
        return std::nullopt;
    }

    const double substepStartNs = lower;
    for (;;) {
        const std::uint64_t firstInteriorNs =
                static_cast<std::uint64_t>(std::floor(lower)) + 1u;
        const std::uint64_t upperCeilingNs =
                static_cast<std::uint64_t>(std::ceil(upper));
        if (upperCeilingNs == 0u ||
            firstInteriorNs >= upperCeilingNs) {
            break;
        }
        const std::uint64_t lastInteriorNs = upperCeilingNs - 1u;
        const std::uint64_t candidateNs =
                firstInteriorNs +
                (lastInteriorNs - firstInteriorNs) / 2u;
        const double partialSeconds =
                (static_cast<double>(candidateNs) - substepStartNs) /
                NanosecondsPerSecond;
        if (partialSeconds <= 0.0) {
            lower = static_cast<double>(candidateNs);
            continue;
        }
        if (probe(static_cast<float>(partialSeconds))) {
            upper = static_cast<double>(candidateNs);
        } else {
            lower = static_cast<double>(candidateNs);
        }
    }

    forevervalidator::FinishTimeEstimate estimate;
    estimate.lowerBoundNs =
            static_cast<std::uint64_t>(std::floor(lower));
    estimate.upperBoundNs =
            static_cast<std::uint64_t>(std::ceil(upper));
    estimate.estimatedNs = estimate.upperBoundNs;
    return estimate.IsValid()
                   ? std::optional<forevervalidator::FinishTimeEstimate>(
                             estimate)
                   : std::nullopt;
}
