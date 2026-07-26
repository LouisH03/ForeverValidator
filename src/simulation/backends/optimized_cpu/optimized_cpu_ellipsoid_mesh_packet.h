#ifndef FOREVERVALIDATOR_OPTIMIZED_CPU_ELLIPSOID_MESH_PACKET_H
#define FOREVERVALIDATOR_OPTIMIZED_CPU_ELLIPSOID_MESH_PACKET_H

#include <array>
#include <cstddef>
#include <cstdint>

#include "engine/physics/geometry/gm_surface.h"
#include "simulation/backends/optimized_cpu/optimized_cpu_native_binary32_collision.h"

struct alignas(32) OptimizedCpuPacketFloatLanes {
    std::array<float, 8u> values{};
};

struct alignas(32) OptimizedCpuPreparedEllipsoidMeshPacket {
    static constexpr std::size_t Width = 8u;

    OptimizedCpuPacketFloatLanes worldXx{};
    OptimizedCpuPacketFloatLanes worldXy{};
    OptimizedCpuPacketFloatLanes worldXz{};
    OptimizedCpuPacketFloatLanes worldYx{};
    OptimizedCpuPacketFloatLanes worldYy{};
    OptimizedCpuPacketFloatLanes worldYz{};
    OptimizedCpuPacketFloatLanes worldZx{};
    OptimizedCpuPacketFloatLanes worldZy{};
    OptimizedCpuPacketFloatLanes worldZz{};
    OptimizedCpuPacketFloatLanes worldTx{};
    OptimizedCpuPacketFloatLanes worldTy{};
    OptimizedCpuPacketFloatLanes worldTz{};
    OptimizedCpuPacketFloatLanes radiiX{};
    OptimizedCpuPacketFloatLanes radiiY{};
    OptimizedCpuPacketFloatLanes radiiZ{};
    OptimizedCpuPacketFloatLanes inverseRadiiX{};
    OptimizedCpuPacketFloatLanes inverseRadiiY{};
    OptimizedCpuPacketFloatLanes inverseRadiiZ{};
    std::array<GmLocalMaterialIndex, Width> materials{};
    std::array<CGmCollisionBuffer *, Width> buffers{};
    std::size_t laneCount = 0u;
    std::uint32_t preparedMask = 0u;
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

#endif
