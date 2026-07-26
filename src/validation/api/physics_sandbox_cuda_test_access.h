#ifndef FOREVERVALIDATOR_PHYSICS_SANDBOX_CUDA_TEST_ACCESS_H
#define FOREVERVALIDATOR_PHYSICS_SANDBOX_CUDA_TEST_ACCESS_H

#include <forevervalidator/experimental/physics_sandbox.h>

#include "simulation/runtime/replay_simulation_session.h"

namespace forevervalidator::experimental::cuda_test {

// Internal differential hook. It is intentionally unavailable through the
// supported public sandbox API.
struct PhysicsSandboxCudaTestAccess {
    static ReplayCudaVehiclePrefixDifferential RunVehiclePrefix(
            PhysicsSandbox &sandbox,
            float dt);
    static ReplayCudaVehiclePrefixDifferential RunVehicleForce(
            PhysicsSandbox &sandbox,
            float dt);
    static ReplayCudaVehiclePrefixDifferential RunCollision(
            PhysicsSandbox &sandbox);
    static ReplayCudaVehiclePrefixDifferential RunPhysicsStep(
            PhysicsSandbox &sandbox);
    static ReplayCudaVehiclePrefixDifferential RunCollisionSubstep(
            PhysicsSandbox &sandbox,
            float dt);
    static ReplayCudaVehiclePrefixDifferential RunPreCollision(
            PhysicsSandbox &sandbox,
            float dt);
    static ReplayCudaVehiclePrefixDifferential RunNextTimelineTick(
            PhysicsSandbox &sandbox);
    static bool StageNextTimelinePrefix(PhysicsSandbox &sandbox);
    static bool StageCollisionSubstep(
            PhysicsSandbox &sandbox,
            float dt);
    static bool StagePreCollision(
            PhysicsSandbox &sandbox,
            float dt = 0.0f);
    static std::string Diagnostic(
            const PhysicsSandbox &sandbox);
    static forevervalidator::simulation::CudaTimelineBatchResult
            RunCandidateBatch(
                    PhysicsSandbox &sandbox,
                    std::uint32_t candidateCount,
                    std::uint32_t tickCount,
                    bool mutateControls,
                    bool cancellationRequested = false);
    static ReplayCudaVehiclePrefixDifferential
            RunCandidateBatchDifferential(
                    PhysicsSandbox &sandbox,
                    std::uint32_t candidateCount,
                    std::uint32_t tickCount,
                    bool mutateControls);
    static std::optional<forevervalidator::simulation::
                                 CudaSceneTransferMetrics>
            SceneTransfer(const PhysicsSandbox &sandbox);
    static std::optional<forevervalidator::simulation::
                                 CudaStaticConfigurationTransferMetrics>
            ConfigurationTransfer(const PhysicsSandbox &sandbox);
    static std::optional<forevervalidator::simulation::CudaCandidateState>
            CaptureCandidateState(const PhysicsSandbox &sandbox);
    static std::size_t TimelineSize(
            const PhysicsSandbox &sandbox) noexcept;
    static std::size_t Cursor(
            const PhysicsSandbox &sandbox) noexcept;
};

}  // namespace forevervalidator::experimental::cuda_test

#endif
