#include <forevervalidator/json.h>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

#include "simulation/runtime/replay_finish_time_estimator.h"

namespace {

bool Contains(const forevervalidator::FinishTimeEstimate &estimate,
              std::uint64_t transitionNs) {
    return estimate.IsValid() &&
           estimate.lowerBoundNs < transitionNs &&
           transitionNs <= estimate.upperBoundNs;
}

bool TestInteriorTransition() {
    const ReplayFinishSubstep substep{1010u, 10u, 0.003, 0.002f};
    constexpr std::uint64_t transitionNs = 1004234567u;
    const double substepStartNs = 1003000000.0;
    const auto estimate = RefineReplayFinishTime(
            substep,
            [&](float dt) {
                const double absoluteNs =
                        substepStartNs +
                        static_cast<double>(dt) * 1000000000.0;
                return absoluteNs >=
                       static_cast<double>(transitionNs);
            });
    return estimate.has_value() && Contains(*estimate, transitionNs);
}

bool TestExactSubstepFinish() {
    const ReplayFinishSubstep substep{20u, 10u, 0.0, 0.01f};
    const double exactEnd =
            10000000.0 +
            static_cast<double>(substep.substepDurationSeconds) *
                    1000000000.0;
    const auto estimate = RefineReplayFinishTime(
            substep,
            [&](float dt) {
                return dt >= substep.substepDurationSeconds;
            });
    return estimate.has_value() && estimate->IsValid() &&
           estimate->lowerBoundNs <
                   static_cast<std::uint64_t>(std::ceil(exactEnd)) &&
           static_cast<std::uint64_t>(std::ceil(exactEnd)) <=
                   estimate->upperBoundNs;
}

bool TestNonFinish() {
    const ReplayFinishSubstep substep{10u, 10u, 0.0, 0.01f};
    return !RefineReplayFinishTime(
                    substep, [](float) { return false; })
                    .has_value();
}

bool TestBoundaryAndRepeatedRuns() {
    const ReplayFinishSubstep substep{
            100u, 10u, 0.00000000025, 0.000000004f};
    const auto run = [&]() {
        return RefineReplayFinishTime(
                substep,
                [](float dt) { return dt >= 2.0e-9f; });
    };
    const auto first = run();
    if (!first.has_value() || !first->IsValid()) {
        return false;
    }
    for (int runIndex = 0; runIndex < 32; ++runIndex) {
        if (run() != first) {
            return false;
        }
    }
    return true;
}

bool TestSerialization() {
    forevervalidator::ValidationReport report;
    report.replay.name = "finish-time";
    report.simulation.raceCompleted = true;
    report.simulation.raceTimeMs = 123;
    report.simulation.raceTime =
            forevervalidator::FinishTimeEstimate{
                    123456788u, 123456789u, 123456789u};
    const auto serialized =
            forevervalidator::SerializeValidationReport(report);
    if (!serialized.HasValue()) {
        return false;
    }
    const std::string &json = serialized.Value();
    return json.find("\"race_time_ns\":123456789") !=
                   std::string::npos &&
           json.find(
                   "\"race_time_bracket_ns\":{\"lower_exclusive\":"
                   "123456788,\"upper_inclusive\":123456789}") !=
                   std::string::npos;
}

}  // namespace

int main() {
    if (!TestInteriorTransition()) {
        std::cerr << "interior transition bracket failed\n";
        return 1;
    }
    if (!TestExactSubstepFinish()) {
        std::cerr << "exact-substep transition bracket failed\n";
        return 1;
    }
    if (!TestNonFinish()) {
        std::cerr << "non-finish was assigned a finish bracket\n";
        return 1;
    }
    if (!TestBoundaryAndRepeatedRuns()) {
        std::cerr << "boundary or repeated-run determinism failed\n";
        return 1;
    }
    if (!TestSerialization()) {
        std::cerr << "nanosecond serialization failed\n";
        return 1;
    }
    return 0;
}
