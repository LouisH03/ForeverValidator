#ifndef FOREVERVALIDATOR_CUDA_COLLISION_LAYOUT_H
#define FOREVERVALIDATOR_CUDA_COLLISION_LAYOUT_H

#include <cstdint>

#include "engine/core/gm_types.h"

namespace forevervalidator::simulation::cuda::collision {

constexpr std::uint32_t CollisionCapacity = 512u;
constexpr std::uint32_t ShapeCollisionCapacity = 256u;
constexpr std::uint32_t SurfaceHitCapacity = 128u;

enum class Status : std::uint32_t {
    Success,
    Overflow,
    UnsupportedGeometry,
    InvalidScene,
};

struct CudaCollision {
    GmVec3 separation{};
    GmVec3 impulseNormal{};
    GmVec3 contactPoint{};
    std::uint32_t materialA = 0u;
    std::uint32_t materialB = 0u;
    bool sphereMergePrimary = false;
    GmVec3 extraNegated{};
    std::uint32_t movingShapeIndex = UINT32_MAX;
    std::uint32_t staticSurfaceIndex = UINT32_MAX;
    std::uint32_t staticActorIndex = UINT32_MAX;
};

struct CudaCollisionSurfaceHit {
    std::uint32_t surfaceIndex = UINT32_MAX;
    std::uint32_t shapeMask = 0u;
};

struct CudaCollisionScratch {
    std::uint32_t collisionCount = 0u;
    std::uint32_t shapeCollisionCount = 0u;
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
    bool overflow = false;
    CudaCollision collisions[CollisionCapacity]{};
    CudaCollision shapeCollisions[ShapeCollisionCapacity]{};
};

struct CudaCollisionSearchScratch {
    std::uint32_t collisionCount;
    std::uint32_t shapeCollisionCount;
    std::uint32_t surfaceHitCount;
    bool overflow;
    bool surfaceCacheEnabled;
    CudaCollision *collisionStorage;
    CudaCollision *shapeCollisionStorage;
    GmIso4 *shapeWorldStorage;
    GmBoxAligned *movingBoundsStorage;
    CudaCollisionSurfaceHit *surfaceHitStorage;
    std::uint32_t slot;
    std::uint32_t stride;
    std::uint32_t shapeCapacity;
    bool surfaceCacheValid = false;
};

}  // namespace forevervalidator::simulation::cuda::collision

#endif
