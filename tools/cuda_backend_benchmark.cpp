#include <forevervalidator/experimental/physics_sandbox.h>
#include <forevervalidator/native.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <future>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "validation/api/physics_sandbox_cuda_test_access.h"

namespace {

using Clock = std::chrono::steady_clock;

double Milliseconds(Clock::time_point begin, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

int Fail(const std::string &message) {
    std::cerr << "cuda_backend_benchmark: " << message << '\n';
    return 1;
}

}  // namespace

int main(int argc, char **argv) {
    using namespace forevervalidator;
    using namespace forevervalidator::experimental;
    using Access = cuda_test::PhysicsSandboxCudaTestAccess;
    if (argc < 3 || argc > 6) {
        return Fail(
                "usage: PACKS REPLAY [CUDA_CANDIDATES] [TICKS] "
                "[CPU_CANDIDATES]");
    }
    const std::uint32_t candidateCount =
            argc >= 4 ? static_cast<std::uint32_t>(std::stoul(argv[3]))
                      : 16u;
    const std::uint32_t tickCount =
            argc >= 5 ? static_cast<std::uint32_t>(std::stoul(argv[4]))
                      : 1000u;
    const std::uint32_t defaultCpuCandidates = std::max(
            1u, std::min(
                        candidateCount,
                        std::thread::hardware_concurrency()));
    const std::uint32_t cpuCandidateCount =
            argc >= 6 ? static_cast<std::uint32_t>(std::stoul(argv[5]))
                      : defaultCpuCandidates;
    if (candidateCount == 0u || tickCount == 0u ||
        cpuCandidateCount == 0u) {
        return Fail("candidate and tick counts must be positive");
    }
    auto replay = ReadNativeReplayFile(
            argv[2], ReplayIdentity{argv[2]});
    if (!replay) return Fail("could not read replay");

    PhysicsSandboxOptions options;
    options.backend = SimulationBackend::Cuda;
    auto cudaSource = OpenInstalledPackDirectory(argv[1]);
    if (!cudaSource) return Fail("could not open CUDA pack source");
    const Clock::time_point cudaInitBegin = Clock::now();
    auto cudaSandbox = CreatePhysicsSandbox(
            std::move(cudaSource).Value(), options);
    if (!cudaSandbox) return Fail("could not create CUDA sandbox");
    auto cudaLoaded = cudaSandbox.Value().LoadReplay(
            {replay.Value().data(), replay.Value().size()},
            ReplayIdentity{argv[2]});
    const Clock::time_point cudaInitEnd = Clock::now();
    if (!cudaLoaded) {
        return Fail("CUDA initialization failed: " +
                    cudaLoaded.Error().diagnostic);
    }

    const Clock::time_point cudaBatchBegin = Clock::now();
    const auto cudaBatch = Access::RunCandidateBatch(
            cudaSandbox.Value(), candidateCount, tickCount, true);
    const Clock::time_point cudaBatchEnd = Clock::now();
    if (cudaBatch.status !=
        simulation::CudaTimelineStatus::Success) {
        return Fail(cudaBatch.diagnostic);
    }
    const auto sceneTransfer =
            Access::SceneTransfer(cudaSandbox.Value());
    const auto configurationTransfer =
            Access::ConfigurationTransfer(cudaSandbox.Value());

    options.backend = SimulationBackend::Batched;
    const Clock::time_point cpuInitBegin = Clock::now();
    std::vector<PhysicsSandbox> cpuSandboxes;
    std::vector<PhysicsSandbox *> cpuPointers;
    cpuSandboxes.reserve(cpuCandidateCount);
    cpuPointers.reserve(cpuCandidateCount);
    for (std::uint32_t index = 0u;
         index < cpuCandidateCount; ++index) {
        auto source = OpenInstalledPackDirectory(argv[1]);
        if (!source) return Fail("could not open CPU pack source");
        auto sandbox = CreatePhysicsSandbox(
                std::move(source).Value(), options);
        if (!sandbox) return Fail("could not create CPU sandbox");
        auto loaded = sandbox.Value().LoadReplay(
                {replay.Value().data(), replay.Value().size()},
                ReplayIdentity{argv[2]});
        if (!loaded) {
            return Fail("CPU candidate initialization failed");
        }
        cpuSandboxes.push_back(std::move(sandbox).Value());
    }
    for (PhysicsSandbox &sandbox : cpuSandboxes) {
        cpuPointers.push_back(&sandbox);
    }
    const Clock::time_point cpuInitEnd = Clock::now();
    const Clock::time_point cpuBatchBegin = Clock::now();
    std::vector<std::future<
            PhysicsSandboxResult<PhysicsSandboxStateView>>> cpuTasks;
    cpuTasks.reserve(cpuPointers.size());
    for (PhysicsSandbox *sandbox : cpuPointers) {
        cpuTasks.push_back(std::async(
                std::launch::async,
                [sandbox, tickCount]() {
                    return sandbox->AdvanceTicks(tickCount);
                }));
    }
    std::vector<PhysicsSandboxResult<PhysicsSandboxStateView>>
            cpuResults;
    cpuResults.reserve(cpuTasks.size());
    for (auto &task : cpuTasks) {
        cpuResults.push_back(task.get());
    }
    const Clock::time_point cpuBatchEnd = Clock::now();
    if (cpuResults.size() != cpuCandidateCount) {
        return Fail("parallel CPU batch did not return every candidate");
    }
    for (const auto &candidate : cpuResults) {
        if (!candidate) return Fail("parallel CPU candidate failed");
    }

    const double cudaWallMs =
            Milliseconds(cudaBatchBegin, cudaBatchEnd);
    const double cpuWallMs =
            Milliseconds(cpuBatchBegin, cpuBatchEnd);
    const double totalTicks =
            static_cast<double>(candidateCount) * tickCount;
    const double totalCpuTicks =
            static_cast<double>(cpuCandidateCount) * tickCount;
    const double cudaTicksPerSecond =
            totalTicks * 1000.0 / cudaWallMs;
    const double cpuTicksPerSecond =
            totalCpuTicks * 1000.0 / cpuWallMs;
    const auto &metrics = cudaBatch.metrics;
    const std::uint64_t immutableDeviceBytes =
            sceneTransfer.value_or(
                    simulation::CudaSceneTransferMetrics{}).deviceBytes +
            configurationTransfer.value_or(
                    simulation::
                            CudaStaticConfigurationTransferMetrics{}).
                    deviceBytes;

    std::cout << std::fixed << std::setprecision(3)
              << "{"
              << "\"candidates\":" << candidateCount << ","
              << "\"ticks_per_candidate\":" << tickCount << ","
              << "\"parallel_cpu_candidates\":"
              << cpuCandidateCount << ","
              << "\"cuda_initialization_ms\":"
              << Milliseconds(cudaInitBegin, cudaInitEnd) << ","
              << "\"cuda_scene_pack_ms\":"
              << sceneTransfer.value_or(
                         simulation::CudaSceneTransferMetrics{}).
                         packMilliseconds << ","
              << "\"cuda_scene_upload_ms\":"
              << sceneTransfer.value_or(
                         simulation::CudaSceneTransferMetrics{}).
                         uploadMilliseconds << ","
              << "\"cuda_configuration_pack_ms\":"
              << configurationTransfer.value_or(
                         simulation::
                                 CudaStaticConfigurationTransferMetrics{}).
                         packMilliseconds << ","
              << "\"cuda_configuration_upload_ms\":"
              << configurationTransfer.value_or(
                         simulation::
                                 CudaStaticConfigurationTransferMetrics{}).
                         uploadMilliseconds << ","
              << "\"cuda_batch_wall_ms\":" << cudaWallMs << ","
              << "\"cuda_allocation_ms\":"
              << metrics.allocationMilliseconds << ","
              << "\"cuda_transfer_ms\":"
              << metrics.transferMilliseconds << ","
              << "\"cuda_kernel_ms\":" << metrics.kernelMilliseconds << ","
              << "\"cuda_synchronization_ms\":"
              << metrics.synchronizationMilliseconds << ","
              << "\"cuda_host_to_device_bytes\":"
              << metrics.hostToDeviceBytes << ","
              << "\"cuda_device_to_host_bytes\":"
              << metrics.deviceToHostBytes << ","
              << "\"cuda_immutable_device_bytes\":"
              << immutableDeviceBytes << ","
              << "\"cuda_peak_timeline_device_bytes\":"
              << metrics.peakDeviceBytes << ","
              << "\"cuda_ticks_per_second\":"
              << cudaTicksPerSecond << ","
              << "\"cuda_candidates_per_second\":"
              << static_cast<double>(candidateCount) * 1000.0 /
                         cudaWallMs << ","
              << "\"winner_candidate_index\":"
              << cudaBatch.winnerCandidateIndex.value_or(SIZE_MAX) << ","
              << "\"winner_candidate_id\":"
              << cudaBatch.winnerCandidateId.value_or(UINT32_MAX) << ","
              << "\"parallel_cpu_initialization_ms\":"
              << Milliseconds(cpuInitBegin, cpuInitEnd) << ","
              << "\"parallel_cpu_batch_wall_ms\":" << cpuWallMs << ","
              << "\"parallel_cpu_ticks_per_second\":"
              << cpuTicksPerSecond << ","
              << "\"parallel_cpu_candidates_per_second\":"
              << static_cast<double>(cpuCandidateCount) * 1000.0 /
                         cpuWallMs << ","
              << "\"cuda_speedup_over_parallel_cpu\":"
              << cudaTicksPerSecond / cpuTicksPerSecond
              << "}\n";
    return 0;
}
