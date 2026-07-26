#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

#include "engine/physics/geometry/gm_surface.h"
#include "engine/physics/geometry/plug_surface.h"
#include "engine/rendering/plug_tree.h"
#include "simulation/backends/optimized_cpu/optimized_cpu_vehicle_collision_bounds_plan.h"

namespace {

constexpr std::size_t ChildCount = 8u;
constexpr std::size_t RandomCaseCount = 4096u;

struct XorShift32 {
    std::uint32_t state = 0x73a9e51du;

    std::uint32_t Next(void) {
        std::uint32_t value = state;
        value ^= value << 13u;
        value ^= value >> 17u;
        value ^= value << 5u;
        state = value;
        return value;
    }

    float FiniteFloat(float scale) {
        const std::int32_t signedValue =
                static_cast<std::int32_t>(Next() & 0x001fffffu) -
                0x00100000;
        return static_cast<float>(signedValue) *
               (scale / 1048576.0f);
    }
};

struct TreeFixture {
    std::unique_ptr<CPlugTree> root;
    std::array<CPlugTree *, ChildCount> children{};
    std::vector<CMwNodRef<CPlugSurfaceGeom>> geometries;
    std::vector<CMwNodRef<CPlugSurface>> surfaces;
};

TreeFixture MakeFixture(void) {
    TreeFixture fixture;
    fixture.root = std::make_unique<CPlugTree>();
    fixture.root->SetUseLocation(1);
    GmIso4 identity;
    identity.SetIdentity();
    fixture.root->SetLocation(identity);
    fixture.geometries.reserve(ChildCount);
    fixture.surfaces.reserve(ChildCount);

    for (std::size_t childIndex = 0u;
         childIndex < ChildCount;
         ++childIndex) {
        CMwNodRef<CPlugSurfaceGeom> geometry =
                MakeMwNod<CPlugSurfaceGeom>();
        auto ellipsoid = std::make_unique<GmSurfEllipsoid>();
        ellipsoid->radii = {
            0.25f + static_cast<float>(childIndex) * 0.03125f,
            0.5f + static_cast<float>(childIndex) * 0.015625f,
            0.75f + static_cast<float>(childIndex) * 0.0625f,
        };
        geometry->SetGmSurf(std::move(ellipsoid));
        CMwNodRef<CPlugSurface> surface = MakeMwNod<CPlugSurface>();
        surface->SetGeometry(geometry.Get());

        auto child = std::make_unique<CPlugTree>();
        child->SetUseLocation(1);
        child->SetLocation(identity);
        child->SetSurface(surface.Get());
        child->UpdateBoundingBox(0);
        fixture.children[childIndex] = child.get();
        fixture.root->AddOwnedChild(std::move(child));
        fixture.geometries.push_back(geometry);
        fixture.surfaces.push_back(surface);
    }
    fixture.root->UpdateBoundingBox(0);
    return fixture;
}

GmIso4 RandomTransform(XorShift32 &random,
                       std::size_t caseIndex,
                       std::size_t nodeIndex) {
    GmIso4 result;
    result.SetIdentity();
    result.rotation.basisX = {
        1.0f + random.FiniteFloat(0.25f),
        random.FiniteFloat(0.25f),
        random.FiniteFloat(0.25f),
    };
    result.rotation.basisY = {
        random.FiniteFloat(0.25f),
        1.0f + random.FiniteFloat(0.25f),
        random.FiniteFloat(0.25f),
    };
    result.rotation.basisZ = {
        random.FiniteFloat(0.25f),
        random.FiniteFloat(0.25f),
        1.0f + random.FiniteFloat(0.25f),
    };
    result.translation = {
        random.FiniteFloat(16.0f),
        random.FiniteFloat(4.0f),
        random.FiniteFloat(16.0f),
    };

    if (caseIndex < 16u) {
        static constexpr std::array<float, 8u> boundaryValues = {{
            0.0f,
            -0.0f,
            std::numeric_limits<float>::denorm_min(),
            -std::numeric_limits<float>::denorm_min(),
            std::numeric_limits<float>::min(),
            -std::numeric_limits<float>::min(),
            1.0f,
            -1.0f,
        }};
        const float boundary =
                boundaryValues[(caseIndex + nodeIndex) %
                               boundaryValues.size()];
        result.rotation.basisX.y = boundary;
        result.translation.z = boundary;
    }
    return result;
}

bool SameBits(const GmBoxAligned &left,
              const GmBoxAligned &right) {
    return std::memcmp(&left, &right, sizeof(GmBoxAligned)) == 0;
}

bool SameTreeBoxes(const TreeFixture &left,
                   const TreeFixture &right) {
    if (!SameBits(left.root->Box(), right.root->Box())) {
        return false;
    }
    for (std::size_t childIndex = 0u;
         childIndex < ChildCount;
         ++childIndex) {
        if (!SameBits(left.children[childIndex]->Box(),
                      right.children[childIndex]->Box())) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main(void) {
    TreeFixture authority = MakeFixture();
    TreeFixture optimized = MakeFixture();
    forevervalidator::simulation::OptimizedCpuVehicleCollisionBoundsPlan
            plan;
    if (!plan.TryBuild(*optimized.root) ||
        !plan.IsFor(optimized.root.get()) ||
        plan.ChildCount() != ChildCount) {
        std::fprintf(stderr, "vehicle bounds plan was not built\n");
        return 1;
    }

    XorShift32 random;
    for (std::size_t caseIndex = 0u;
         caseIndex < RandomCaseCount;
         ++caseIndex) {
        const GmIso4 rootTransform =
                RandomTransform(random, caseIndex, ChildCount);
        authority.root->SetUseLocation((caseIndex & 1u) != 0u);
        optimized.root->SetUseLocation((caseIndex & 1u) != 0u);
        authority.root->SetLocation(rootTransform);
        optimized.root->SetLocation(rootTransform);

        for (std::size_t childIndex = 0u;
             childIndex < ChildCount;
             ++childIndex) {
            const GmIso4 childTransform =
                    RandomTransform(random, caseIndex, childIndex);
            const bool usesLocation =
                    ((caseIndex + childIndex) % 3u) != 0u;
            authority.children[childIndex]->SetUseLocation(usesLocation);
            optimized.children[childIndex]->SetUseLocation(usesLocation);
            authority.children[childIndex]->SetLocation(childTransform);
            optimized.children[childIndex]->SetLocation(childTransform);
        }

        authority.root->UpdateBoundingBox(0);
        if ((caseIndex & 1u) == 0u) {
            plan.RefreshRuntimeCertified();
        } else if (!plan.TryRefresh()) {
            std::fprintf(stderr,
                         "guarded refresh rejected case %zu\n",
                         caseIndex);
            return 1;
        }
        if (!SameTreeBoxes(authority, optimized)) {
            std::fprintf(stderr,
                         "vehicle bounds case %zu differs\n",
                         caseIndex);
            return 1;
        }
    }

    const GmBoxAligned savedRootBounds = optimized.root->Box();
    std::array<GmBoxAligned, ChildCount> savedChildBounds{};
    for (std::size_t childIndex = 0u;
         childIndex < ChildCount;
         ++childIndex) {
        savedChildBounds[childIndex] =
                optimized.children[childIndex]->Box();
    }
    CMwNodRef<CPlugSurfaceGeom> replacementGeometry =
            MakeMwNod<CPlugSurfaceGeom>();
    auto replacementEllipsoid = std::make_unique<GmSurfEllipsoid>();
    replacementEllipsoid->radii = {2.0f, 3.0f, 4.0f};
    replacementGeometry->SetGmSurf(std::move(replacementEllipsoid));
    optimized.surfaces[3u]->SetGeometry(replacementGeometry.Get());
    if (plan.TryRefresh() ||
        !SameBits(savedRootBounds, optimized.root->Box())) {
        std::fprintf(stderr,
                     "mutated geometry did not cleanly reject\n");
        return 1;
    }
    for (std::size_t childIndex = 0u;
         childIndex < ChildCount;
         ++childIndex) {
        if (!SameBits(savedChildBounds[childIndex],
                      optimized.children[childIndex]->Box())) {
            std::fprintf(stderr,
                         "rejected refresh partially changed child %zu\n",
                         childIndex);
            return 1;
        }
    }

    CPlugTree unrelated;
    if (plan.IsFor(&unrelated)) {
        std::fprintf(stderr, "plan accepted unrelated root\n");
        return 1;
    }
    forevervalidator::simulation::OptimizedCpuVehicleCollisionBoundsPlan
            unsupportedPlan;
    CPlugTree unsupportedRoot;
    auto onlyChild = std::make_unique<CPlugTree>();
    unsupportedRoot.AddOwnedChild(std::move(onlyChild));
    if (unsupportedPlan.TryBuild(unsupportedRoot) ||
        unsupportedPlan.IsAvailable()) {
        std::fprintf(stderr, "unsupported tree retained a plan\n");
        return 1;
    }

    std::printf(
            "vehicle_collision_bounds_cases=%zu result=identical\n",
            RandomCaseCount);
    return 0;
}
