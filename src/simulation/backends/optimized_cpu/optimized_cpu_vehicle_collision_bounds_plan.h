#pragma once

#include <array>
#include <cstddef>

#include "engine/core/gm_types.h"

struct CPlugSurface;
struct CPlugSurfaceGeom;
struct CPlugTree;

namespace forevervalidator::simulation {

// Compiles the common Stadium vehicle collision-tree shape: one undecorated
// root and a fixed ordered list of direct surface leaves. Refresh replays the
// authoritative scalar box transforms and ordered unions exactly, but avoids
// recursive virtual traversal and repeated decoration discovery.
class OptimizedCpuVehicleCollisionBoundsPlan final {
public:
    static constexpr std::size_t MaxChildCount = 16u;

    bool TryBuild(CPlugTree &root) noexcept;
    bool TryRefresh(void) const noexcept;
    void RefreshRuntimeCertified(void) const noexcept;
    void Clear(void) noexcept;

    bool IsAvailable(void) const noexcept {
        return root_ != nullptr && childCount_ >= 2u;
    }

    std::size_t ChildCount(void) const noexcept {
        return childCount_;
    }

    bool IsFor(const CPlugTree *root) const noexcept {
        return IsAvailable() && root_ == root;
    }

private:
    void RefreshUnchecked(void) const noexcept;

    struct Child {
        CPlugTree *tree = nullptr;
        CPlugSurface *surface = nullptr;
        CPlugSurfaceGeom *geometry = nullptr;
        GmBoxAligned geometryBounds{};
    };

    CPlugTree *root_ = nullptr;
    std::array<Child, MaxChildCount> children_{};
    std::size_t childCount_ = 0u;
};

}  // namespace forevervalidator::simulation
