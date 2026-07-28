#ifndef FOREVERVALIDATOR_CUDA_COLLISION_CERTIFICATION_H
#define FOREVERVALIDATOR_CUDA_COLLISION_CERTIFICATION_H

#include <string>
#include <vector>

#include "simulation/backends/cuda/cuda_collision_layout.h"
#include "simulation/backends/cuda/cuda_static_configuration.h"
#include "simulation/backends/cuda/cuda_state_layout.h"

namespace forevervalidator::simulation {

struct CudaCollisionExecution {
    bool success = false;
    cuda::collision::Status status =
            cuda::collision::Status::InvalidScene;
    std::vector<cuda::collision::CudaCollision> collisions;
    CudaCandidateState finalState{};
    std::uint32_t accelerationCellVisits = 0u;
    std::uint32_t accelerationSurfaceVisits = 0u;
    std::uint32_t meshCellVisits = 0u;
    std::uint32_t meshCellIntersections = 0u;
    std::uint32_t meshTriangleCells = 0u;
    std::uint32_t triangleTests = 0u;
    std::uint32_t triangleHits = 0u;
    std::uint32_t firstVisitedShape = UINT32_MAX;
    std::uint32_t firstVisitedSurface = UINT32_MAX;
    GmIso4 firstShapeWorld{};
    GmBoxAligned firstMovingBounds{};
    GmBoxAligned firstEllipsoidBox{};
    GmBoxAligned firstSurfaceWorldBounds{};
    GmBoxAligned firstMeshRootBounds{};
    std::uint32_t firstResponseWheelIndex = UINT32_MAX;
    GmVec3 firstResponseReplacementBefore{};
    GmVec3 firstResponseReplacementAfter{};
    std::string diagnostic;
};

CudaCollisionExecution ExecuteCudaCollisionForCertification(
        const void *deviceScene,
        const void *deviceStaticConfiguration,
        const CudaCandidateState &state) noexcept;

struct CudaCollisionOrderingExecution {
    bool success = false;
    std::vector<cuda::collision::CudaCollision> collisions;
    std::string diagnostic;
};

CudaCollisionOrderingExecution
ExecuteCudaCollisionOrderingForCertification(
        const std::vector<cuda::collision::CudaCollision> &collisions)
        noexcept;

struct CudaShapeWorldPoseExecution {
    bool success = false;
    GmIso4 worldPose{};
    std::string diagnostic;
};

CudaShapeWorldPoseExecution ExecuteCudaShapeWorldPoseForCertification(
        const std::vector<CudaVehicleCollisionShape> &shapes,
        const CudaCandidateState &state,
        std::uint32_t shapeIndex) noexcept;

}  // namespace forevervalidator::simulation

#endif
