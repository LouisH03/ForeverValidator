#include <forevervalidator/experimental/physics_sandbox.h>
#include <forevervalidator/native.h>

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

namespace {

using forevervalidator::experimental::PhysicsSandboxError;
using Clock = std::chrono::steady_clock;

double Milliseconds(Clock::time_point begin, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

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

    if (argc < 6 || argc > 8) {
        return Fail(
                "usage: PACKS REPLAY CANDIDATES TIMELINE_TICKS "
                "REPETITIONS [BRANCH_TIME_MS] "
                "[random-steering|input-insertion]");
    }
    const std::uint32_t candidateCount =
            static_cast<std::uint32_t>(std::stoul(argv[3]));
    const std::uint32_t timelineTicks =
            static_cast<std::uint32_t>(std::stoul(argv[4]));
    const std::uint32_t repetitions =
            static_cast<std::uint32_t>(std::stoul(argv[5]));
    const std::uint64_t branchTimeMs =
            argc >= 7 ? std::stoull(argv[6]) : 5000u;
    const std::string modifier =
            argc == 8 ? argv[7] : "random-steering";
    if (modifier != "random-steering" &&
        modifier != "input-insertion") {
        return Fail("unknown modifier");
    }
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
    if (modifier == "random-steering") {
        configuration.modifiers.push_back(
                PhysicsSandboxCudaRandomSteeringModifier{
                        {firstTickTimeMs,
                         evaluationEndTimeMs,
                         0x6d2b79f5u}});
    } else {
        PhysicsSandboxCudaInputInsertionModifier insertion;
        insertion.window = {
                firstTickTimeMs,
                evaluationEndTimeMs,
                0x6d2b79f5u};
        insertion.steering.enabled = true;
        insertion.steering.minimumCount = 1u;
        insertion.steering.maximumCount = 1u;
        insertion.steeringOffset = true;
        insertion.steeringOffsetMinimum = 1;
        insertion.steeringOffsetMaximum = 1;
        configuration.modifiers.push_back(insertion);
    }
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

    std::uint64_t firstCandidateId = 0u;
    for (std::uint32_t repetition = 0u;
         repetition < repetitions; ++repetition) {
        const Clock::time_point wallBegin = Clock::now();
        auto batch = session.Value().RunBatch(
                firstCandidateId, candidateCount, false);
        const Clock::time_point wallEnd = Clock::now();
        if (!batch) {
            return Fail("CUDA search batch failed: " +
                        Diagnostic(batch.Error()));
        }
        if (batch.Value().cancelled) {
            return Fail("CUDA search batch was incomplete");
        }
        const double simulatedTicks =
                static_cast<double>(
                        batch.Value().evaluatedCandidateCount) *
                timelineTicks;
        const double kernelMilliseconds =
                batch.Value().metrics.kernelMilliseconds;
        const double simulationKernelMilliseconds =
                batch.Value().metrics.simulationKernelMilliseconds;
        const double normalizedPhysicsNanoseconds =
                simulatedTicks == 0.0
                ? 0.0
                : simulationKernelMilliseconds * 1.0e6 /
                          simulatedTicks;
        const double ticksPerSecond = simulationKernelMilliseconds == 0.0
                ? 0.0
                : simulatedTicks * 1000.0 /
                          simulationKernelMilliseconds;
        std::cout << std::fixed << std::setprecision(6)
                  << "{"
                  << "\"repetition\":" << repetition << ","
                  << "\"candidates\":" << candidateCount << ","
                  << "\"evaluated_candidates\":"
                  << batch.Value().evaluatedCandidateCount << ","
                  << "\"baseline_input_events\":"
                  << baseline.Value().bestInputs.size() << ","
                  << "\"best_input_events\":"
                  << batch.Value().bestInputs.size() << ","
                  << "\"total_mutation_count\":"
                  << batch.Value().totalMutationCount << ","
                  << "\"timeline_ticks\":" << timelineTicks << ","
                  << "\"branch_time_ms\":" << branchTimeMs << ","
                  << "\"modifier\":\"" << modifier << "\","
                  << "\"kernel_ms\":" << kernelMilliseconds << ","
                  << "\"wall_ms\":"
                  << Milliseconds(wallBegin, wallEnd) << ","
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
                  << "\"simulation_threads_per_block\":"
                  << batch.Value().metrics
                             .simulationThreadsPerBlock
                  << ","
                  << "\"simulation_registers_per_thread\":"
                  << batch.Value().metrics
                             .simulationRegistersPerThread
                  << ","
                  << "\"simulation_local_bytes_per_thread\":"
                  << batch.Value().metrics
                             .simulationLocalBytesPerThread
                  << ","
                  << "\"simulation_active_blocks_per_sm\":"
                  << batch.Value().metrics
                             .simulationActiveBlocksPerMultiprocessor
                  << ","
                  << "\"simulation_theoretical_occupancy\":"
                  << batch.Value().metrics
                             .simulationTheoreticalOccupancy
                  << ","
                  << "\"resident_device_bytes\":"
                  << batch.Value().metrics.residentDeviceBytes
                  << ",\"host_to_device_bytes\":"
                  << batch.Value().metrics.hostToDeviceBytes
                  << ",\"device_to_host_bytes\":"
                  << batch.Value().metrics.deviceToHostBytes
                  << "}\n";
        firstCandidateId += candidateCount;
    }
    return 0;
}
