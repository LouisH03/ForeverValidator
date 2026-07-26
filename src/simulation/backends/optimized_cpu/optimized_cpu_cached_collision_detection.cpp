// OptimizedCpu static collision traversal with immutable transform sidecars.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <typeinfo>

#include "engine/physics/collision/gm_collision_buffer.h"
#include "engine/physics/collision/hms_collision_manager.h"
#include "engine/physics/dynamics/hms_corpus.h"
#include "engine/physics/dynamics/hms_item.h"
#include "engine/physics/geometry/plug_surface.h"
#include "engine/rendering/plug_tree.h"
#include "simulation/backends/optimized_cpu/optimized_cpu_static_surface_transform_cache.h"
#include "simulation/backends/optimized_cpu/optimized_cpu_static_triangle_mesh_query.h"
#include "simulation/backends/optimized_cpu/optimized_cpu_native_binary32_collision.h"
#include "simulation/backends/optimized_cpu/optimized_cpu_ellipsoid_mesh_packet.h"
#include "simulation/backends/optimized_cpu/optimized_cpu_static_bounds_overlap.h"

#if defined(__i386__) || defined(__x86_64__)
#include <xmmintrin.h>
#endif

using forevervalidator::simulation::OptimizedCpuStaticBoundsOverlap;

namespace {

constexpr std::size_t EllipsoidPacketWidth = 8u;

struct EllipsoidPacketTraversalLane {
    CPlugTree *tree = nullptr;
    CPlugSurface *surface = nullptr;
    GmIso4 location{};
    GmBoxAligned bounds{};
    LocatedGmSurf located{};
    SHmsSphereBufferContact *sphereContact = nullptr;
    CHmsCollisionBuffer *buffer = nullptr;
    u32 temporalSlotOrdinal = 0u;
    bool collided = false;
};

bool CollectEllipsoidPacketTraversalLanes(
        const GmIso4 &parentIso,
        const CPlugTree &tree,
        std::array<EllipsoidPacketTraversalLane, EllipsoidPacketWidth> *lanes,
        std::size_t *laneCount,
        u32 *nextTemporalSlotOrdinal) {
    const u32 temporalSlotOrdinal = (*nextTemporalSlotOrdinal)++;
    if (!tree.HasWorldBox()) {
        return true;
    }

    GmIso4 localIso;
    tree.ComposeCollisionIso(parentIso, localIso);
    const u32 childCount = tree.GetChildCount();
    for (u32 childIndex = 0u; childIndex < childCount; ++childIndex) {
        if (!CollectEllipsoidPacketTraversalLanes(
                    localIso,
                    *tree.GetChild(childIndex),
                    lanes,
                    laneCount,
                    nextTemporalSlotOrdinal)) {
            return false;
        }
    }

    CPlugSurface *surface = tree.Surface();
    if (surface == nullptr) {
        return true;
    }
    const GmSurf *geometry = surface->Geometry();
    if (geometry == nullptr || typeid(*geometry) != typeid(GmSurfEllipsoid) ||
        !surface->UsesSphereContactBuffer() ||
        *laneCount >= EllipsoidPacketWidth) {
        return false;
    }

    EllipsoidPacketTraversalLane &lane = (*lanes)[(*laneCount)++];
    lane.tree = const_cast<CPlugTree *>(&tree);
    lane.surface = surface;
    lane.location = localIso;
    tree.GetTransformedCollisionBox(parentIso, lane.bounds);
    lane.temporalSlotOrdinal = temporalSlotOrdinal;
    return true;
}

void CompletePacketCollisionMaterials(
        CHmsCollisionBuffer &buffer,
        u32 firstNew,
        CPlugSurface &movingSurface,
        CPlugSurface &staticSurface) {
    const u32 count = buffer.PhysicalCollisionCount();
    for (u32 collisionIndex = firstNew;
         collisionIndex < count;
         ++collisionIndex) {
        GmCollision &collision = buffer.GetCollision(collisionIndex);
        collision.materialA =
                movingSurface.SurfaceMaterialIdFromLocalIndex(
                        collision.localMaterialA);
        collision.materialB =
                staticSurface.SurfaceMaterialIdFromLocalIndex(
                        collision.localMaterialB);
    }
}

bool DetectEllipsoidPacketAgainstStaticGroup(
        CHmsCollisionManagerSZone &zone,
        const GmIso4 &movingIso,
        const CPlugTree &movingTree,
        const OptimizedCpuMovingEllipsoidPacketPlan *movingPlan,
        const OptimizedCpuStaticSurfaceTransformGroup &transforms,
        GmOctree<CHmsCollisionManagerSColOctreeCell> &staticTrees) {
    if (!OptimizedCpuEllipsoidMeshPacketAvailable()) {
        return false;
    }

    std::array<EllipsoidPacketTraversalLane, EllipsoidPacketWidth> lanes{};
    std::size_t laneCount = 0u;
    if (movingPlan != nullptr) {
        std::array<
                GmIso4,
                OptimizedCpuMovingEllipsoidPacketPlan::MaxNodeCount>
                nodeLocations;
        const auto *planNodes = movingPlan->NodeData();
        const auto *planLanes = movingPlan->LaneData();
        const auto *operations = movingPlan->OperationData();
        for (std::size_t operationIndex = 0u;
             operationIndex < movingPlan->OperationCount();
             ++operationIndex) {
            const OptimizedCpuMovingEllipsoidPacketPlan::Operation
                    &operation = operations[operationIndex];
            if (operation.kind ==
                OptimizedCpuMovingEllipsoidPacketPlan::OperationKind::
                        ComposeNode) {
                const OptimizedCpuMovingEllipsoidPacketPlan::Node &node =
                        planNodes[operation.index];
                const GmIso4 &parentLocation =
                        node.parentNodeIndex ==
                                        OptimizedCpuMovingEllipsoidPacketPlan::
                                                NoParent
                                ? movingIso
                                : nodeLocations[node.parentNodeIndex];
                GmIso4 &location = nodeLocations[operation.index];
                if (node.usesLocalTransform) {
                    location = node.tree->LocalIso();
                    location.Mult(parentLocation);
                } else {
                    location = parentLocation;
                }
                continue;
            }

            const OptimizedCpuMovingEllipsoidPacketPlan::Lane &planLane =
                    planLanes[operation.index];
            const OptimizedCpuMovingEllipsoidPacketPlan::Node &node =
                    planNodes[planLane.nodeIndex];
            const GmIso4 &parentLocation =
                    node.parentNodeIndex ==
                                    OptimizedCpuMovingEllipsoidPacketPlan::
                                            NoParent
                            ? movingIso
                            : nodeLocations[node.parentNodeIndex];
            EllipsoidPacketTraversalLane &lane = lanes[laneCount++];
            lane.tree = planLane.tree;
            lane.surface = planLane.surface;
            lane.location = nodeLocations[planLane.nodeIndex];
            planLane.tree->GetTransformedCollisionBox(
                    parentLocation, lane.bounds);
            lane.temporalSlotOrdinal = planLane.temporalSlotOrdinal;
        }
    } else {
        u32 nextTemporalSlotOrdinal = 0u;
        if (!CollectEllipsoidPacketTraversalLanes(
                    movingIso,
                    movingTree,
                    &lanes,
                    &laneCount,
                    &nextTemporalSlotOrdinal) ||
            laneCount < 2u) {
            return false;
        }
    }

    std::array<
            OptimizedCpuStaticSurfaceTransformGroup::TemporalCandidateSpan,
            EllipsoidPacketWidth>
            candidateSpans{};
    std::array<std::size_t, EllipsoidPacketWidth> candidateOffsets{};
    for (std::size_t laneIndex = 0u;
         laneIndex < laneCount;
         ++laneIndex) {
        if (!transforms.TemporalCandidateSpanFor(
                    *lanes[laneIndex].tree,
                    lanes[laneIndex].temporalSlotOrdinal,
                    lanes[laneIndex].bounds,
                    &candidateSpans[laneIndex])) {
            return false;
        }
    }

    std::array<OptimizedCpuEllipsoidMeshPacketLane, EllipsoidPacketWidth>
            packetLanes{};
    for (std::size_t laneIndex = 0u;
         laneIndex < laneCount;
         ++laneIndex) {
        EllipsoidPacketTraversalLane &lane = lanes[laneIndex];
        lane.buffer = zone.ChooseCollisionOutputBuffer(
                lane.tree, lane.surface, &lane.sphereContact);
        if (lane.buffer == nullptr || lane.sphereContact == nullptr) {
            return false;
        }
        lane.located = {
            lane.surface->Geometry(),
            &lane.location,
            1,
        };
        packetLanes[laneIndex] = {&lane.located, lane.buffer};
    }
    OptimizedCpuPreparedEllipsoidMeshPacket preparedPacket;
    const std::uint32_t allLaneMask =
            (1u << static_cast<std::uint32_t>(laneCount)) - 1u;
    if (!PrepareOptimizedCpuEllipsoidMeshPacket(
                packetLanes.data(),
                laneCount,
                allLaneMask,
                &preparedPacket)) {
        return false;
    }

    for (;;) {
        u32 staticTreeIndex = std::numeric_limits<u32>::max();
        for (std::size_t laneIndex = 0u;
             laneIndex < laneCount;
             ++laneIndex) {
            const auto &span = candidateSpans[laneIndex];
            const std::size_t offset = candidateOffsets[laneIndex];
            if (offset < span.size) {
                staticTreeIndex = std::min(
                        staticTreeIndex, span.data[offset]);
            }
        }
        if (staticTreeIndex == std::numeric_limits<u32>::max()) {
            break;
        }

        CHmsCollisionManagerSColOctreeCell *record =
                &staticTrees[staticTreeIndex];
        std::uint32_t activeMask = 0u;
        for (std::size_t laneIndex = 0u;
             laneIndex < laneCount;
             ++laneIndex) {
            const auto &span = candidateSpans[laneIndex];
            std::size_t &offset = candidateOffsets[laneIndex];
            if (offset >= span.size ||
                span.data[offset] != staticTreeIndex) {
                continue;
            }
            ++offset;
            if (OptimizedCpuStaticBoundsOverlap(
                        lanes[laneIndex].bounds,
                        record->Bounds())) {
                activeMask |= 1u << laneIndex;
            }
        }
        if (activeMask == 0u) {
            continue;
        }

        const CHmsCollisionManagerSColOctreeCell::StaticSurface &staticSurface =
                record->SurfaceData();
        const OptimizedCpuStaticMeshTriangleSidecar *triangleSidecar =
                transforms.TriangleSidecarAt(staticTreeIndex);
        const OptimizedCpuCertifiedStaticMeshPacket *certifiedMesh =
                transforms.CertifiedMeshPacketAt(staticTreeIndex);
        bool packetHandled = false;
        if (certifiedMesh != nullptr) {
            std::array<u32, EllipsoidPacketWidth> firstNew{};
            for (std::size_t laneIndex = 0u;
                 laneIndex < laneCount;
                 ++laneIndex) {
                if ((activeMask & (1u << laneIndex)) == 0u) {
                    continue;
                }
                firstNew[laneIndex] =
                        lanes[laneIndex].buffer->PhysicalCollisionCount();
            }
            std::uint32_t hitMask = 0u;
            packetHandled =
                    GmCollision_PreparedEllipsoidPacket_Mesh_InlineMathOptimizedCpuNativeBinary32WithCertifiedStaticMesh(
                            preparedPacket,
                            activeMask,
                            *certifiedMesh,
                            &hitMask);
            if (packetHandled) {
                for (std::size_t laneIndex = 0u;
                     laneIndex < laneCount;
                     ++laneIndex) {
                    if ((hitMask & (1u << laneIndex)) == 0u) {
                        continue;
                    }
                    EllipsoidPacketTraversalLane &lane = lanes[laneIndex];
                    CompletePacketCollisionMaterials(
                            *lane.buffer,
                            firstNew[laneIndex],
                            *lane.surface,
                            *staticSurface.surface);
                    zone.TagNewStaticCollisions(
                            lane.buffer,
                            firstNew[laneIndex],
                            lane.tree,
                            record);
                    lane.collided = true;
                }
            }
        }

        if (!packetHandled) {
            for (std::size_t laneIndex = 0u;
                 laneIndex < laneCount;
                 ++laneIndex) {
                if ((activeMask & (1u << laneIndex)) == 0u) {
                    continue;
                }
                EllipsoidPacketTraversalLane &lane = lanes[laneIndex];
                const u32 firstNew =
                        lane.buffer->PhysicalCollisionCount();
                const SPlugSurfaceLocatedPair surfacePair = {
                    *lane.surface,
                    lane.location,
                    *staticSurface.surface,
                    staticSurface.location,
                };
                const int collided =
                        ComputePlugSurfaceCollisionInlineMathOptimizedCpuNativeBinary32WithStaticCache(
                                surfacePair,
                                transforms.InverseAt(staticTreeIndex),
                                triangleSidecar,
                                *lane.buffer);
                if (collided == 0) {
                    continue;
                }
                zone.TagNewStaticCollisions(
                        lane.buffer,
                        firstNew,
                        lane.tree,
                        record);
                lane.collided = true;
            }
        }
    }

    for (std::size_t laneIndex = 0u;
         laneIndex < laneCount;
         ++laneIndex) {
        if (lanes[laneIndex].collided) {
            zone.AddSphereContactOnce(lanes[laneIndex].sphereContact);
        }
    }
    return true;
}

bool TrySkipWholeTreeBoundsEmpty(
        const GmIso4 &movingIso,
        const CPlugTree &movingTree,
        const OptimizedCpuStaticSurfaceTransformGroup &transforms) {
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
    const unsigned int savedMxcsr = _mm_getcsr();
    constexpr unsigned int MxcsrControlMask = 0xffc0u;
    constexpr unsigned int DeterministicMxcsrControl = 0x1f80u;
    constexpr unsigned int InexactStatus = 0x20u;
    if ((savedMxcsr & MxcsrControlMask) != DeterministicMxcsrControl ||
        (savedMxcsr & InexactStatus) == 0u ||
        !transforms.BroadPhaseArithmeticIsBoundedFor(
                movingTree, movingIso)) {
        return false;
    }
    GmBoxAligned movingBounds;
    movingTree.GetTransformedCollisionBox(movingIso, movingBounds);
    const bool empty = !transforms.WholeTreeBoundsOverlapAnySurface(
            movingBounds);
    return empty;
#else
    (void)movingIso;
    (void)movingTree;
    (void)transforms;
    return false;
#endif
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((hot, noinline))
#endif
void DetectNativeBinary32CachedTemporalSpan(
        CHmsCollisionManagerSZone &zone,
        const GmBoxAligned &movingBox,
        CPlugTree &movingTree,
        CPlugSurface &movingSurface,
        const GmIso4 &movingLocation,
        const OptimizedCpuStaticSurfaceTransformGroup &transforms,
        const OptimizedCpuStaticSurfaceTransformGroup::TemporalCandidateSpan
                &temporalCandidates) {
    const u32 *candidate = temporalCandidates.data;
    const u32 *const end = candidate + temporalCandidates.size;
    const CHmsCollisionManagerSColOctreeCell *const records =
            transforms.RecordData();
    const GmIso4 *const inverses = transforms.InverseData();
    const OptimizedCpuStaticMeshTriangleSidecar *const *const
            triangleSidecars = transforms.TriangleSidecarData();
    for (; candidate != end; ++candidate) {
        const u32 staticTreeIndex = *candidate;
        const CHmsCollisionManagerSColOctreeCell *record =
                &records[staticTreeIndex];
        if (!OptimizedCpuStaticBoundsOverlap(
                    movingBox, record->Bounds())) {
            continue;
        }

        // The span contains only surface records, and the advance certificate
        // proves every static surface tree is collision-enabled.
        const CHmsCollisionManagerSColOctreeCell::StaticSurface
                &staticSurface = record->SurfaceData();
        SHmsSphereBufferContact *sphereContact = nullptr;
        CHmsCollisionBuffer *buffer = zone.ChooseCollisionOutputBuffer(
                &movingTree, &movingSurface, &sphereContact);
        const u32 firstNew = buffer->PhysicalCollisionCount();
        const SPlugSurfaceLocatedPair surfacePair = {
            movingSurface,
            movingLocation,
            *staticSurface.surface,
            staticSurface.location,
        };
        const int collided =
                ComputePlugSurfaceCollisionInlineMathOptimizedCpuNativeBinary32WithStaticCache(
                        surfacePair,
                        inverses[staticTreeIndex],
                        triangleSidecars[staticTreeIndex],
                        *buffer);
        if (collided == 0) {
            continue;
        }
        if (sphereContact != nullptr) {
            zone.AddSphereContactOnce(sphereContact);
        }
        zone.TagNewStaticCollisions(
                buffer, firstNew, &movingTree, record);
    }
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((cold, noinline))
#endif
void DetectNativeBinary32CachedColdFallback(
        CHmsCollisionManagerSZone &zone,
        const GmBoxAligned &movingBox,
        CPlugTree &movingTree,
        CPlugSurface &movingSurface,
        const GmIso4 &movingLocation,
        const OptimizedCpuStaticSurfaceTransformGroup &transforms,
        GmOctree<CHmsCollisionManagerSColOctreeCell> &staticTrees) {
    const auto processSurfaceRecord = [&](u32 staticTreeIndex) {
        CHmsCollisionManagerSColOctreeCell *record =
                &staticTrees[staticTreeIndex];
        if (!OptimizedCpuStaticBoundsOverlap(
                    movingBox, record->Bounds()) ||
            !record->ContainsSurface()) {
            return;
        }

        const CHmsCollisionManagerSColOctreeCell::StaticSurface
                &staticSurface = record->SurfaceData();
        SHmsSphereBufferContact *sphereContact = nullptr;
        CHmsCollisionBuffer *buffer = zone.ChooseCollisionOutputBuffer(
                &movingTree, &movingSurface, &sphereContact);
        const u32 firstNew = buffer->PhysicalCollisionCount();
        const SPlugSurfaceLocatedPair surfacePair = {
            movingSurface,
            movingLocation,
            *staticSurface.surface,
            staticSurface.location,
        };
        const int collided =
                ComputePlugSurfaceCollisionInlineMathOptimizedCpuNativeBinary32WithStaticCache(
                        surfacePair,
                        transforms.InverseAt(staticTreeIndex),
                        transforms.TriangleSidecarAt(staticTreeIndex),
                        *buffer);
        if (collided == 0) {
            return;
        }
        if (sphereContact != nullptr) {
            zone.AddSphereContactOnce(sphereContact);
        }
        zone.TagNewStaticCollisions(
                buffer, firstNew, &movingTree, record);
    };

    const u32 staticTreeCount = staticTrees.GetCount();
    for (u32 staticTreeIndex = 0u;
         staticTreeIndex < staticTreeCount;) {
        CHmsCollisionManagerSColOctreeCell *record =
                &staticTrees[staticTreeIndex];
        if (!OptimizedCpuStaticBoundsOverlap(
                    movingBox, record->Bounds())) {
            staticTreeIndex += record->SubtreeEntryCount();
            continue;
        }
        processSurfaceRecord(staticTreeIndex);
        ++staticTreeIndex;
    }
}

}  // namespace

void CHmsCollisionManager::SZone::DetectCollisionsCorpusOptimizedCpuCached(
        CHmsCollisionBuffer &collisionBuffer,
        CHmsCorpus *corpus,
        const OptimizedCpuStaticSurfaceTransformCache &transforms) {
    activeCollisionBuffer = &collisionBuffer;

    const u32 groupIndex = corpus->Item()->CollisionGroup();
    CHmsCollisionManagerSGroup *group = &groups[groupIndex - 1u];

    for (CHmsCollisionManagerSAgainstGroup &againstEntry :
         group->againstGroups) {
        CHmsCollisionManagerSAgainstGroup *against = &againstEntry;
        activeCollisionGroupPair = against->collisionGroupPair;

        for (const SGroup::MovingCorpusState &target :
             against->targetGroup->movingCorpuses) {
            CHmsCorpus *other = target.corpus;
            if (against->collisionSchedule.IsEnabled(*corpus, *other)) {
                DetectCollisionBetween(corpus, other);
            }
        }

        activeStaticTargetGroup = against->targetGroup;
        if (against->targetGroup->StaticTreeCount() > 1u) {
            activeCorpusA = corpus;
            const OptimizedCpuStaticSurfaceTransformGroup *groupTransforms =
                    transforms.GroupFor(*against->targetGroup);
            if (groupTransforms == nullptr) {
                DetectCollisionBetweenTreeAndStaticCollisionTreeOptimizedCpu(
                        *corpus->LocationIso(),
                        *corpus->CollisionTree());
            } else {
                u32 nextTemporalSlotOrdinal = 0u;
                DetectCollisionBetweenTreeAndStaticCollisionTreeOptimizedCpuCached(
                        *corpus->LocationIso(),
                        *corpus->CollisionTree(),
                        nextTemporalSlotOrdinal,
                        *groupTransforms);
            }
        }
    }

    MergeQueuedSphereContacts(collisionBuffer);
}

void CHmsCollisionManager::SZone::
DetectCollisionBetweenTreeAndStaticCollisionTreeOptimizedCpuCached(
        const GmIso4 &movingIsoRef,
        const CPlugTree &movingTree,
        u32 &nextTemporalSlotOrdinal,
        const OptimizedCpuStaticSurfaceTransformGroup &transforms) {
    CHmsCollisionManagerSZone *zone = this;
    const GmIso4 *movingIso = &movingIsoRef;
    const u32 temporalSlotOrdinal = nextTemporalSlotOrdinal++;
    if (!movingTree.HasWorldBox()) {
        return;
    }

    GmIso4 localIso;
    movingTree.ComposeCollisionIso(*movingIso, localIso);

    const u32 childCount = movingTree.GetChildCount();
    for (u32 childIndex = 0u; childIndex < childCount; ++childIndex) {
        zone->DetectCollisionBetweenTreeAndStaticCollisionTreeOptimizedCpuCached(
                localIso,
                *movingTree.GetChild(childIndex),
                nextTemporalSlotOrdinal,
                transforms);
    }

    CPlugSurface *movingSurface = movingTree.Surface();
    if (movingSurface == nullptr) {
        return;
    }
    CPlugTree *movingTreeNode = const_cast<CPlugTree *>(&movingTree);

    GmBoxAligned movingBox;
    movingTree.GetTransformedCollisionBox(*movingIso, movingBox);

    GmOctree<CHmsCollisionManagerSColOctreeCell> &staticTrees =
            zone->activeStaticTargetGroup->staticTrees;
    const u32 staticTreeCount = staticTrees.GetCount();
    const auto processSurfaceRecord = [&](u32 staticTreeIndex,
                                          bool surfaceIsKnown) {
        CHmsCollisionManagerSColOctreeCell *record =
                &staticTrees[staticTreeIndex];
        if (!OptimizedCpuStaticBoundsOverlap(movingBox, record->Bounds()) ||
            (!surfaceIsKnown && !record->ContainsSurface())) {
            return;
        }

        const SColOctreeCell::StaticSurface &staticSurface =
                record->SurfaceData();
        SHmsSphereBufferContact *sphereContact = nullptr;
        CHmsCollisionBuffer *buffer = zone->ChooseCollisionOutputBuffer(
                movingTreeNode, movingSurface, &sphereContact);
        const u32 firstNew = buffer->PhysicalCollisionCount();
        const SPlugSurfaceLocatedPair surfacePair = {
            *movingSurface,
            localIso,
            *staticSurface.surface,
            staticSurface.location,
        };

        const int surfaceCollisionResult =
                ComputeCollisionOptimizedCpuWithStaticMeshTriangleSidecar(
                        surfacePair,
                        transforms.InverseAt(staticTreeIndex),
                        transforms.TriangleSidecarAt(staticTreeIndex),
                        *buffer);
        if (surfaceCollisionResult) {
            if (sphereContact != nullptr) {
                zone->AddSphereContactOnce(sphereContact);
            }
            zone->TagNewStaticCollisions(
                    buffer, firstNew, movingTreeNode, record);
        }
    };

    OptimizedCpuStaticSurfaceTransformGroup::TemporalCandidateSpan
            temporalCandidates;
    if (transforms.TemporalCandidateSpanFor(
                movingTree,
                temporalSlotOrdinal,
                movingBox,
                &temporalCandidates)) {
        for (std::size_t candidateIndex = 0u;
             candidateIndex < temporalCandidates.size;
             ++candidateIndex) {
            processSurfaceRecord(
                    temporalCandidates.data[candidateIndex], true);
        }
        return;
    }

    for (u32 staticTreeIndex = 0u;
         staticTreeIndex < staticTreeCount;) {
        CHmsCollisionManagerSColOctreeCell *record =
                &staticTrees[staticTreeIndex];
        if (!OptimizedCpuStaticBoundsOverlap(movingBox, record->Bounds())) {
            staticTreeIndex += record->SubtreeEntryCount();
            continue;
        }

        processSurfaceRecord(staticTreeIndex, false);

        ++staticTreeIndex;
    }
}

void CHmsCollisionManager::SZone::
DetectCollisionsCorpusOptimizedCpuNativeBinary32Cached(
        CHmsCollisionBuffer &collisionBuffer,
        CHmsCorpus *corpus,
        const OptimizedCpuStaticSurfaceTransformCache &transforms) {
    activeCollisionBuffer = &collisionBuffer;

    const u32 groupIndex = corpus->Item()->CollisionGroup();
    CHmsCollisionManagerSGroup *group = &groups[groupIndex - 1u];

    for (CHmsCollisionManagerSAgainstGroup &againstEntry :
         group->againstGroups) {
        CHmsCollisionManagerSAgainstGroup *against = &againstEntry;
        activeCollisionGroupPair = against->collisionGroupPair;

        for (const SGroup::MovingCorpusState &target :
             against->targetGroup->movingCorpuses) {
            CHmsCorpus *other = target.corpus;
            if (against->collisionSchedule.IsEnabled(*corpus, *other)) {
                DetectCollisionBetween(corpus, other);
            }
        }

        activeStaticTargetGroup = against->targetGroup;
        if (against->targetGroup->StaticTreeCount() > 1u) {
            activeCorpusA = corpus;
            const OptimizedCpuStaticSurfaceTransformGroup *groupTransforms =
                    transforms.GroupFor(*against->targetGroup);
            if (groupTransforms == nullptr) {
                DetectCollisionBetweenTreeAndStaticCollisionTreeOptimizedCpuNativeBinary32(
                        *corpus->LocationIso(),
                        *corpus->CollisionTree());
            } else {
                bool empty = false;
                if (groupTransforms->ShouldRefreshWholePassPrediction(
                            *corpus->CollisionTree())) {
                    empty = TrySkipWholeTreeBoundsEmpty(
                            *corpus->LocationIso(),
                            *corpus->CollisionTree(),
                            *groupTransforms);
                    groupTransforms->ObserveWholePassResult(
                            *corpus->CollisionTree(), empty);
                }
                if (empty) {
                    continue;
                }
                if (!DetectEllipsoidPacketAgainstStaticGroup(
                            *this,
                            *corpus->LocationIso(),
                            *corpus->CollisionTree(),
                            transforms.MovingEllipsoidPacketPlanFor(
                                    *corpus->CollisionTree()),
                            *groupTransforms,
                            against->targetGroup->staticTrees)) {
                    u32 nextTemporalSlotOrdinal = 0u;
                    DetectCollisionBetweenTreeAndStaticCollisionTreeOptimizedCpuNativeBinary32Cached(
                            *corpus->LocationIso(),
                            *corpus->CollisionTree(),
                            nextTemporalSlotOrdinal,
                            *groupTransforms);
                }
            }
        }
    }

    MergeQueuedSphereContacts(collisionBuffer);
}

void CHmsCollisionManager::SZone::
DetectCollisionBetweenTreeAndStaticCollisionTreeOptimizedCpuNativeBinary32Cached(
        const GmIso4 &movingIsoRef,
        const CPlugTree &movingTree,
        u32 &nextTemporalSlotOrdinal,
        const OptimizedCpuStaticSurfaceTransformGroup &transforms) {
    CHmsCollisionManagerSZone *zone = this;
    const GmIso4 *movingIso = &movingIsoRef;
    const u32 temporalSlotOrdinal = nextTemporalSlotOrdinal++;
    if (!movingTree.HasWorldBox()) {
        return;
    }

    GmIso4 localIso;
    movingTree.ComposeCollisionIso(*movingIso, localIso);

    const u32 childCount = movingTree.GetChildCount();
    for (u32 childIndex = 0u; childIndex < childCount; ++childIndex) {
        zone->DetectCollisionBetweenTreeAndStaticCollisionTreeOptimizedCpuNativeBinary32Cached(
                localIso,
                *movingTree.GetChild(childIndex),
                nextTemporalSlotOrdinal,
                transforms);
    }

    CPlugSurface *movingSurface = movingTree.Surface();
    if (movingSurface == nullptr) {
        return;
    }
    CPlugTree *movingTreeNode = const_cast<CPlugTree *>(&movingTree);

    GmBoxAligned movingBox;
    movingTree.GetTransformedCollisionBox(*movingIso, movingBox);

    GmOctree<CHmsCollisionManagerSColOctreeCell> &staticTrees =
            zone->activeStaticTargetGroup->staticTrees;

    OptimizedCpuStaticSurfaceTransformGroup::TemporalCandidateSpan
            temporalCandidates;
    if (transforms.TemporalCandidateSpanFor(
                movingTree,
                temporalSlotOrdinal,
                movingBox,
                &temporalCandidates)) {
        if (temporalCandidates.size != 0u) {
            DetectNativeBinary32CachedTemporalSpan(
                    *zone,
                    movingBox,
                    *movingTreeNode,
                    *movingSurface,
                    localIso,
                    transforms,
                    temporalCandidates);
        }
        return;
    }

    DetectNativeBinary32CachedColdFallback(
            *zone,
            movingBox,
            *movingTreeNode,
            *movingSurface,
            localIso,
            transforms,
            staticTrees);
}
