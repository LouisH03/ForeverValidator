#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "engine/physics/collision/gm_collision_buffer.h"
#include "engine/physics/geometry/gm_surface.h"
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
    XorShift32 random;

    for (std::size_t caseIndex = 0u; caseIndex < 4096u; ++caseIndex) {
        std::array<GmSurfEllipsoid, PacketWidth> ellipsoids;
        std::array<GmIso4, PacketWidth> ellipsoidIsos;
        std::array<LocatedGmSurf, PacketWidth> located;
        std::array<VectorCollisionBuffer, PacketWidth> scalarBuffers;
        std::array<VectorCollisionBuffer, PacketWidth> packetBuffers;
        std::array<OptimizedCpuEllipsoidMeshPacketLane, PacketWidth> lanes;
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
        for (std::size_t lane = 0u; lane < PacketWidth; ++lane) {
            if (!SameBuffer(scalarBuffers[lane], packetBuffers[lane])) {
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
