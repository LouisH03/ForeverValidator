#include <forevervalidator/experimental/physics_sandbox.h>
#include <forevervalidator/native.h>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

namespace {

using forevervalidator::experimental::PhysicsSandboxError;

int Fail(const std::string &message) {
    std::cerr << "cuda_search_benchmark: " << message << '\n';
    return 1;
}

std::string Diagnostic(const PhysicsSandboxError &error) {
    return error.diagnostic.empty() ? "unknown error" : error.diagnostic;
}

}  // namespace

int main(int argc, char **argv) {
    using namespace forevervalidator;
    using namespace forevervalidator::experimental;

    if (argc < 6 || argc > 7) {
        return Fail(
                "usage: PACKS REPLAY CANDIDATES TIMELINE_TICKS "
                "REPETITIONS [BRANCH_TIME_MS]");
    }
    const std::uint32_t candidateCount =
            static_cast<std::uint32_t>(std::stoul(argv[3]));
    const std::uint32_t timelineTicks =
            static_cast<std::uint32_t>(std::stoul(argv[4]));
    const std::uint32_t repetitions =
            static_cast<std::uint32_t>(std::stoul(argv[5]));
    const std::uint64_t branchTimeMs =
            argc == 7 ? std::stoull(argv[6]) : 5000u;
    if (candidateCount == 0u || timelineTicks == 0u ||
        repetitions == 0u) {
        return Fail("benchmark dimensions must be positive");
    }

    auto replay = ReadNativeReplayFile(
            argv[2], ReplayIdentity{argv[2]});
    if (!replay) {
        return Fail("could not read replay");
    }
    auto source = OpenInstalledPackDirectory(argv[1]);
    if (!source) {
        return Fail("could not open pack source");
    }
    PhysicsSandboxOptions options;
    options.backend = SimulationBackend::Cuda;
    auto sandbox = CreatePhysicsSandbox(
            std::move(source).Value(), options);
    if (!sandbox) {
        return Fail("could not create CUDA sandbox: " +
                    Diagnostic(sandbox.Error()));
    }
    auto loaded = sandbox.Value().LoadReplay(
            {replay.Value().data(), replay.Value().size()},
            ReplayIdentity{argv[2]});
    if (!loaded) {
        return Fail("could not load replay: " +
                    Diagnostic(loaded.Error()));
    }
    const std::uint32_t tickDurationMs = options.tickDurationMs;
    if (branchTimeMs < loaded.Value().timeMs ||
        (branchTimeMs - loaded.Value().timeMs) % tickDurationMs != 0u) {
        return Fail("branch time is not reachable on whole ticks");
    }
    const std::uint64_t advanceTicks =
            (branchTimeMs - loaded.Value().timeMs) / tickDurationMs;
    if (advanceTicks > std::numeric_limits<std::uint32_t>::max()) {
        return Fail("branch time is too large");
    }
    if (advanceTicks != 0u) {
        auto advanced = sandbox.Value().AdvanceTicks(
                static_cast<std::uint32_t>(advanceTicks));
        if (!advanced) {
            return Fail("could not advance to branch: " +
                        Diagnostic(advanced.Error()));
        }
    }

    const std::int64_t firstTickTimeMs =
            static_cast<std::int64_t>(branchTimeMs + tickDurationMs);
    const std::int64_t evaluationEndTimeMs =
            firstTickTimeMs +
            static_cast<std::int64_t>(timelineTicks - 1u) *
                    tickDurationMs;
    PhysicsSandboxCudaSearchConfiguration configuration;
    configuration.maximumBatchSize = candidateCount;
    configuration.earliestMutationTimeMs = firstTickTimeMs;
    configuration.evaluationStartTimeMs = firstTickTimeMs;
    configuration.evaluationEndTimeMs = evaluationEndTimeMs;
    configuration.modifiers.push_back(
            PhysicsSandboxCudaRandomSteeringModifier{
                    {firstTickTimeMs,
                     evaluationEndTimeMs,
                     0x6d2b79f5u}});
    configuration.evaluator = PhysicsSandboxCudaVelocityEvaluator{};

    auto session = CreatePhysicsSandboxCudaSearchSession(
            sandbox.Value(), configuration);
    if (!session) {
        return Fail("could not create CUDA search session: " +
                    Diagnostic(session.Error()));
    }
    auto baseline = session.Value().EvaluateBaseline();
    if (!baseline) {
        return Fail("could not evaluate baseline: " +
                    Diagnostic(baseline.Error()));
    }

    const double simulatedTicks =
            static_cast<double>(candidateCount) * timelineTicks;
    std::uint64_t firstCandidateId = 0u;
    for (std::uint32_t repetition = 0u;
         repetition < repetitions; ++repetition) {
        auto batch = session.Value().RunBatch(
                firstCandidateId, candidateCount, false);
        if (!batch) {
            return Fail("CUDA search batch failed: " +
                        Diagnostic(batch.Error()));
        }
        if (batch.Value().cancelled ||
            batch.Value().evaluatedCandidateCount != candidateCount) {
            return Fail("CUDA search batch was incomplete");
        }
        const double kernelMilliseconds =
                batch.Value().metrics.kernelMilliseconds;
        const double simulationKernelMilliseconds =
                batch.Value().metrics.simulationKernelMilliseconds;
        const double normalizedPhysicsNanoseconds =
                simulationKernelMilliseconds * 1.0e6 /
                simulatedTicks;
        const double ticksPerSecond =
                simulatedTicks * 1000.0 /
                simulationKernelMilliseconds;
        std::cout << std::fixed << std::setprecision(6)
                  << "{"
                  << "\"repetition\":" << repetition << ","
                  << "\"candidates\":" << candidateCount << ","
                  << "\"timeline_ticks\":" << timelineTicks << ","
                  << "\"branch_time_ms\":" << branchTimeMs << ","
                  << "\"kernel_ms\":" << kernelMilliseconds << ","
                  << "\"score_initialization_kernel_ms\":"
                  << batch.Value().metrics
                             .scoreInitializationKernelMilliseconds
                  << ","
                  << "\"mutation_kernel_ms\":"
                  << batch.Value().metrics.mutationKernelMilliseconds
                  << ","
                  << "\"simulation_kernel_ms\":"
                  << simulationKernelMilliseconds << ","
                  << "\"winner_kernel_ms\":"
                  << batch.Value().metrics.winnerKernelMilliseconds
                  << ","
                  << "\"winner_reduction_kernel_ms\":"
                  << batch.Value().metrics
                             .winnerReductionKernelMilliseconds
                  << ","
                  << "\"winner_state_capture_kernel_ms\":"
                  << batch.Value().metrics
                             .winnerStateCaptureKernelMilliseconds
                  << ","
                  << "\"finalization_kernel_ms\":"
                  << batch.Value().metrics
                             .finalizationKernelMilliseconds
                  << ","
                  << "\"simulation_kernel_ns_per_tick\":"
                  << normalizedPhysicsNanoseconds << ","
                  << "\"simulation_kernel_ticks_per_second\":"
                  << ticksPerSecond << ","
                  << "\"resident_device_bytes\":"
                  << batch.Value().metrics.residentDeviceBytes
                  << "}\n";
        firstCandidateId += candidateCount;
    }
    return 0;
}
