#include "simulation/backends/optimized_cpu/optimized_cpu_vehicle_collision_bounds_plan.h"

#include <cmath>
#include <cstring>
#include <typeinfo>

#include "engine/rendering/plug_tree.h"

namespace forevervalidator::simulation {

namespace {

inline float OrderedRowProduct(float x,
                               float xValue,
                               float y,
                               float yValue,
                               float z,
                               float zValue) noexcept {
    const float xy = x * xValue + y * yValue;
    return xy + z * zValue;
}

inline void TransformCertifiedBox(GmBoxAligned &output,
                                  const GmBoxAligned &source,
                                  const GmIso4 &transform) noexcept {
    const GmMat3 &rotation = transform.rotation;
    const GmVec3 &sourceCenter = source.center;
    const GmVec3 &sourceHalf = source.halfExtents;

    const float centerX = OrderedRowProduct(
            rotation.basisX.x, sourceCenter.x,
            rotation.basisY.x, sourceCenter.y,
            rotation.basisZ.x, sourceCenter.z);
    const float centerY = OrderedRowProduct(
            rotation.basisX.y, sourceCenter.x,
            rotation.basisY.y, sourceCenter.y,
            rotation.basisZ.y, sourceCenter.z);
    const float centerZ = OrderedRowProduct(
            rotation.basisX.z, sourceCenter.x,
            rotation.basisY.z, sourceCenter.y,
            rotation.basisZ.z, sourceCenter.z);
    output.center = {
        centerX + transform.translation.x,
        centerY + transform.translation.y,
        centerZ + transform.translation.z,
    };

    output.halfExtents = {
        OrderedRowProduct(
                std::fabs(rotation.basisX.x), sourceHalf.x,
                std::fabs(rotation.basisY.x), sourceHalf.y,
                std::fabs(rotation.basisZ.x), sourceHalf.z),
        OrderedRowProduct(
                std::fabs(rotation.basisX.y), sourceHalf.x,
                std::fabs(rotation.basisY.y), sourceHalf.y,
                std::fabs(rotation.basisZ.y), sourceHalf.z),
        OrderedRowProduct(
                std::fabs(rotation.basisX.z), sourceHalf.x,
                std::fabs(rotation.basisY.z), sourceHalf.y,
                std::fabs(rotation.basisZ.z), sourceHalf.z),
    };
}

// The plan only accepts geometry boxes that are valid for tree refresh.
// GmBoxAligned::SetMult applies absolute rotation elements to non-negative
// half-extents, so a finite transformed child remains valid. Preserve the
// authoritative ordered union arithmetic while avoiding the generic invalid-
// box guards and out-of-line call on every certified child.
inline void AddCertifiedBox(GmBoxAligned &merged,
                            const GmBoxAligned &other) noexcept {
    if (other.halfExtents.x < 0.0f) {
        // Defensive parity for a corrupted runtime transform. The certified
        // vehicle path never takes this branch.
        merged.AddValidPlugTreeBox(other);
        return;
    }

    GmVec3 minPoint = {
        merged.center.x - merged.halfExtents.x,
        merged.center.y - merged.halfExtents.y,
        merged.center.z - merged.halfExtents.z,
    };
    GmVec3 maxPoint = {
        merged.center.x + merged.halfExtents.x,
        merged.center.y + merged.halfExtents.y,
        merged.center.z + merged.halfExtents.z,
    };
    float otherX = other.center.x - other.halfExtents.x;
    float otherY = other.center.y - other.halfExtents.y;
    float otherZ = other.center.z - other.halfExtents.z;
    if (minPoint.x > otherX) {
        minPoint.x = otherX;
    }
    if (minPoint.y > otherY) {
        minPoint.y = otherY;
    }
    if (minPoint.z > otherZ) {
        minPoint.z = otherZ;
    }

    otherX = other.center.x + other.halfExtents.x;
    otherY = other.center.y + other.halfExtents.y;
    otherZ = other.center.z + other.halfExtents.z;
    if (maxPoint.x < otherX) {
        maxPoint.x = otherX;
    }
    if (maxPoint.y < otherY) {
        maxPoint.y = otherY;
    }
    if (maxPoint.z < otherZ) {
        maxPoint.z = otherZ;
    }
    merged.SetMinMax(minPoint, maxPoint);
}

}  // namespace

bool OptimizedCpuVehicleCollisionBoundsPlan::TryBuild(
        CPlugTree &root) noexcept {
    OptimizedCpuVehicleCollisionBoundsPlan candidate;
    const std::size_t childCount = root.GetChildCount();
    if (typeid(root) != typeid(CPlugTree) ||
        root.Visual() != nullptr || root.Surface() != nullptr ||
        childCount < 2u || childCount > MaxChildCount) {
        Clear();
        return false;
    }

    candidate.root_ = &root;
    candidate.childCount_ = childCount;
    for (std::size_t childIndex = 0u;
         childIndex < childCount;
         ++childIndex) {
        CPlugTree *child = root.GetChild(childIndex);
        CPlugSurface *surface = child != nullptr ? child->Surface() : nullptr;
        CPlugSurfaceGeom *geometry =
                surface != nullptr ? surface->GeometryNode() : nullptr;
        if (child == nullptr || typeid(*child) != typeid(CPlugTree) ||
            child->GetChildCount() != 0u || child->Visual() != nullptr ||
            surface == nullptr || geometry == nullptr ||
            !geometry->Bounds().IsValidForPlugTreeRefresh()) {
            Clear();
            return false;
        }
        candidate.children_[childIndex] = {
            child,
            surface,
            geometry,
            geometry->Bounds(),
        };
    }
    *this = candidate;
    return true;
}

bool OptimizedCpuVehicleCollisionBoundsPlan::TryRefresh(void) const noexcept {
    if (!IsAvailable() || root_->GetChildCount() != childCount_ ||
        root_->Visual() != nullptr || root_->Surface() != nullptr) {
        return false;
    }

    // Prove every source and operation target before changing any box. If a
    // caller mutates topology or geometry, the authoritative recursive path
    // can therefore run without needing to repair a partial transition.
    for (std::size_t childIndex = 0u;
         childIndex < childCount_;
         ++childIndex) {
        const Child &child = children_[childIndex];
        if (root_->GetChild(childIndex) != child.tree ||
            child.tree->GetChildCount() != 0u ||
            child.tree->Visual() != nullptr ||
            child.tree->Surface() != child.surface ||
            child.surface->GeometryNode() != child.geometry ||
            std::memcmp(&child.geometry->Bounds(),
                        &child.geometryBounds,
                        sizeof(GmBoxAligned)) != 0) {
            return false;
        }
    }

    RefreshUnchecked();
    return true;
}

void OptimizedCpuVehicleCollisionBoundsPlan::
RefreshRuntimeCertified(void) const noexcept {
    RefreshUnchecked();
}

void OptimizedCpuVehicleCollisionBoundsPlan::RefreshUnchecked(
        void) const noexcept {
    GmBoxAligned merged;
    for (std::size_t childIndex = 0u;
         childIndex < childCount_;
         ++childIndex) {
        const Child &child = children_[childIndex];
        GmBoxAligned childBounds;
        if (child.tree->HasLocalTransform()) {
            TransformCertifiedBox(
                    childBounds, child.geometryBounds, child.tree->LocalIso());
        } else {
            childBounds = child.geometryBounds;
        }
        child.tree->SetTreeBounds(childBounds);
        if (childIndex == 0u) {
            merged = childBounds;
        } else {
            AddCertifiedBox(merged, childBounds);
        }
    }

    GmBoxAligned rootBounds;
    if (root_->HasLocalTransform()) {
        TransformCertifiedBox(rootBounds, merged, root_->LocalIso());
    } else {
        rootBounds = merged;
    }
    root_->SetTreeBounds(rootBounds);
}

void OptimizedCpuVehicleCollisionBoundsPlan::Clear(void) noexcept {
    root_ = nullptr;
    childCount_ = 0u;
}

}  // namespace forevervalidator::simulation
