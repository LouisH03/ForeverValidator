#include "simulation/backends/cuda/cuda_scene_storage.h"
#include "simulation/backends/cuda/cuda_static_configuration.h"
#include "simulation/backends/cuda/cuda_timeline_executor.h"

#include <cuda_runtime_api.h>

#include <cstdint>
#include <iostream>
#include <vector>

namespace {

template<typename T>
class DeviceValue {
public:
    explicit DeviceValue(const T &value) {
        if (cudaMalloc(&data_, sizeof(T)) == cudaSuccess) {
            if (cudaMemcpy(
                        data_, &value, sizeof(T),
                        cudaMemcpyHostToDevice) != cudaSuccess) {
                cudaFree(data_);
                data_ = nullptr;
            }
        }
    }
    ~DeviceValue() {
        if (data_ != nullptr) cudaFree(data_);
    }
    DeviceValue(const DeviceValue &) = delete;
    DeviceValue &operator=(const DeviceValue &) = delete;
    const void *Get() const { return data_; }

private:
    void *data_ = nullptr;
};

}  // namespace

int main() {
    using namespace forevervalidator::simulation;
    std::vector<CudaCandidateTimelineOutput> ranked(3u);
    for (std::size_t index = 0u; index < ranked.size(); ++index) {
        ranked[index].status = CudaTimelineStatus::Success;
        ranked[index].finalState.candidateId =
                static_cast<std::uint32_t>(10u + index);
    }
    ranked[0].finalState.race.progress.completedLapCount = 1u;
    ranked[1].finalState.race.progress.raceCompleted = true;
    ranked[1].finalState.race.progress.completedLapCount = 1u;
    ranked[1].finalState.race.progress.lastPrepareTimeMs = 1200u;
    ranked[2].finalState.race.progress.raceCompleted = true;
    ranked[2].finalState.race.progress.completedLapCount = 1u;
    ranked[2].finalState.race.progress.lastPrepareTimeMs = 1100u;
    if (SelectCudaTimelineWinner(ranked) != 2u) {
        std::cerr << "race candidate winner ordering is not exact\n";
        return 1;
    }
    for (CudaCandidateTimelineOutput &candidate : ranked) {
        candidate.finalState.race.replayPlayMode =
                static_cast<std::uint32_t>(
                        EChallengePlayMode::Stunts);
    }
    ranked[0].finalState.stunts.stuntsScore = 100u;
    ranked[1].finalState.stunts.stuntsScore = 250u;
    ranked[2].finalState.stunts.stuntsScore = 200u;
    if (SelectCudaTimelineWinner(ranked) != 1u) {
        std::cerr << "stunt candidate winner ordering is not exact\n";
        return 1;
    }

    CudaPackedSceneHeader scene;
    scene.totalSize = sizeof(scene);
    CudaPackedStaticConfigurationHeader configuration;
    configuration.totalSize = sizeof(configuration);
    DeviceValue<CudaPackedSceneHeader> deviceScene(scene);
    DeviceValue<CudaPackedStaticConfigurationHeader>
            deviceConfiguration(configuration);
    if (deviceScene.Get() == nullptr ||
        deviceConfiguration.Get() == nullptr) {
        std::cerr << "test device input allocation failed\n";
        return 1;
    }

    ReplayControlTick sourceTick;
    sourceTick.periodMs = 10u;
    sourceTick.timeMs = 1234u;
    sourceTick.controls = {0.25f, 0.75f, -0.5f};
    sourceTick.actions.enableRaceSimulation = true;
    sourceTick.observe = true;
    sourceTick.comparisonTarget = GmVec3{1.0f, 2.0f, 3.0f};
    CudaControlTick tick = FlattenCudaControlTick(sourceTick);
    if ((tick.actionFlags &
         CudaControlActionEnableRaceSimulation) == 0u ||
        !tick.hasComparisonTarget) {
        std::cerr << "control flattening lost timeline inputs\n";
        return 1;
    }

    std::vector<CudaCandidateTimelineInput> batch(256u);
    for (std::size_t index = 0u; index < batch.size(); ++index) {
        batch[index].initialState.candidateId =
                static_cast<std::uint32_t>(index);
        batch[index].initialState.firstStep = false;
        batch[index].ticks.push_back(tick);
    }
    CudaTimelineBatchResult executed = ExecuteCudaTimelineBatch(
            deviceScene.Get(), deviceConfiguration.Get(), batch);
    if (executed.status != CudaTimelineStatus::Success ||
        executed.candidates.size() != batch.size() ||
        executed.metrics.candidateCount != batch.size() ||
        executed.metrics.tickCount != batch.size() ||
        executed.metrics.kernelMilliseconds < 0.0) {
        std::cerr << "batched CUDA timeline launch failed: "
                  << executed.diagnostic << '\n';
        return 1;
    }
    for (std::size_t index = 0u;
         index < executed.candidates.size(); ++index) {
        const CudaCandidateTimelineOutput &candidate =
                executed.candidates[index];
        if (candidate.status != CudaTimelineStatus::Success ||
            candidate.failureTick != UINT32_MAX ||
            candidate.executedTickCount != 1u ||
            candidate.finalState.candidateId != index ||
            candidate.finalState.world.schemePeriodMs != 10u ||
            candidate.finalState.world.tickTimeMs != 1234u ||
            candidate.finalState.vehicle.controls.steeringControl !=
                    -0.5f) {
            std::cerr
                    << "candidate-owned CUDA transition state changed: "
                    << "status="
                    << CudaTimelineStatusName(candidate.status)
                    << " failure_tick=" << candidate.failureTick
                    << " executed=" << candidate.executedTickCount
                    << " candidate_id="
                    << candidate.finalState.candidateId
                    << " period="
                    << candidate.finalState.world.schemePeriodMs
                    << " time="
                    << candidate.finalState.world.tickTimeMs
                    << " steering="
                    << candidate.finalState.vehicle.controls.
                            steeringControl
                    << '\n';
            return 1;
        }
    }

    std::vector<CudaCandidateTimelineInput> largeBatch(4097u);
    for (std::size_t index = 0u; index < largeBatch.size(); ++index) {
        largeBatch[index].initialState.candidateId =
                static_cast<std::uint32_t>(index);
        largeBatch[index].initialState.firstStep = false;
        largeBatch[index].ticks.push_back(tick);
    }
    const CudaTimelineBatchResult large =
            ExecuteCudaTimelineBatch(
                    deviceScene.Get(), deviceConfiguration.Get(),
                    largeBatch);
    if (large.status != CudaTimelineStatus::Success ||
        large.candidates.size() != largeBatch.size() ||
        large.metrics.candidateCount != largeBatch.size() ||
        large.winnerCandidateId != 0u) {
        std::cerr << "CUDA timeline retained an arbitrary 4096-candidate "
                     "limit: "
                  << large.diagnostic << '\n';
        return 1;
    }

    CudaTimelineBatchResult cancelled = ExecuteCudaTimelineBatch(
            deviceScene.Get(), deviceConfiguration.Get(),
            {batch.front()}, true);
    if (cancelled.status != CudaTimelineStatus::Cancelled ||
        cancelled.candidates.size() != 1u ||
        cancelled.candidates[0].failureTick != 0u ||
        cancelled.candidates[0].finalState.world.tickTimeMs != 0u) {
        std::cerr << "CUDA cancellation was not deterministic\n";
        return 1;
    }

    CudaTimelineBatchResult invalid = ExecuteCudaTimelineBatch(
            nullptr, deviceConfiguration.Get(), {batch.front()});
    if (invalid.status != CudaTimelineStatus::InvalidArgument) {
        std::cerr << "invalid CUDA device inputs were not rejected\n";
        return 1;
    }
    CudaCandidateTimelineInput wrongSchema = batch.front();
    ++wrongSchema.initialState.schemaVersion;
    const CudaTimelineBatchResult schema =
            ExecuteCudaTimelineBatch(
                    deviceScene.Get(), deviceConfiguration.Get(),
                    {wrongSchema});
    if (schema.status != CudaTimelineStatus::SchemaMismatch ||
        schema.candidates.size() != 1u ||
        schema.candidates[0].status !=
                CudaTimelineStatus::SchemaMismatch) {
        std::cerr << "CUDA candidate schema mismatch was not explicit\n";
        return 1;
    }
    CudaPackedSceneHeader corruptScene = scene;
    corruptScene.magic = 0u;
    DeviceValue<CudaPackedSceneHeader> deviceCorruptScene(corruptScene);
    const CudaTimelineBatchResult corrupt =
            ExecuteCudaTimelineBatch(
                    deviceCorruptScene.Get(),
                    deviceConfiguration.Get(), {batch.front()});
    if (corrupt.status != CudaTimelineStatus::InvalidArgument ||
        corrupt.candidates.size() != 1u ||
        corrupt.candidates[0].status !=
                CudaTimelineStatus::InvalidArgument) {
        std::cerr << "corrupt CUDA scene header was not rejected\n";
        return 1;
    }
    if (ExecuteCudaTimelineBatch(
                deviceScene.Get(), deviceConfiguration.Get(), {}).
                    status != CudaTimelineStatus::InvalidArgument) {
        std::cerr << "empty CUDA batch was not rejected\n";
        return 1;
    }
    return 0;
}
