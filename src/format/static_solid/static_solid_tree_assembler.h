#pragma once

#include "engine/rendering/plug_visual.h"
#include "engine/rendering/plug_material.h"
#include "engine/physics/geometry/plug_surface.h"
#include "engine/rendering/plug_tree.h"
#include "engine/core/engine_types.h"
#include "format/static_solid/static_solid_surface_assembler.h"
#include "format/static_solid/static_solid_archive_identity.h"
#include "format/static_solid/static_solid_tree_storage.h"
#include "format/static_solid/static_solid_archive_id.h"
#include <optional>
#include <vector>

class CGameCtnReplayStaticSolidArchiveNodeDirectory;
class CGameCtnReplayStaticSolidArchiveTreeGraph;
class CGameCtnReplayStaticSolidArchiveSolidPhysicsDefinition;
class CGameCtnReplayStaticSolidArchiveSolidTreeLink;
class CGameCtnReplayStaticSolidArchiveTreeSourceLink;
class CGameCtnReplayStaticSolidArchiveTreeSurfaceLink;

class StaticSolidTreeAssembler {
public:
    void Free();
    int BuildTrees(const CGameCtnReplayStaticSolidArchiveNodeDirectory &nodes,
                   const CGameCtnReplayStaticSolidArchiveTreeGraph &treeGraph);

    CPlugTree *TreeAt(u32 index);
    CPlugTree *TreeForNode(
            CGameCtnReplayStaticSolidArchiveNodeIdentity tree);
    CPlugTree *CollisionRootForPayload(
            StaticSolidArchiveId payload);
    CPlugTree *ModelRoot(StaticSolidArchiveId payload,
                         std::optional<u32> treeNodeIndex);
    const CGameCtnReplayStaticSolidArchiveSolidPhysicsDefinition
            *PhysicsDefinitionForPayload(
                    StaticSolidArchiveId payload);
    int HasNodeMap() const;
    int HasSolidRoots() const;
    u32 TreeCount() const;

    void ApplyTreeSurfaceLinks(
            const CGameCtnReplayStaticSolidArchiveTreeGraph &treeGraph,
            StaticSolidSurfaceAssembler &surfaces);
    void ApplyTreeSourceLinks(
            const CGameCtnReplayStaticSolidArchiveNodeDirectory &nodes,
            const CGameCtnReplayStaticSolidArchiveTreeGraph &treeGraph,
            StaticSolidSurfaceAssembler &surfaces);
    void ApplySolidTreeLinks(
            const CGameCtnReplayStaticSolidArchiveTreeGraph &treeGraph);

private:
    class SolidRootDirectory {
    public:
        void Clear();
        void Install(StaticSolidArchiveId payload,
                     CPlugTree *root,
                     const CGameCtnReplayStaticSolidArchiveSolidPhysicsDefinition
                             *physical);
        CPlugTree *RootForPayload(
                StaticSolidArchiveId payload) const;
        const CGameCtnReplayStaticSolidArchiveSolidPhysicsDefinition
                *PhysicalForPayload(
                        StaticSolidArchiveId payload)
                        const;
        int HasRoots() const;

    private:
        struct Root {
            StaticSolidArchiveId payload =
                    StaticSolidArchiveId::Invalid();
            CPlugTree *tree = nullptr;
            const CGameCtnReplayStaticSolidArchiveSolidPhysicsDefinition
                    *physical = nullptr;
        };

        std::vector<Root> roots;
    };

    StaticSolidTreeStorage trees;
    SolidRootDirectory solidRoots;
    u32 treeCount = 0u;

    u32 TreeIndexForNode(
            CGameCtnReplayStaticSolidArchiveNodeIdentity tree) const {
        return trees.TreeIndexForNode(tree);
    }
    void LinkTreeToSurface(
            CGameCtnReplayStaticSolidArchiveNodeIdentity tree,
            CGameCtnReplayStaticSolidArchiveNodeIdentity surface,
            StaticSolidSurfaceAssembler &surfaces);
    void ApplyTreeSourceLink(
            const CGameCtnReplayStaticSolidArchiveTreeSourceLink &link,
            const CGameCtnReplayStaticSolidArchiveNodeDirectory &nodes,
            StaticSolidSurfaceAssembler &surfaces);
    void ApplySolidTreeLink(
            const CGameCtnReplayStaticSolidArchiveTreeGraph &treeGraph,
            const CGameCtnReplayStaticSolidArchiveSolidTreeLink &link);
    const CGameCtnReplayStaticSolidArchiveSolidPhysicsDefinition
            *PhysicsDefinitionForSolid(
                    const CGameCtnReplayStaticSolidArchiveTreeGraph &treeGraph,
                    CGameCtnReplayStaticSolidArchiveNodeIdentity solid) const;
    void AttachSurface(CPlugTree *tree, CPlugSurface *surface);
    void AttachVisual(CPlugTree *tree,
                                CPlugVisual *provider);
    void AttachMaterial(CPlugTree *tree, CPlugMaterial *material);
};
