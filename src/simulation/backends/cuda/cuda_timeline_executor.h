#ifndef FOREVERVALIDATOR_CUDA_TIMELINE_EXECUTOR_H
#define FOREVERVALIDATOR_CUDA_TIMELINE_EXECUTOR_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "simulation/backends/cuda/cuda_state_layout.h"

namespace forevervalidator::simulation {

struct CudaControlTick {
    std::uint32_t periodMs = 0u;
    std::uint32_t timeMs = 0u;
    std::uint32_t actionFlags = 0u;
    std::uint32_t stuntsTimeLimitMs = 0u;
    std::uint32_t respawnAtCheckpointCount = 0u;
    ReplayVehicleControlState controls{};
    ReplayStuntInputState stuntsInput{};
    bool observe = false;
    bool hasComparisonTarget = false;
    GmVec3 comparisonTarget{};
};

enum CudaControlActionFlag : std::uint32_t {
    CudaControlActionEstablishRaceSpawn = 1u << 0u,
    CudaControlActionSuppressVehicleForceCallbacks = 1u << 1u,
    CudaControlActionEnableRaceSimulation = 1u << 2u,
    CudaControlActionResetAtRaceStart = 1u << 3u,
    CudaControlActionEnableStuntsSimulation = 1u << 4u,
    CudaControlActionFinishRace = 1u << 5u,
};

struct CudaTimelineObservation {
    GmVec3 simulatedPosition{};
    GmVec3 writePosition{};
    bool hasComparison = false;
    GmVec3 comparisonTarget{};
    GmVec3 comparisonDelta{};
    float comparisonDistance = 0.0f;
    bool hasFinishTick = false;
    std::uint32_t finishTickMs = 0u;
};

enum class CudaTimelineStatus : std::uint32_t {
    Success,
    InvalidArgument,
    SchemaMismatch,
    CapacityExceeded,
    Cancelled,
    DeviceFailure,
    UnsupportedPhysicsTransition,
};

struct CudaCandidateTimelineInput {
    const void *deviceScene = nullptr;
    const void *deviceStaticConfiguration = nullptr;
    CudaCandidateState initialState{};
    std::vector<CudaControlTick> ticks;
};

struct CudaCandidateTimelineOutput {
    CudaTimelineStatus status = CudaTimelineStatus::InvalidArgument;
    std::uint32_t failureTick = UINT32_MAX;
    std::uint32_t failureDetail = 0u;
    std::uint32_t executedTickCount = 0u;
    std::uint32_t executedRespawnCount = 0u;
    CudaCandidateState finalState{};
    std::vector<CudaTimelineObservation> observations;
};

struct CudaTimelineExecutionMetrics {
    std::uint64_t candidateCount = 0u;
    std::uint64_t tickCount = 0u;
    std::uint64_t observationCapacity = 0u;
    std::uint64_t hostToDeviceBytes = 0u;
    std::uint64_t deviceToHostBytes = 0u;
    std::uint64_t peakDeviceBytes = 0u;
    double allocationMilliseconds = 0.0;
    double transferMilliseconds = 0.0;
    double kernelMilliseconds = 0.0;
    double synchronizationMilliseconds = 0.0;
};

struct CudaTimelineBatchResult {
    CudaTimelineStatus status = CudaTimelineStatus::InvalidArgument;
    std::vector<CudaCandidateTimelineOutput> candidates;
    CudaTimelineExecutionMetrics metrics{};
    std::optional<std::size_t> winnerCandidateIndex;
    std::optional<std::uint32_t> winnerCandidateId;
    std::string diagnostic;
};

CudaControlTick FlattenCudaControlTick(
        const ReplayControlTick &source) noexcept;

CudaTimelineBatchResult ExecuteCudaTimelineBatch(
        const void *deviceScene,
        const void *deviceStaticConfiguration,
        const std::vector<CudaCandidateTimelineInput> &candidates,
        bool cancellationRequested = false) noexcept;

CudaTimelineBatchResult ExecuteCudaReplayTimelineBatch(
        const std::vector<CudaCandidateTimelineInput> &replays,
        bool cancellationRequested = false) noexcept;

const char *CudaTimelineStatusName(CudaTimelineStatus status) noexcept;

std::optional<std::size_t> SelectCudaTimelineWinner(
        const std::vector<CudaCandidateTimelineOutput> &candidates) noexcept;

}  // namespace forevervalidator::simulation

#endif
