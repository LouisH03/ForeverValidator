#include "simulation/backends/optimized_cpu/optimized_cpu_vehicle_collision_bounds_plan.h"

#include <cstring>
#include <typeinfo>

#include "engine/rendering/plug_tree.h"

namespace forevervalidator::simulation {

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
            childBounds.SetMult(
                    child.geometryBounds, child.tree->LocalIso());
        } else {
            childBounds = child.geometryBounds;
        }
        child.tree->SetTreeBounds(childBounds);
        if (childIndex == 0u) {
            merged = childBounds;
        } else {
            merged.AddValidPlugTreeBox(childBounds);
        }
    }

    GmBoxAligned rootBounds;
    if (root_->HasLocalTransform()) {
        rootBounds.SetMult(merged, root_->LocalIso());
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
