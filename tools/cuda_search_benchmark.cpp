#include <forevervalidator/experimental/physics_sandbox.h>
#include <forevervalidator/native.h>

#include <chrono>
#include <cstring>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <string>

namespace {

using forevervalidator::experimental::PhysicsSandboxError;
using forevervalidator::experimental::PhysicsSandboxCudaSearchBatch;
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

bool SameInput(
        const forevervalidator::experimental::PhysicsSandboxInputEvent &left,
        const forevervalidator::experimental::PhysicsSandboxInputEvent &right) {
    return left.timeMs == right.timeMs &&
            left.action == right.action &&
            left.value.kind == right.value.kind &&
            left.value.switchState == right.value.switchState &&
            left.value.analog == right.value.analog;
}

bool SameBatch(const PhysicsSandboxCudaSearchBatch &left,
               const PhysicsSandboxCudaSearchBatch &right) {
    if (left.firstCandidateId != right.firstCandidateId ||
        left.candidateCount != right.candidateCount ||
        left.evaluatedCandidateCount != right.evaluatedCandidateCount ||
        left.evaluatorCalls != right.evaluatorCalls ||
        left.totalMutationCount != right.totalMutationCount ||
        left.mutationImprovementCount != right.mutationImprovementCount ||
        left.cancelled != right.cancelled ||
        left.bestChanged != right.bestChanged ||
        left.bestIsMutation != right.bestIsMutation ||
        left.bestCandidateId != right.bestCandidateId ||
        left.bestMutationCount != right.bestMutationCount ||
        left.bestScore != right.bestScore ||
        left.bestTimeMs != right.bestTimeMs ||
        left.bestDetail0 != right.bestDetail0 ||
        left.bestDetail1 != right.bestDetail1 ||
        left.bestInputs.size() != right.bestInputs.size() ||
        left.bestSnapshot.has_value() != right.bestSnapshot.has_value() ||
        std::memcmp(
                &left.bestState.car,
                &right.bestState.car,
                sizeof(left.bestState.car)) != 0 ||
        left.bestState.tick != right.bestState.tick ||
        left.bestState.timeMs != right.bestState.timeMs ||
        left.bestState.durationMs != right.bestState.durationMs ||
        left.bestState.mapEnvironment != right.bestState.mapEnvironment ||
        left.bestState.vehicleModel != right.bestState.vehicleModel ||
        left.bestState.playMode != right.bestState.playMode ||
        left.bestState.accelerate != right.bestState.accelerate ||
        left.bestState.brake != right.bestState.brake ||
        left.bestState.steering != right.bestState.steering ||
        left.bestState.checkpointsCollected !=
                right.bestState.checkpointsCollected ||
        left.bestState.checkpointsTotal !=
                right.bestState.checkpointsTotal ||
        left.bestState.completedLaps != right.bestState.completedLaps ||
        left.bestState.totalLaps != right.bestState.totalLaps ||
        left.bestState.raceCompleted != right.bestState.raceCompleted ||
        left.bestState.finishTimeMs != right.bestState.finishTimeMs ||
        left.bestState.respawnCount != right.bestState.respawnCount ||
        left.bestState.stuntsScore != right.bestState.stuntsScore) {
        return false;
    }
    for (std::size_t index = 0u; index < left.bestInputs.size(); ++index) {
        if (!SameInput(left.bestInputs[index], right.bestInputs[index])) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main(int argc, char **argv) {
    using namespace forevervalidator;
    using namespace forevervalidator::experimental;

    if (argc < 6 || argc > 9) {
        return Fail(
                "usage: PACKS REPLAY CANDIDATES TIMELINE_TICKS "
                "REPETITIONS [BRANCH_TIME_MS] "
                "[random-steering|existing-event|smooth-steering|"
                "input-insertion|dense-insertion|input-deletion] "
                "[optimized|legacy|differential]");
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
            argc >= 8 ? argv[7] : "random-steering";
    const std::string pipeline =
            argc == 9 ? argv[8] : "optimized";
    if (modifier != "random-steering" &&
        modifier != "existing-event" &&
        modifier != "smooth-steering" &&
        modifier != "input-insertion" &&
        modifier != "dense-insertion" &&
        modifier != "input-deletion") {
        return Fail("unknown modifier");
    }
    if (pipeline != "optimized" &&
        pipeline != "legacy" &&
        pipeline != "differential") {
        return Fail("unknown mutation pipeline");
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
    configuration.maximumBatchSize =
            pipeline == "differential" ? 1u : candidateCount;
    configuration.earliestMutationTimeMs = firstTickTimeMs;
    configuration.evaluationStartTimeMs = firstTickTimeMs;
    configuration.evaluationEndTimeMs = evaluationEndTimeMs;
    if (modifier == "random-steering") {
        configuration.modifiers.push_back(
                PhysicsSandboxCudaRandomSteeringModifier{
                        {firstTickTimeMs,
                         evaluationEndTimeMs,
                         0x6d2b79f5u}});
    } else if (modifier == "existing-event") {
        PhysicsSandboxCudaExistingEventModifier existing;
        existing.window = {
                firstTickTimeMs,
                evaluationEndTimeMs,
                0x6d2b79f5u};
        existing.minimumCount = 1u;
        existing.maximumCount = 16u;
        existing.maximumTimeShiftMs = 100;
        existing.steeringDeltaMinimum = -4096;
        existing.steeringDeltaMaximum = 4096;
        existing.toggleAccelerate = true;
        existing.toggleBrake = true;
        configuration.modifiers.push_back(existing);
    } else if (modifier == "smooth-steering") {
        PhysicsSandboxCudaSmoothSteeringModifier smooth;
        smooth.window = {
                firstTickTimeMs,
                evaluationEndTimeMs,
                0x6d2b79f5u};
        smooth.deformationCount = 8u;
        smooth.radiusMs = 100;
        smooth.amplitudeMinimum = -8192;
        smooth.amplitudeMaximum = 8192;
        configuration.modifiers.push_back(smooth);
    } else if (modifier == "input-insertion" ||
               modifier == "dense-insertion") {
        PhysicsSandboxCudaInputInsertionModifier insertion;
        insertion.window = {
                firstTickTimeMs,
                evaluationEndTimeMs,
                0x6d2b79f5u};
        insertion.steering.enabled = true;
        const bool dense = modifier == "dense-insertion";
        insertion.steering.minimumCount = dense ? 16u : 1u;
        insertion.steering.maximumCount = dense ? 16u : 1u;
        insertion.steering.maximumHoldMs = dense ? 100 : 0;
        insertion.accelerate.enabled = dense;
        insertion.accelerate.minimumCount = dense ? 16u : 0u;
        insertion.accelerate.maximumCount = dense ? 16u : 0u;
        insertion.accelerate.maximumHoldMs = dense ? 100 : 0;
        insertion.brake = insertion.accelerate;
        insertion.steeringOffset = true;
        insertion.steeringOffsetMinimum = dense ? -4096 : 1;
        insertion.steeringOffsetMaximum = dense ? 4096 : 1;
        configuration.modifiers.push_back(insertion);
    } else {
        PhysicsSandboxCudaInputDeletionModifier deletion;
        deletion.window = {
                firstTickTimeMs,
                evaluationEndTimeMs,
                0x6d2b79f5u};
        deletion.steering.enabled = true;
        deletion.steering.maximumCount = 16u;
        deletion.accelerate.enabled = true;
        deletion.accelerate.maximumCount = 16u;
        deletion.brake.enabled = true;
        deletion.brake.maximumCount = 16u;
        configuration.modifiers.push_back(deletion);
    }
    configuration.evaluator = PhysicsSandboxCudaVelocityEvaluator{};
    configuration.useLegacyMutationPipelineForTesting =
            pipeline == "legacy";

    auto session = CreatePhysicsSandboxCudaSearchSession(
            sandbox.Value(), configuration);
    if (!session) {
        return Fail("could not create CUDA search session: " +
                    Diagnostic(session.Error()));
    }
    std::optional<PhysicsSandboxCudaSearchSession> legacySession;
    if (pipeline == "differential") {
        configuration.useLegacyMutationPipelineForTesting = true;
        auto created = CreatePhysicsSandboxCudaSearchSession(
                sandbox.Value(), configuration);
        if (!created) {
            return Fail("could not create legacy CUDA search session: " +
                        Diagnostic(created.Error()));
        }
        legacySession.emplace(std::move(created).Value());
        auto optimizedCapacity =
                session.Value().ReserveBatchCapacity(candidateCount);
        auto legacyCapacity =
                legacySession->ReserveBatchCapacity(candidateCount);
        if (!optimizedCapacity || !legacyCapacity ||
            optimizedCapacity.Value() != candidateCount ||
            legacyCapacity.Value() != candidateCount) {
            return Fail(
                    "optimized and legacy CUDA capacity growth differs");
        }
    }
    auto baseline = session.Value().EvaluateBaseline();
    if (!baseline) {
        return Fail("could not evaluate baseline: " +
                    Diagnostic(baseline.Error()));
    }
    if (legacySession.has_value()) {
        auto legacyBaseline = legacySession->EvaluateBaseline();
        if (!legacyBaseline ||
            !SameBatch(baseline.Value(), legacyBaseline.Value())) {
            return Fail("optimized and legacy CUDA baselines differ");
        }
        auto cancelled = session.Value().RunBatch(
                0u, candidateCount, true);
        auto legacyCancelled = legacySession->RunBatch(
                0u, candidateCount, true);
        if (!cancelled || !legacyCancelled ||
            !cancelled.Value().cancelled ||
            !SameBatch(cancelled.Value(), legacyCancelled.Value())) {
            if (cancelled && legacyCancelled) {
                const auto &optimized = cancelled.Value();
                const auto &legacy = legacyCancelled.Value();
                std::cerr
                        << "optimized cancellation: evaluated="
                        << optimized.evaluatedCandidateCount
                        << " evaluator_calls=" << optimized.evaluatorCalls
                        << " mutations=" << optimized.totalMutationCount
                        << " improvements="
                        << optimized.mutationImprovementCount
                        << " best_changed=" << optimized.bestChanged
                        << " best_candidate="
                        << optimized.bestCandidateId.value_or(UINT64_MAX)
                        << '\n'
                        << "legacy cancellation: evaluated="
                        << legacy.evaluatedCandidateCount
                        << " evaluator_calls=" << legacy.evaluatorCalls
                        << " mutations=" << legacy.totalMutationCount
                        << " improvements="
                        << legacy.mutationImprovementCount
                        << " best_changed=" << legacy.bestChanged
                        << " best_candidate="
                        << legacy.bestCandidateId.value_or(UINT64_MAX)
                        << '\n';
            } else {
                if (!cancelled) {
                    std::cerr << "optimized cancellation error: "
                              << Diagnostic(cancelled.Error()) << '\n';
                }
                if (!legacyCancelled) {
                    std::cerr << "legacy cancellation error: "
                              << Diagnostic(legacyCancelled.Error()) << '\n';
                }
            }
            return Fail(
                    "optimized and legacy CUDA cancellation differs");
        }
        auto boundary = session.Value().RunBatch(UINT64_MAX, 1u, false);
        auto legacyBoundary =
                legacySession->RunBatch(UINT64_MAX, 1u, false);
        if (!boundary || !legacyBoundary ||
            !SameBatch(boundary.Value(), legacyBoundary.Value())) {
            return Fail(
                    "optimized and legacy CUDA candidate-id boundary differs");
        }
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
        if (legacySession.has_value()) {
            auto legacyBatch = legacySession->RunBatch(
                    firstCandidateId, candidateCount, false);
            if (!legacyBatch ||
                !SameBatch(batch.Value(), legacyBatch.Value())) {
                return Fail(
                        "optimized and legacy CUDA mutation batches differ");
            }
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
                  << "\"best_candidate_id\":"
                  << batch.Value().bestCandidateId.value_or(UINT64_MAX)
                  << ",\"best_is_mutation\":"
                  << (batch.Value().bestIsMutation ? "true" : "false")
                  << ",\"best_score\":"
                  << batch.Value().bestScore
                  << ",\"best_time_ms\":"
                  << batch.Value().bestTimeMs
                  << ",\"best_detail_0\":"
                  << batch.Value().bestDetail0
                  << ",\"best_detail_1\":"
                  << batch.Value().bestDetail1 << ","
                  << "\"timeline_ticks\":" << timelineTicks << ","
                  << "\"branch_time_ms\":" << branchTimeMs << ","
                  << "\"modifier\":\"" << modifier << "\","
                  << "\"mutation_pipeline\":\"" << pipeline << "\","
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
                  << ",\"mutation_device_bytes\":"
                  << batch.Value().metrics.mutationDeviceBytes
                  << ",\"candidate_input_device_bytes\":"
                  << batch.Value().metrics.candidateInputDeviceBytes
                  << ",\"mutation_scratch_device_bytes\":"
                  << batch.Value().metrics.mutationScratchDeviceBytes
                  << ",\"host_to_device_bytes\":"
                  << batch.Value().metrics.hostToDeviceBytes
                  << ",\"device_to_host_bytes\":"
                  << batch.Value().metrics.deviceToHostBytes
                  << ",\"initial_host_to_device_bytes\":"
                  << baseline.Value().metrics.hostToDeviceBytes
                  << ",\"baseline_device_to_host_bytes\":"
                  << baseline.Value().metrics.deviceToHostBytes
                  << "}\n";
        firstCandidateId += candidateCount;
    }
    return 0;
}
