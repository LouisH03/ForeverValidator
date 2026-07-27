#include <array>
#include <cfenv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

#include "engine/physics/collision/gm_collision_buffer.h"
#include "engine/physics/geometry/gm_surface.h"
#include "simulation/backends/optimized_cpu/optimized_cpu_ellipsoid_mesh_packet.h"
#include "simulation/backends/optimized_cpu/optimized_cpu_native_binary32_collision.h"
#include "simulation/backends/optimized_cpu/optimized_cpu_static_mesh_triangle_sidecar.h"
#include "simulation/runtime/replay_deterministic_execution.h"

#if defined(__i386__) || defined(__x86_64__)
#include <xmmintrin.h>
#endif

namespace {

constexpr std::size_t PacketWidth = 8u;

class VectorCollisionBuffer final : public CGmCollisionBuffer {
public:
    GmCollision &GetCollision(unsigned long index) override {
        return collisions_[index];
    }

    GmCollision &AddCollision(void) override {
        collisions_.emplace_back();
        return collisions_.back();
    }

    unsigned long GetCurrentCount(void) override {
        return static_cast<unsigned long>(collisions_.size());
    }

    const std::vector<GmCollision> &Collisions(void) const {
        return collisions_;
    }

private:
    std::vector<GmCollision> collisions_;
};

struct XorShift32 {
    std::uint32_t state = 0x51f3a26du;

    std::uint32_t Next(void) {
        state ^= state << 13u;
        state ^= state >> 17u;
        state ^= state << 5u;
        return state;
    }
};

std::uint32_t Bits(float value) {
    std::uint32_t result = 0u;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

bool SameVector(const char *field,
                std::size_t collisionIndex,
                const GmVec3 &left,
                const GmVec3 &right) {
    const std::uint32_t lx = Bits(left.x);
    const std::uint32_t ly = Bits(left.y);
    const std::uint32_t lz = Bits(left.z);
    const std::uint32_t rx = Bits(right.x);
    const std::uint32_t ry = Bits(right.y);
    const std::uint32_t rz = Bits(right.z);
    if (lx == rx && ly == ry && lz == rz) {
        return true;
    }
    std::fprintf(stderr,
                 "collision %zu %s differs: %08x/%08x %08x/%08x %08x/%08x\n",
                 collisionIndex,
                 field,
                 lx,
                 rx,
                 ly,
                 ry,
                 lz,
                 rz);
    return false;
}

bool SameCollision(std::size_t collisionIndex,
                   const GmCollision &left,
                   const GmCollision &right) {
    return SameVector("separation", collisionIndex,
                      left.separation, right.separation) &&
           SameVector("impulseNormal", collisionIndex,
                      left.impulseNormal, right.impulseNormal) &&
           SameVector("contactPoint", collisionIndex,
                      left.contactPoint, right.contactPoint) &&
           SameVector("extraNegated", collisionIndex,
                      left.extraNegated, right.extraNegated) &&
           left.localMaterialA.Index() == right.localMaterialA.Index() &&
           left.localMaterialB.Index() == right.localMaterialB.Index() &&
           left.materialA == right.materialA &&
           left.materialB == right.materialB &&
           left.sphereMergePrimary == right.sphereMergePrimary;
}

bool SameBuffer(const VectorCollisionBuffer &left,
                const VectorCollisionBuffer &right) {
    if (left.Collisions().size() != right.Collisions().size()) {
        std::fprintf(stderr,
                     "collision count differs: scalar=%zu packet=%zu\n",
                     left.Collisions().size(),
                     right.Collisions().size());
        return false;
    }
    for (std::size_t index = 0u; index < left.Collisions().size(); ++index) {
        if (!SameCollision(index,
                           left.Collisions()[index],
                           right.Collisions()[index])) {
            return false;
        }
    }
    return true;
}

void DumpCollision(const char *label, const GmCollision &collision) {
    std::fprintf(
            stderr,
            "%s sep=%08x,%08x,%08x normal=%08x,%08x,%08x contact=%08x,%08x,%08x extra=%08x,%08x,%08x primary=%d\n",
            label,
            Bits(collision.separation.x),
            Bits(collision.separation.y),
            Bits(collision.separation.z),
            Bits(collision.impulseNormal.x),
            Bits(collision.impulseNormal.y),
            Bits(collision.impulseNormal.z),
            Bits(collision.contactPoint.x),
            Bits(collision.contactPoint.y),
            Bits(collision.contactPoint.z),
            Bits(collision.extraNegated.x),
            Bits(collision.extraNegated.y),
            Bits(collision.extraNegated.z),
            collision.sphereMergePrimary);
}

GmSurfMeshTriangle Triangle(u32 a,
                            u32 b,
                            u32 c,
                            std::uint16_t material) {
    GmSurfMeshTriangle triangle{};
    triangle.normal = {0.0f, 0.0f, 1.0f};
    triangle.vertexIndex = {a, b, c};
    triangle.material = GmLocalMaterialIndex::FromIndex(material);
    return triangle;
}

bool BuildMesh(GmSurfMesh *mesh) {
    mesh->material = GmLocalMaterialIndex::FromIndex(31u);
    return mesh->SetGeometry(
            {{-1.0f, -1.0f, 0.0f},
             {1.0f, -1.0f, 0.0f},
             {-1.0f, 1.0f, 0.0f},
             {1.0f, 1.0f, 0.0f},
             {2.0f, -1.0f, 0.25f},
             {4.0f, -1.0f, 0.25f},
             {2.0f, 1.0f, 0.25f},
             {4.0f, 1.0f, 0.25f}},
            {Triangle(0u, 1u, 2u, 7u),
             Triangle(2u, 1u, 3u, 9u),
             Triangle(4u, 5u, 6u, 11u),
             Triangle(6u, 5u, 7u, 13u)},
            {},
            GmSurfMesh::PlaneSource::Archived);
}

float Coordinate(XorShift32 *random, int range, float scale) {
    const int value = static_cast<int>(
            random->Next() % static_cast<std::uint32_t>(range * 2 + 1)) -
            range;
    return static_cast<float>(value) * scale;
}

GmMat3 Rotation(std::uint32_t selector) {
    switch (selector & 3u) {
        case 1u:
            return {{0.0f, 1.0f, 0.0f},
                    {-1.0f, 0.0f, 0.0f},
                    {0.0f, 0.0f, 1.0f}};
        case 2u:
            return {{-1.0f, 0.0f, 0.0f},
                    {0.0f, -1.0f, 0.0f},
                    {0.0f, 0.0f, 1.0f}};
        case 3u:
            return {{0.0f, -1.0f, 0.0f},
                    {1.0f, 0.0f, 0.0f},
                    {0.0f, 0.0f, 1.0f}};
        default:
            return {{1.0f, 0.0f, 0.0f},
                    {0.0f, 1.0f, 0.0f},
                    {0.0f, 0.0f, 1.0f}};
    }
}

bool RunPackets(const GmSurfMesh &mesh,
                const OptimizedCpuStaticMeshTriangleSidecar &sidecar) {
    if (!OptimizedCpuEllipsoidMeshPacketAvailable()) {
        std::fprintf(stderr, "AVX2 packet path unavailable\n");
        return false;
    }
    const GmIso4 meshIso = {
        Rotation(1u),
        {0.25f, -0.5f, 0.75f},
    };
    GmIso4 meshInverse;
    meshInverse.SetInverse(meshIso);
    const LocatedGmSurf locatedMesh = {&mesh, &meshIso, true};
    OptimizedCpuStaticMeshTriangleHierarchyView hierarchy;
    if (!sidecar.TriangleHierarchyView(&hierarchy)) {
        std::fprintf(stderr, "certified hierarchy unavailable\n");
        return false;
    }
    const OptimizedCpuCertifiedStaticMeshPacket certifiedMesh = {
        &mesh,
        meshIso,
        meshInverse,
        &sidecar,
        hierarchy,
    };
    XorShift32 random;

    for (std::size_t caseIndex = 0u; caseIndex < 4096u; ++caseIndex) {
        std::array<GmSurfEllipsoid, PacketWidth> ellipsoids;
        std::array<GmIso4, PacketWidth> ellipsoidIsos;
        std::array<LocatedGmSurf, PacketWidth> located;
        std::array<VectorCollisionBuffer, PacketWidth> scalarBuffers;
        std::array<VectorCollisionBuffer, PacketWidth> packetBuffers;
        std::array<VectorCollisionBuffer, PacketWidth> preparedBuffers;
        std::array<VectorCollisionBuffer, PacketWidth> certifiedBuffers;
        std::array<OptimizedCpuEllipsoidMeshPacketLane, PacketWidth> lanes;
        std::array<OptimizedCpuEllipsoidMeshPacketLane, PacketWidth>
                preparedLanes;
        std::array<OptimizedCpuEllipsoidMeshPacketLane, PacketWidth>
                certifiedLanes;
        std::uint32_t activeMask = random.Next() & 0xffu;
        if (__builtin_popcount(activeMask) < 2) {
            activeMask |= 0x3u;
        }
        std::uint32_t scalarHitMask = 0u;

        for (std::size_t lane = 0u; lane < PacketWidth; ++lane) {
            ellipsoids[lane].material =
                    GmLocalMaterialIndex::FromIndex(
                            static_cast<std::uint16_t>(lane + 1u));
            ellipsoids[lane].radii = {
                0.25f + static_cast<float>(random.Next() % 8u) * 0.0625f,
                0.25f + static_cast<float>(random.Next() % 8u) * 0.0625f,
                0.25f + static_cast<float>(random.Next() % 8u) * 0.0625f,
            };
            ellipsoidIsos[lane] = {
                Rotation(random.Next()),
                {
                    Coordinate(&random, 40, 0.0625f) + 1.25f,
                    Coordinate(&random, 24, 0.0625f),
                    Coordinate(&random, 12, 0.03125f) + 0.75f,
                },
            };
            located[lane] = {
                &ellipsoids[lane],
                &ellipsoidIsos[lane],
                true,
            };
            lanes[lane] = {&located[lane], &packetBuffers[lane]};
            preparedLanes[lane] = {
                &located[lane],
                &preparedBuffers[lane],
            };
            certifiedLanes[lane] = {
                &located[lane],
                &certifiedBuffers[lane],
            };
            if ((activeMask & (1u << lane)) != 0u) {
                const int hit =
                        GmCollision_Ellipsoid_Mesh_InlineMathOptimizedCpuNativeBinary32WithStaticCache(
                                located[lane],
                                locatedMesh,
                                meshInverse,
                                sidecar,
                                scalarBuffers[lane]);
                if (hit != 0) {
                    scalarHitMask |= 1u << lane;
                }
            }
        }

        std::uint32_t packetHitMask = 0u;
        if (!GmCollision_EllipsoidPacket_Mesh_InlineMathOptimizedCpuNativeBinary32WithStaticCache(
                    lanes.data(),
                    lanes.size(),
                    activeMask,
                    locatedMesh,
                    meshInverse,
                    sidecar,
                    &packetHitMask)) {
            std::fprintf(stderr, "packet case %zu was not accepted\n", caseIndex);
            return false;
        }
        if (packetHitMask != scalarHitMask) {
            std::fprintf(stderr,
                         "packet case %zu hit mask differs: scalar=%02x packet=%02x\n",
                         caseIndex,
                         scalarHitMask,
                         packetHitMask);
            return false;
        }
        OptimizedCpuPreparedEllipsoidMeshPacket prepared;
        if (!PrepareOptimizedCpuEllipsoidMeshPacket(
                    preparedLanes.data(),
                    preparedLanes.size(),
                    0xffu,
                    &prepared)) {
            std::fprintf(stderr,
                         "prepared packet case %zu was not accepted\n",
                         caseIndex);
            return false;
        }
        std::uint32_t preparedHitMask = 0u;
        if (!GmCollision_PreparedEllipsoidPacket_Mesh_InlineMathOptimizedCpuNativeBinary32WithStaticCache(
                    prepared,
                    activeMask,
                    locatedMesh,
                    meshInverse,
                    sidecar,
                    &preparedHitMask)) {
            std::fprintf(stderr,
                         "prepared collision case %zu was not accepted\n",
                         caseIndex);
            return false;
        }
        if (preparedHitMask != scalarHitMask) {
            std::fprintf(stderr,
                         "prepared case %zu hit mask differs: scalar=%02x prepared=%02x\n",
                         caseIndex,
                         scalarHitMask,
                         preparedHitMask);
            return false;
        }
        OptimizedCpuPreparedEllipsoidMeshPacket certifiedPrepared;
        if (!PrepareOptimizedCpuEllipsoidMeshPacket(
                    certifiedLanes.data(),
                    certifiedLanes.size(),
                    0xffu,
                    &certifiedPrepared)) {
            std::fprintf(stderr,
                         "certified packet case %zu was not prepared\n",
                         caseIndex);
            return false;
        }
        std::uint32_t certifiedHitMask = 0u;
        if (!GmCollision_PreparedEllipsoidPacket_Mesh_InlineMathOptimizedCpuNativeBinary32WithCertifiedStaticMesh(
                    certifiedPrepared,
                    activeMask,
                    certifiedMesh,
                    &certifiedHitMask) ||
            certifiedHitMask != scalarHitMask) {
            std::fprintf(stderr,
                         "certified case %zu hit mask differs: scalar=%02x certified=%02x\n",
                         caseIndex,
                         scalarHitMask,
                         certifiedHitMask);
            return false;
        }
        for (std::size_t lane = 0u; lane < PacketWidth; ++lane) {
            if (!SameBuffer(scalarBuffers[lane], packetBuffers[lane]) ||
                !SameBuffer(scalarBuffers[lane], preparedBuffers[lane]) ||
                !SameBuffer(scalarBuffers[lane], certifiedBuffers[lane])) {
                std::fprintf(stderr,
                             "packet case %zu lane %zu collision differs\n",
                             caseIndex,
                             lane);
                std::fprintf(stderr,
                             "active=%02x radii=%08x,%08x,%08x translation=%08x,%08x,%08x\n",
                             activeMask,
                             Bits(ellipsoids[lane].radii.x),
                             Bits(ellipsoids[lane].radii.y),
                             Bits(ellipsoids[lane].radii.z),
                             Bits(ellipsoidIsos[lane].translation.x),
                             Bits(ellipsoidIsos[lane].translation.y),
                             Bits(ellipsoidIsos[lane].translation.z));
                if (!scalarBuffers[lane].Collisions().empty() &&
                    !packetBuffers[lane].Collisions().empty()) {
                    DumpCollision("scalar", scalarBuffers[lane].Collisions()[0u]);
                    DumpCollision("packet", packetBuffers[lane].Collisions()[0u]);
                }
                return false;
            }
        }
        if (caseIndex == 0u) {
            std::array<std::size_t, PacketWidth> collisionCountsBefore{};
            for (std::size_t lane = 0u; lane < PacketWidth; ++lane) {
                collisionCountsBefore[lane] =
                        certifiedBuffers[lane].Collisions().size();
            }
            OptimizedCpuCertifiedStaticMeshPacket excessiveDepth =
                    certifiedMesh;
            excessiveDepth.hierarchy.maximumTraversalDepth =
                    std::numeric_limits<std::size_t>::max();
            std::uint32_t excessiveDepthHitMask = 0xffffffffu;
            if (GmCollision_PreparedEllipsoidPacket_Mesh_InlineMathOptimizedCpuNativeBinary32WithCertifiedStaticMesh(
                        certifiedPrepared,
                        activeMask,
                        excessiveDepth,
                        &excessiveDepthHitMask) ||
                excessiveDepthHitMask != 0u) {
                std::fprintf(stderr,
                             "certified packet accepted excessive traversal depth\n");
                return false;
            }
            for (std::size_t lane = 0u; lane < PacketWidth; ++lane) {
                if (certifiedBuffers[lane].Collisions().size() !=
                    collisionCountsBefore[lane]) {
                    std::fprintf(stderr,
                                 "traversal depth rejection emitted collisions\n");
                    return false;
                }
            }

            OptimizedCpuCertifiedStaticMeshPacket missingPacketCells =
                    certifiedMesh;
            missingPacketCells.hierarchy.packetCells = nullptr;
            std::uint32_t missingPacketCellsHitMask = 0xffffffffu;
            if (GmCollision_PreparedEllipsoidPacket_Mesh_InlineMathOptimizedCpuNativeBinary32WithCertifiedStaticMesh(
                        certifiedPrepared,
                        activeMask,
                        missingPacketCells,
                        &missingPacketCellsHitMask) ||
                missingPacketCellsHitMask != 0u) {
                std::fprintf(stderr,
                             "certified packet accepted missing packet cells\n");
                return false;
            }
            for (std::size_t lane = 0u; lane < PacketWidth; ++lane) {
                if (certifiedBuffers[lane].Collisions().size() !=
                    collisionCountsBefore[lane]) {
                    std::fprintf(stderr,
                                 "missing packet cells emitted collisions\n");
                    return false;
                }
            }

            OptimizedCpuCertifiedStaticMeshPacket missingDepths =
                    certifiedMesh;
            missingDepths.hierarchy.depths = nullptr;
            std::uint32_t missingDepthsHitMask = 0xffffffffu;
            if (GmCollision_PreparedEllipsoidPacket_Mesh_InlineMathOptimizedCpuNativeBinary32WithCertifiedStaticMesh(
                        certifiedPrepared,
                        activeMask,
                        missingDepths,
                        &missingDepthsHitMask) ||
                missingDepthsHitMask != 0u) {
                std::fprintf(stderr,
                             "certified packet accepted missing traversal depths\n");
                return false;
            }
            for (std::size_t lane = 0u; lane < PacketWidth; ++lane) {
                if (certifiedBuffers[lane].Collisions().size() !=
                    collisionCountsBefore[lane]) {
                    std::fprintf(stderr,
                                 "missing traversal depths emitted collisions\n");
                    return false;
                }
            }

            OptimizedCpuPreparedEllipsoidMeshPacket incomplete = prepared;
            const std::uint32_t firstActiveBit =
                    activeMask & (0u - activeMask);
            incomplete.preparedMask &= ~firstActiveBit;
            std::uint32_t rejectedHitMask = 0xffffffffu;
            if (GmCollision_PreparedEllipsoidPacket_Mesh_InlineMathOptimizedCpuNativeBinary32WithStaticCache(
                        incomplete,
                        activeMask,
                        locatedMesh,
                        meshInverse,
                        sidecar,
                        &rejectedHitMask) ||
                rejectedHitMask != 0u) {
                std::fprintf(stderr,
                             "prepared packet accepted an unprepared active lane\n");
                return false;
            }

            const GmVec3 savedRadii = ellipsoids[0u].radii;
            ellipsoids[0u].radii.x = 0.0f;
            GmIso4 invalidInverse = meshInverse;
            invalidInverse.translation.x =
                    std::numeric_limits<float>::quiet_NaN();
            std::feclearexcept(FE_ALL_EXCEPT);
            std::feraiseexcept(FE_INVALID);
            const int exceptionsBefore = std::fetestexcept(FE_ALL_EXCEPT);
            std::uint32_t invalidHitMask = 0xffffffffu;
            if (GmCollision_EllipsoidPacket_Mesh_InlineMathOptimizedCpuNativeBinary32WithStaticCache(
                        lanes.data(),
                        lanes.size(),
                        0x1u,
                        locatedMesh,
                        invalidInverse,
                        sidecar,
                        &invalidHitMask) ||
                invalidHitMask != 0u ||
                std::fetestexcept(FE_ALL_EXCEPT) != exceptionsBefore) {
                std::fprintf(stderr,
                             "rejected packet changed floating-point environment\n");
                return false;
            }
            std::feclearexcept(FE_ALL_EXCEPT);
            ellipsoids[0u].radii = savedRadii;
        }
    }
    return true;
}

}  // namespace

int main(void) {
    tmnf::simulation::DeterministicExecutionScope deterministicScope;
    if (!deterministicScope.Established()) {
        std::fprintf(stderr, "deterministic execution unavailable\n");
        return 1;
    }
    GmSurfMesh mesh;
    OptimizedCpuStaticMeshTriangleSidecar sidecar;
    if (!BuildMesh(&mesh) || !sidecar.TryBuild(mesh) ||
        !RunPackets(mesh, sidecar)) {
        return 1;
    }
    std::printf(
            "ellipsoid_mesh_packet_cases=4096 lanes=8 result=identical\n");
    return 0;
}
