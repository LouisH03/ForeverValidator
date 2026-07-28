#include <forevervalidator/experimental/physics_sandbox.h>
#include <forevervalidator/native.h>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
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

constexpr std::uint64_t FnvOffset = 1469598103934665603ull;
constexpr std::uint64_t FnvPrime = 1099511628211ull;

template<typename T>
void HashValue(std::uint64_t &hash, const T &value) {
    const auto *bytes =
            reinterpret_cast<const unsigned char *>(&value);
    for (std::size_t index = 0u; index < sizeof(value); ++index) {
        hash ^= bytes[index];
        hash *= FnvPrime;
    }
}

template<typename T>
void HashOptional(
        std::uint64_t &hash,
        const std::optional<T> &value) {
    const bool present = value.has_value();
    HashValue(hash, present);
    if (present) {
        HashValue(hash, *value);
    }
}

std::uint64_t StateFingerprint(
        const forevervalidator::experimental::
                PhysicsSandboxStateView &view) {
    std::uint64_t hash = FnvOffset;
    HashValue(hash, view.tick);
    HashValue(hash, view.timeMs);
    HashValue(hash, view.mapEnvironment);
    HashValue(hash, view.vehicleModel);
    HashOptional(hash, view.playMode);
    HashValue(hash, view.car.rotationX);
    HashValue(hash, view.car.rotationY);
    HashValue(hash, view.car.rotationZ);
    HashValue(hash, view.car.rotationW);
    HashValue(hash, view.car.position.x);
    HashValue(hash, view.car.position.y);
    HashValue(hash, view.car.position.z);
    HashValue(hash, view.car.linearSpeed.x);
    HashValue(hash, view.car.linearSpeed.y);
    HashValue(hash, view.car.linearSpeed.z);
    HashValue(hash, view.car.angularSpeed.x);
    HashValue(hash, view.car.angularSpeed.y);
    HashValue(hash, view.car.angularSpeed.z);
    HashValue(hash, view.car.force.x);
    HashValue(hash, view.car.force.y);
    HashValue(hash, view.car.force.z);
    HashValue(hash, view.car.torque.x);
    HashValue(hash, view.car.torque.y);
    HashValue(hash, view.car.torque.z);
    HashValue(hash, view.accelerate);
    HashValue(hash, view.brake);
    HashValue(hash, view.steering);
    HashValue(hash, view.checkpointsCollected);
    HashValue(hash, view.checkpointsTotal);
    HashValue(hash, view.completedLaps);
    HashValue(hash, view.totalLaps);
    HashValue(hash, view.raceCompleted);
    HashOptional(hash, view.finishTimeMs);
    HashValue(hash, view.respawnCount);
    HashOptional(hash, view.stuntsScore);
    return hash;
}

std::uint64_t InputFingerprint(
        const std::vector<
                forevervalidator::experimental::
                        PhysicsSandboxInputEvent> &inputs) {
    std::uint64_t hash = FnvOffset;
    for (const auto &input : inputs) {
        HashValue(hash, input.timeMs);
        HashValue(hash, input.action);
        HashValue(hash, input.value.kind);
        HashValue(hash, input.value.switchState);
        HashValue(hash, input.value.analog);
    }
    return hash;
}

}  // namespace

int main(int argc, char **argv) {
    using namespace forevervalidator;
    using namespace forevervalidator::experimental;

    if (argc < 6 || argc > 9) {
        return Fail(
                "usage: PACKS REPLAY CANDIDATES TIMELINE_TICKS "
                "REPETITIONS [BRANCH_TIME_MS] "
                "[random-steering|input-insertion|cancelled] "
                "[velocity|point|pose|volume-entry|finish-time]");
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
    const std::string evaluatorName =
            argc == 9 ? argv[8] : "velocity";
    if (modifier != "random-steering" &&
        modifier != "input-insertion" &&
        modifier != "cancelled") {
        return Fail("unknown modifier");
    }
    if (evaluatorName != "velocity" &&
        evaluatorName != "point" &&
        evaluatorName != "pose" &&
        evaluatorName != "volume-entry" &&
        evaluatorName != "finish-time") {
        return Fail("unknown evaluator");
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
    PhysicsSandboxStateView branchState = loaded.Value();
    if (advanceTicks != 0u) {
        auto advanced = sandbox.Value().AdvanceTicks(
                static_cast<std::uint32_t>(advanceTicks));
        if (!advanced) {
            return Fail("could not advance to branch: " +
                        Diagnostic(advanced.Error()));
        }
        branchState = advanced.Value();
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
    if (modifier == "random-steering" || modifier == "cancelled") {
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
    if (evaluatorName == "velocity") {
        configuration.evaluator =
                PhysicsSandboxCudaVelocityEvaluator{};
    } else if (evaluatorName == "point") {
        configuration.evaluator = PhysicsSandboxCudaPointEvaluator{
                {branchState.car.position.x,
                 branchState.car.position.y,
                 branchState.car.position.z}};
    } else if (evaluatorName == "pose") {
        PhysicsSandboxCudaPoseEvaluator evaluator;
        evaluator.targetPosition = {
                branchState.car.position.x,
                branchState.car.position.y,
                branchState.car.position.z};
        evaluator.targetRotationX = branchState.car.rotationX;
        evaluator.targetRotationY = branchState.car.rotationY;
        evaluator.targetRotationZ = branchState.car.rotationZ;
        evaluator.targetRotationW = branchState.car.rotationW;
        configuration.evaluator = evaluator;
    } else if (evaluatorName == "volume-entry") {
        constexpr double Radius = 0.01;
        configuration.evaluator =
                PhysicsSandboxCudaVolumeEntryEvaluator{
                        {branchState.car.position.x - Radius,
                         branchState.car.position.y - Radius,
                         branchState.car.position.z - Radius},
                        {branchState.car.position.x + Radius,
                         branchState.car.position.y + Radius,
                         branchState.car.position.z + Radius}};
    } else {
        configuration.evaluator =
                PhysicsSandboxCudaFinishTimeEvaluator{};
    }

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
        std::uint32_t cancellationProbeCount = 0u;
        auto batch = modifier == "cancelled"
                ? session.Value().RunBatch(
                          firstCandidateId, candidateCount,
                          [&cancellationProbeCount] {
                              return cancellationProbeCount++ >= 30u;
                          })
                : session.Value().RunBatch(
                          firstCandidateId, candidateCount, false);
        if (!batch) {
            return Fail("CUDA search batch failed: " +
                        Diagnostic(batch.Error()));
        }
        if ((modifier == "cancelled" &&
             !batch.Value().cancelled) ||
            (modifier != "cancelled" &&
             (batch.Value().cancelled ||
              batch.Value().evaluatedCandidateCount == 0u))) {
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
        const double ticksPerSecond =
                simulationKernelMilliseconds == 0.0
                ? 0.0
                : simulatedTicks * 1000.0 /
                          simulationKernelMilliseconds;
        const std::optional<std::uint64_t> winningEvaluationTick =
                batch.Value().bestState.timeMs > branchTimeMs
                ? std::optional<std::uint64_t>(
                          (batch.Value().bestState.timeMs -
                           branchTimeMs) /
                                  tickDurationMs -
                          1u)
                : std::nullopt;
        std::cout << std::fixed << std::setprecision(6)
                  << "{"
                  << "\"repetition\":" << repetition << ","
                  << "\"candidates\":" << candidateCount << ","
                  << "\"evaluated_candidates\":"
                  << batch.Value().evaluatedCandidateCount << ","
                  << "\"timeline_ticks\":" << timelineTicks << ","
                  << "\"branch_time_ms\":" << branchTimeMs << ","
                  << "\"modifier\":\"" << modifier << "\","
                  << "\"evaluator\":\"" << evaluatorName << "\","
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
                  << "\"best_changed\":"
                  << (batch.Value().bestChanged ? "true" : "false")
                  << ","
                  << "\"cancelled\":"
                  << (batch.Value().cancelled ? "true" : "false")
                  << ","
                  << "\"best_is_mutation\":"
                  << (batch.Value().bestIsMutation ? "true" : "false")
                  << ","
                  << "\"best_candidate_id\":";
        if (batch.Value().bestCandidateId.has_value()) {
            std::cout << *batch.Value().bestCandidateId;
        } else {
            std::cout << "null";
        }
        std::cout << ","
                  << "\"best_evaluation_tick\":";
        if (winningEvaluationTick.has_value()) {
            std::cout << *winningEvaluationTick;
        } else {
            std::cout << "null";
        }
        std::cout << ","
                  << "\"best_score\":" << batch.Value().bestScore << ","
                  << "\"best_time_ms\":" << batch.Value().bestTimeMs
                  << ","
                  << "\"best_detail0\":" << batch.Value().bestDetail0
                  << ","
                  << "\"best_detail1\":" << batch.Value().bestDetail1
                  << ","
                  << "\"best_mutation_count\":"
                  << batch.Value().bestMutationCount << ","
                  << "\"mutation_improvement_count\":"
                  << batch.Value().mutationImprovementCount << ","
                  << "\"best_state_fingerprint\":"
                  << StateFingerprint(batch.Value().bestState) << ","
                  << "\"best_input_count\":"
                  << batch.Value().bestInputs.size() << ","
                  << "\"best_input_fingerprint\":"
                  << InputFingerprint(batch.Value().bestInputs) << ","
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
                  << ","
                  << "\"winner_selection_device_bytes\":"
                  << batch.Value().metrics
                             .winnerSelectionDeviceBytes
                  << "}\n";
        firstCandidateId += candidateCount;
    }
    return 0;
}
