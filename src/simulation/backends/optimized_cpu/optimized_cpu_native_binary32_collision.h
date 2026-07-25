#ifndef FOREVERVALIDATOR_OPTIMIZED_CPU_NATIVE_BINARY32_COLLISION_H
#define FOREVERVALIDATOR_OPTIMIZED_CPU_NATIVE_BINARY32_COLLISION_H

#include <cstddef>
#include <cstdint>

struct CGmCollisionBuffer;
struct LocatedGmSurf;
class OptimizedCpuStaticMeshTriangleSidecar;
struct GmIso4;
struct SPlugSurfaceLocatedPair;

struct OptimizedCpuEllipsoidMeshPacketLane {
    const LocatedGmSurf *ellipsoid = nullptr;
    CGmCollisionBuffer *collisionBuffer = nullptr;
};

bool OptimizedCpuEllipsoidMeshPacketAvailable(void) noexcept;
bool GmCollision_EllipsoidPacket_Mesh_InlineMathOptimizedCpuNativeBinary32WithStaticCache(
        const OptimizedCpuEllipsoidMeshPacketLane *lanes,
        std::size_t laneCount,
        std::uint32_t activeMask,
        const LocatedGmSurf &mesh,
        const GmIso4 &meshInverse,
        const OptimizedCpuStaticMeshTriangleSidecar &triangles,
        std::uint32_t *hitMask) noexcept;

int GmCollision_Sphere_Mesh_OptimizedCpuNativeBinary32(
        const LocatedGmSurf &sphere,
        const LocatedGmSurf &mesh,
        CGmCollisionBuffer &collisionBuffer);
int GmCollision_Ellipsoid_Mesh_OptimizedCpuNativeBinary32(
        const LocatedGmSurf &ellipsoid,
        const LocatedGmSurf &mesh,
        CGmCollisionBuffer &collisionBuffer);
int GmCollision_Sphere_Mesh_InlineMathOptimizedCpuNativeBinary32(
        const LocatedGmSurf &sphere,
        const LocatedGmSurf &mesh,
        CGmCollisionBuffer &collisionBuffer);
int GmCollision_Ellipsoid_Mesh_InlineMathOptimizedCpuNativeBinary32(
        const LocatedGmSurf &ellipsoid,
        const LocatedGmSurf &mesh,
        CGmCollisionBuffer &collisionBuffer);
int ComputePlugSurfaceCollisionOptimizedCpuNativeBinary32(
        const SPlugSurfaceLocatedPair &pair,
        CGmCollisionBuffer &collisionBuffer);
int ComputePlugSurfaceCollisionInlineMathOptimizedCpuNativeBinary32(
        const SPlugSurfaceLocatedPair &pair,
        CGmCollisionBuffer &collisionBuffer);
int GmCollision_Sphere_Mesh_InlineMathOptimizedCpuNativeBinary32WithStaticCache(
        const LocatedGmSurf &sphere,
        const LocatedGmSurf &mesh,
        const GmIso4 &meshInverse,
        const OptimizedCpuStaticMeshTriangleSidecar &triangles,
        CGmCollisionBuffer &collisionBuffer);
int GmCollision_Ellipsoid_Mesh_InlineMathOptimizedCpuNativeBinary32WithStaticInverse(
        const LocatedGmSurf &ellipsoid,
        const LocatedGmSurf &mesh,
        const GmIso4 &meshInverse,
        CGmCollisionBuffer &collisionBuffer);
int GmCollision_Ellipsoid_Mesh_InlineMathOptimizedCpuNativeBinary32WithStaticCache(
        const LocatedGmSurf &ellipsoid,
        const LocatedGmSurf &mesh,
        const GmIso4 &meshInverse,
        const OptimizedCpuStaticMeshTriangleSidecar &triangles,
        CGmCollisionBuffer &collisionBuffer);
#if defined(FOREVERVALIDATOR_RELEASE_IPO_BUILD) && \
        defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif
#if defined(FOREVERVALIDATOR_RELEASE_IPO_BUILD) && \
        (defined(__GNUC__) || defined(__clang__))
__attribute__((always_inline))
#endif
int ComputePlugSurfaceCollisionInlineMathOptimizedCpuNativeBinary32WithStaticCache(
        const SPlugSurfaceLocatedPair &pair,
        const GmIso4 &staticMeshInverse,
        const OptimizedCpuStaticMeshTriangleSidecar *triangles,
        CGmCollisionBuffer &collisionBuffer);
#if defined(FOREVERVALIDATOR_RELEASE_IPO_BUILD) && \
        defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif
