#ifndef FOREVERVALIDATOR_OPTIMIZED_CPU_ELLIPSOID_MESH_PACKET_H
#define FOREVERVALIDATOR_OPTIMIZED_CPU_ELLIPSOID_MESH_PACKET_H

#include <array>
#include <cstddef>
#include <cstdint>

#include "engine/physics/geometry/gm_surface.h"
#include "simulation/backends/optimized_cpu/optimized_cpu_native_binary32_collision.h"
#include "simulation/backends/optimized_cpu/optimized_cpu_static_mesh_triangle_sidecar.h"

struct alignas(32) OptimizedCpuPacketFloatLanes {
    // Prepared packets are always populated through the preparation routine.
    // Leave default-initialized destination storage untouched so callers do
    // not clear the full packet before that routine builds its authoritative
    // value-initialized candidate.
    std::array<float, 8u> values;
};

struct alignas(32) OptimizedCpuPreparedEllipsoidMeshPacket {
    static constexpr std::size_t Width = 8u;

    OptimizedCpuPacketFloatLanes worldXx;
    OptimizedCpuPacketFloatLanes worldXy;
    OptimizedCpuPacketFloatLanes worldXz;
    OptimizedCpuPacketFloatLanes worldYx;
    OptimizedCpuPacketFloatLanes worldYy;
    OptimizedCpuPacketFloatLanes worldYz;
    OptimizedCpuPacketFloatLanes worldZx;
    OptimizedCpuPacketFloatLanes worldZy;
    OptimizedCpuPacketFloatLanes worldZz;
    OptimizedCpuPacketFloatLanes worldTx;
    OptimizedCpuPacketFloatLanes worldTy;
    OptimizedCpuPacketFloatLanes worldTz;
    OptimizedCpuPacketFloatLanes radiiX;
    OptimizedCpuPacketFloatLanes radiiY;
    OptimizedCpuPacketFloatLanes radiiZ;
    OptimizedCpuPacketFloatLanes inverseRadiiX;
    OptimizedCpuPacketFloatLanes inverseRadiiY;
    OptimizedCpuPacketFloatLanes inverseRadiiZ;
    std::array<GmLocalMaterialIndex, Width> materials;
    std::array<CGmCollisionBuffer *, Width> buffers;
    std::size_t laneCount;
    std::uint32_t preparedMask;
};

struct OptimizedCpuCertifiedStaticMeshPacket {
    const GmSurfMesh *sourceMesh = nullptr;
    GmIso4 meshIso{};
    GmIso4 meshInverse{};
    const OptimizedCpuStaticMeshTriangleSidecar *triangles = nullptr;
    OptimizedCpuStaticMeshTriangleHierarchyView hierarchy{};

    bool IsAvailable(void) const noexcept {
        return sourceMesh != nullptr && triangles != nullptr &&
               hierarchy.cells != nullptr && hierarchy.depths != nullptr &&
               hierarchy.packetCells != nullptr &&
               hierarchy.count != 0u;
    }
};

bool PrepareOptimizedCpuEllipsoidMeshPacket(
        const OptimizedCpuEllipsoidMeshPacketLane *lanes,
        std::size_t laneCount,
        std::uint32_t preparedMask,
        OptimizedCpuPreparedEllipsoidMeshPacket *prepared) noexcept;

bool GmCollision_PreparedEllipsoidPacket_Mesh_InlineMathOptimizedCpuNativeBinary32WithStaticCache(
        const OptimizedCpuPreparedEllipsoidMeshPacket &prepared,
        std::uint32_t activeMask,
        const LocatedGmSurf &mesh,
        const GmIso4 &meshInverse,
        const OptimizedCpuStaticMeshTriangleSidecar &triangles,
        std::uint32_t *hitMask) noexcept;

bool GmCollision_PreparedEllipsoidPacket_Mesh_InlineMathOptimizedCpuNativeBinary32WithCertifiedStaticMesh(
        const OptimizedCpuPreparedEllipsoidMeshPacket &prepared,
        std::uint32_t activeMask,
        const OptimizedCpuCertifiedStaticMeshPacket &mesh,
        std::uint32_t *hitMask) noexcept;

#endif
