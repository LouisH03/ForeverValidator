#ifndef FOREVERVALIDATOR_CUDA_PHYSICS_STEP_CUH
#define FOREVERVALIDATOR_CUDA_PHYSICS_STEP_CUH

#include "simulation/backends/cuda/cuda_collision_response.cuh"
#include "simulation/backends/cuda/cuda_dynamics.cuh"
#include "simulation/backends/cuda/cuda_environment.cuh"
#include "simulation/backends/cuda/cuda_vehicle_after_contacts.cuh"
#include "simulation/backends/cuda/cuda_vehicle_forces.cuh"

namespace forevervalidator::simulation::cuda::physics {

enum class Status : std::uint32_t {
    Success,
    UnsupportedVehicleForce,
    CollisionFailure,
    UnsupportedForceBase = 100u,
};

template <bool ReuseSteeringSinCos = false>
__device__ inline vehicle::ForceStatus ForcePass(
        CudaCandidatePhysicsState &candidate,
        const CudaPackedStaticConfigurationHeader *configuration,
        float dt) {
    environment::BeginForcePass(candidate.body, configuration);
    if (!candidate.vehicle.mobil.physicsUpdatesEnabled) {
        return vehicle::ForceStatus::Success;
    }
    return vehicle::ComputeForcesModel6<
            ReuseSteeringSinCos>(
            candidate, configuration, dt);
}

template <
        bool TrackCollisionDiagnostics = true,
        bool ReuseSteeringSinCos = false,
        typename Scratch = collision::CudaCollisionScratch>
__device__ inline Status CollisionSubstep(
        const CudaPackedSceneHeader *scene,
        const CudaPackedStaticConfigurationHeader *configuration,
        CudaCandidatePhysicsState &candidate,
        float dt,
        Scratch &scratch) {
    const vehicle::ForceStatus forceStatus =
            ForcePass<ReuseSteeringSinCos>(
                    candidate, configuration, dt);
    if (forceStatus != vehicle::ForceStatus::Success) {
        return static_cast<Status>(
                static_cast<std::uint32_t>(
                        Status::UnsupportedForceBase) +
                static_cast<std::uint32_t>(forceStatus));
    }
    dynamics::PreCollision(candidate.body, dt);
    collision::Status collisionStatus =
            collision::Detect<TrackCollisionDiagnostics>(
                    scene, configuration, candidate, scratch);
    if (collisionStatus == collision::Status::Success) {
        collisionStatus =
                collision::Respond<TrackCollisionDiagnostics>(
                        scene, configuration, candidate, scratch);
    }
    if (collisionStatus != collision::Status::Success) {
        return Status::CollisionFailure;
    }
    dynamics::PostCollision(candidate.body);
    return Status::Success;
}

template <
        bool TrackCollisionDiagnostics = true,
        bool ReuseSteeringSinCos = false,
        typename Scratch = collision::CudaCollisionScratch>
__device__ inline Status Step(
        const CudaPackedSceneHeader *scene,
        const CudaPackedStaticConfigurationHeader *configuration,
        CudaCandidatePhysicsState &candidate,
        Scratch &scratch) {
    const float dt =
            __int2float_rn(static_cast<std::int32_t>(
                    candidate.world.schemePeriodMs)) *
            0.001f;
    // StadiumCar is registered in the dynamic collision group, so the
    // authoritative zone excludes it from the initial ungrouped force pass.
    if (candidate.body.dynamicActive) {
        candidate.body.temporary = candidate.body.current;
        const GmVec3 &linear =
                candidate.body.current.linearSpeed;
        const GmVec3 &angular =
                candidate.body.current.angularSpeed;
        const float linearLength = exact::Sqrt(
                (linear.y * linear.y + linear.x * linear.x) +
                linear.z * linear.z);
        const float angularLength = exact::Sqrt(
                (angular.x * angular.x +
                 angular.y * angular.y) +
                angular.z * angular.z);
        const float scaled =
                ((linearLength + angularLength) * dt) /
                candidate.body.parameters.maxStepDistance;
        std::uint32_t substeps =
                exact::TruncateToUint32Modulo(scaled) + 1u;
        if (substeps > 1000u) substeps = 1000u;
        float remaining = dt;
        if (substeps > 1u) {
            const float split =
                    dt / exact::FromUnsignedInteger(substeps);
            for (std::uint32_t count = substeps - 1u;
                 count != 0u; --count) {
                const Status status =
                        CollisionSubstep<
                                TrackCollisionDiagnostics,
                                ReuseSteeringSinCos>(
                                scene, configuration, candidate,
                                split, scratch);
                if (status != Status::Success) return status;
                remaining -= split;
            }
        }
        const Status finalStatus =
                CollisionSubstep<
                        TrackCollisionDiagnostics,
                        ReuseSteeringSinCos>(
                        scene, configuration, candidate,
                        remaining, scratch);
        if (finalStatus != Status::Success) {
            return finalStatus;
        }
        candidate.body.write = candidate.body.temporary;
    }
    if (candidate.vehicle.mobil.physicsUpdatesEnabled) {
        vehicle::AfterContacts(candidate, configuration);
    }
    return Status::Success;
}

}  // namespace forevervalidator::simulation::cuda::physics

#endif
