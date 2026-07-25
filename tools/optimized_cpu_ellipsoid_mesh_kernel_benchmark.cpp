#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include "engine/physics/collision/gm_collision_buffer.h"
#include "engine/physics/geometry/gm_surface.h"
#include "engine/physics/geometry/gmsurf_collision.h"
#include "simulation/backends/optimized_cpu/optimized_cpu_native_binary32_collision.h"
#include "simulation/backends/optimized_cpu/optimized_cpu_static_mesh_triangle_sidecar.h"
#include "simulation/runtime/replay_deterministic_execution.h"

namespace {

constexpr std::size_t PacketWidth = 8u;
constexpr std::size_t CaseCount = 256u;
constexpr std::size_t CollisionCapacity = 128u;

volatile std::uint64_t benchmarkSink = 0u;

class FixedCollisionBuffer final : public CGmCollisionBuffer {
public:
    GmCollision &GetCollision(unsigned long index) override {
        return collisions_[index];
    }

    GmCollision &AddCollision(void) override {
        if (count_ >= collisions_.size()) {
            std::abort();
        }
        collisions_[count_] = {};
        return collisions_[count_++];
    }

    unsigned long GetCurrentCount(void) override {
        return static_cast<unsigned long>(count_);
    }

    void Reset(void) noexcept {
        count_ = 0u;
    }

    std::uint64_t Checksum(void) const noexcept {
        if (count_ == 0u) {
            return 0u;
        }
        const GmCollision &collision = collisions_[0u];
        std::uint32_t x = 0u;
        std::memcpy(&x, &collision.contactPoint.x, sizeof(x));
        return static_cast<std::uint64_t>(count_) * 0x9e3779b185ebca87ull + x;
    }

private:
    std::array<GmCollision, CollisionCapacity> collisions_{};
    std::size_t count_ = 0u;
};

struct XorShift32 {
    std::uint32_t state = 0x7f4a7c15u;

    std::uint32_t Next(void) noexcept {
        state ^= state << 13u;
        state ^= state >> 17u;
        state ^= state << 5u;
        return state;
    }

    float Unit(void) noexcept {
        return static_cast<float>(Next() & 0xffffu) / 65535.0f;
    }
};

GmSurfMeshTriangle Triangle(u32 a,
                            u32 b,
                            u32 c,
                            std::uint16_t material) {
    GmSurfMeshTriangle triangle{};
    triangle.vertexIndex = {a, b, c};
    triangle.material = GmLocalMaterialIndex::FromIndex(material);
    return triangle;
}

bool BuildTriangleField(GmSurfMesh *mesh) {
    constexpr u32 dimension = 128u;
    constexpr float spacing = 0.75f;
    const u32 row = dimension + 1u;
    std::vector<GmVec3> vertices;
    std::vector<GmSurfMeshTriangle> triangles;
    vertices.reserve(static_cast<std::size_t>(row) * row);
    triangles.reserve(static_cast<std::size_t>(dimension) * dimension * 2u);

    for (u32 y = 0u; y <= dimension; ++y) {
        for (u32 x = 0u; x <= dimension; ++x) {
            const float px = (static_cast<float>(x) - dimension * 0.5f) * spacing;
            const float py = (static_cast<float>(y) - dimension * 0.5f) * spacing;
            const float pz = 0.04f * std::sin(px * 0.11f) * std::cos(py * 0.09f);
            vertices.push_back({px, py, pz});
        }
    }
    for (u32 y = 0u; y < dimension; ++y) {
        for (u32 x = 0u; x < dimension; ++x) {
            const u32 a = y * row + x;
            const u32 b = a + 1u;
            const u32 c = a + row;
            const u32 d = c + 1u;
            const std::uint16_t material =
                    static_cast<std::uint16_t>(1u + ((x + y) & 7u));
            triangles.push_back(Triangle(a, b, c, material));
            triangles.push_back(Triangle(c, b, d, material));
        }
    }
    mesh->material = GmLocalMaterialIndex::FromIndex(31u);
    return mesh->SetGeometry(
            std::move(vertices),
            std::move(triangles),
            {},
            GmSurfMesh::PlaneSource::Generated);
}

enum class Scenario {
    Contact,
    Reject,
    Mixed,
};

struct BenchmarkCase {
    std::array<GmSurfEllipsoid, PacketWidth> ellipsoids{};
    std::array<GmIso4, PacketWidth> transforms{};
    std::array<LocatedGmSurf, PacketWidth> located{};
    std::array<FixedCollisionBuffer, PacketWidth> scalarBuffers{};
    std::array<FixedCollisionBuffer, PacketWidth> packetBuffers{};
    std::array<OptimizedCpuEllipsoidMeshPacketLane, PacketWidth> lanes{};
};

void BuildCases(std::vector<BenchmarkCase> *cases, Scenario scenario) {
    cases->resize(CaseCount);
    XorShift32 random;
    for (std::size_t caseIndex = 0u; caseIndex < cases->size(); ++caseIndex) {
        BenchmarkCase &entry = (*cases)[caseIndex];
        const float baseX = (random.Unit() - 0.5f) * 72.0f;
        const float baseY = (random.Unit() - 0.5f) * 72.0f;
        const bool contact = scenario == Scenario::Contact ||
                (scenario == Scenario::Mixed && (caseIndex & 1u) == 0u);
        for (std::size_t lane = 0u; lane < PacketWidth; ++lane) {
            GmSurfEllipsoid &ellipsoid = entry.ellipsoids[lane];
            ellipsoid.material = GmLocalMaterialIndex::FromIndex(
                    static_cast<std::uint16_t>(lane + 1u));
            ellipsoid.radii = {
                0.42f + static_cast<float>(lane & 1u) * 0.03f,
                0.28f + static_cast<float>((lane >> 1u) & 1u) * 0.025f,
                0.34f + static_cast<float>((lane >> 2u) & 1u) * 0.035f,
            };
            const float laneX = static_cast<float>(lane & 3u) * 0.18f;
            const float laneY = static_cast<float>(lane >> 2u) * 0.22f;
            entry.transforms[lane] = {
                {{1.0f, 0.0f, 0.0f},
                 {0.0f, 1.0f, 0.0f},
                 {0.0f, 0.0f, 1.0f}},
                {
                    baseX + laneX,
                    baseY + laneY,
                    contact
                            ? 0.25f + static_cast<float>(lane & 1u) * 0.025f
                            : 2.75f + static_cast<float>(lane) * 0.02f,
                },
            };
            entry.located[lane] = {
                &ellipsoid,
                &entry.transforms[lane],
                true,
            };
            entry.lanes[lane] = {
                &entry.located[lane],
                &entry.packetBuffers[lane],
            };
        }
    }
}

std::uint64_t RunScalar(
        std::vector<BenchmarkCase> &cases,
        const LocatedGmSurf &mesh,
        const GmIso4 &meshInverse,
        const OptimizedCpuStaticMeshTriangleSidecar &sidecar,
        std::size_t rounds) {
    std::uint64_t checksum = 0u;
    for (std::size_t round = 0u; round < rounds; ++round) {
        for (BenchmarkCase &entry : cases) {
            for (std::size_t lane = 0u; lane < PacketWidth; ++lane) {
                entry.scalarBuffers[lane].Reset();
                const int hit =
                        GmCollision_Ellipsoid_Mesh_InlineMathOptimizedCpuNativeBinary32WithStaticCache(
                                entry.located[lane],
                                mesh,
                                meshInverse,
                                sidecar,
                                entry.scalarBuffers[lane]);
                checksum = checksum * 0x100000001b3ull +
                        static_cast<std::uint64_t>(hit) +
                        entry.scalarBuffers[lane].Checksum();
            }
        }
    }
    return checksum;
}

std::uint64_t RunReference(
        std::vector<BenchmarkCase> &cases,
        const LocatedGmSurf &mesh,
        std::size_t rounds) {
    std::uint64_t checksum = 0u;
    for (std::size_t round = 0u; round < rounds; ++round) {
        for (BenchmarkCase &entry : cases) {
            for (std::size_t lane = 0u; lane < PacketWidth; ++lane) {
                entry.scalarBuffers[lane].Reset();
                const int hit = GmCollision_Ellipsoid_Mesh(
                        entry.located[lane],
                        mesh,
                        entry.scalarBuffers[lane]);
                checksum = checksum * 0x100000001b3ull +
                        static_cast<std::uint64_t>(hit) +
                        entry.scalarBuffers[lane].Checksum();
            }
        }
    }
    return checksum;
}

std::uint64_t RunPacket(
        std::vector<BenchmarkCase> &cases,
        const LocatedGmSurf &mesh,
        const GmIso4 &meshInverse,
        const OptimizedCpuStaticMeshTriangleSidecar &sidecar,
        std::size_t rounds) {
    std::uint64_t checksum = 0u;
    for (std::size_t round = 0u; round < rounds; ++round) {
        for (BenchmarkCase &entry : cases) {
            for (FixedCollisionBuffer &buffer : entry.packetBuffers) {
                buffer.Reset();
            }
            std::uint32_t hitMask = 0u;
            if (!GmCollision_EllipsoidPacket_Mesh_InlineMathOptimizedCpuNativeBinary32WithStaticCache(
                        entry.lanes.data(),
                        entry.lanes.size(),
                        0xffu,
                        mesh,
                        meshInverse,
                        sidecar,
                        &hitMask)) {
                std::abort();
            }
            checksum = checksum * 0x100000001b3ull + hitMask;
            for (const FixedCollisionBuffer &buffer : entry.packetBuffers) {
                checksum = checksum * 0x100000001b3ull + buffer.Checksum();
            }
        }
    }
    return checksum;
}

template <typename Function>
std::uint64_t MeasureMedian(Function &&function,
                            std::size_t warmups,
                            std::size_t repetitions) {
    for (std::size_t index = 0u; index < warmups; ++index) {
        benchmarkSink ^= function();
    }
    std::vector<std::uint64_t> samples;
    samples.reserve(repetitions);
    for (std::size_t index = 0u; index < repetitions; ++index) {
        const auto begin = std::chrono::steady_clock::now();
        benchmarkSink ^= function();
        const auto end = std::chrono::steady_clock::now();
        samples.push_back(static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                        end - begin).count()));
    }
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2u];
}

const char *ScenarioName(Scenario scenario) {
    switch (scenario) {
        case Scenario::Contact:
            return "contact";
        case Scenario::Reject:
            return "reject";
        case Scenario::Mixed:
            return "mixed";
    }
    return "unknown";
}

void BenchmarkScenario(
        Scenario scenario,
        const LocatedGmSurf &mesh,
        const GmIso4 &meshInverse,
        const OptimizedCpuStaticMeshTriangleSidecar &sidecar) {
    std::vector<BenchmarkCase> cases;
    BuildCases(&cases, scenario);
    constexpr std::size_t rounds = 8u;
    constexpr std::size_t warmups = 5u;
    constexpr std::size_t repetitions = 21u;
    const std::uint64_t scalarNs = MeasureMedian(
            [&] {
                return RunScalar(cases, mesh, meshInverse, sidecar, rounds);
            },
            warmups,
            repetitions);
    const std::uint64_t referenceNs = MeasureMedian(
            [&] {
                return RunReference(cases, mesh, rounds);
            },
            warmups,
            repetitions);
    const std::uint64_t packetNs = MeasureMedian(
            [&] {
                return RunPacket(cases, mesh, meshInverse, sidecar, rounds);
            },
            warmups,
            repetitions);
    const double invocations =
            static_cast<double>(CaseCount * rounds);
    const double speedup = static_cast<double>(scalarNs) / packetNs;
    const double referenceSpeedup =
            static_cast<double>(referenceNs) / packetNs;
    std::printf(
            "scenario=%s reference_total_ns=%llu scalar_total_ns=%llu "
            "packet_total_ns=%llu scalar_ns_per_8=%.2f "
            "packet_ns_per_8=%.2f "
            "speedup_vs_scalar=%.3fx speedup_vs_reference=%.3fx\n",
            ScenarioName(scenario),
            static_cast<unsigned long long>(referenceNs),
            static_cast<unsigned long long>(scalarNs),
            static_cast<unsigned long long>(packetNs),
            static_cast<double>(scalarNs) / invocations,
            static_cast<double>(packetNs) / invocations,
            speedup,
            referenceSpeedup);
}

}  // namespace

int main(void) {
    tmnf::simulation::DeterministicExecutionScope deterministicScope;
    if (!deterministicScope.Established() ||
        !OptimizedCpuEllipsoidMeshPacketAvailable()) {
        std::fprintf(stderr, "required deterministic AVX2 path unavailable\n");
        return 1;
    }

    GmSurfMesh mesh;
    OptimizedCpuStaticMeshTriangleSidecar sidecar;
    if (!BuildTriangleField(&mesh) || !sidecar.TryBuild(mesh)) {
        std::fprintf(stderr, "failed to build benchmark mesh\n");
        return 1;
    }
    const GmIso4 meshIso = {
        {{1.0f, 0.0f, 0.0f},
         {0.0f, 1.0f, 0.0f},
         {0.0f, 0.0f, 1.0f}},
        {},
    };
    GmIso4 meshInverse;
    meshInverse.SetInverse(meshIso);
    const LocatedGmSurf locatedMesh = {&mesh, &meshIso, true};

    BenchmarkScenario(Scenario::Reject, locatedMesh, meshInverse, sidecar);
    BenchmarkScenario(Scenario::Contact, locatedMesh, meshInverse, sidecar);
    BenchmarkScenario(Scenario::Mixed, locatedMesh, meshInverse, sidecar);
    return benchmarkSink == 0xdeadbeefull ? 2 : 0;
}
