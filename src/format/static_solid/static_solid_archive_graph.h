#ifndef TMNF_STATIC_SOLID_ARCHIVE_GRAPH_H
#define TMNF_STATIC_SOLID_ARCHIVE_GRAPH_H

#include "engine/core/engine_types.h"
#include <array>
#include <new>
#include <stdint.h>
#include <vector>

#include "format/static_solid/static_solid_archive_identity.h"
#include "format/static_solid/static_solid_archive_definitions.h"
#include "format/archive/archive_node_reference.h"
struct StaticSolidArchivePayload;
class StaticSolidArchiveLoadSession;
struct CGameCtnReplayStaticSolidDescriptorDependencyQueue;
class CGameCtnReplayStaticSolidArchiveTreeGraph;
class CGameCtnReplayStaticSolidArchiveSurfaceGraph;

static constexpr u32 StaticSolidArchiveDescriptorPathCapacity = 192u;
static constexpr u32 StaticSolidArchiveTreePathCapacity = 8u;

class CGameCtnReplayStaticSolidArchiveTreePath {
public:
    void Clear();
    int Assign(const u32 *path, u32 length);
    u32 Length() const;
    int CopyTo(u32 *path, u32 capacity, u32 *lengthOut) const;

private:
    std::array<u32, StaticSolidArchiveTreePathCapacity> childSlots{};
    u32 childSlotCount = 0u;
};

class CGameCtnReplayStaticSolidArchiveNode {
public:
    void Install(CGameCtnReplayStaticSolidArchiveNodeIdentity identity,
                 u32 classId);
    void SetTreeId(const CMwId &id);
    int Matches(CGameCtnReplayStaticSolidArchiveNodeIdentity identity) const;
    CGameCtnReplayStaticSolidArchiveNodeIdentity Identity() const;
    u32 ClassId() const;
    int IsSolid() const;
    int IsTree() const;
    int IsSurface() const;
    int IsSurfaceGeom() const;
    int IsMaterial() const;
    int IsVisualIndexedTriangles() const;
    int HasTreeId() const;
    CMwId TreeId() const;

private:
    CGameCtnReplayStaticSolidArchiveNodeIdentity identity =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    u32 classId = 0u;
    u32 hasTreeId = 0u;
    CMwId treeId = {};
};

class CGameCtnReplayStaticSolidArchiveTreeIdName {
public:
    int Install(CGameCtnReplayStaticSolidArchiveNodeIdentity tree,
                const char *name);
    int Matches(CGameCtnReplayStaticSolidArchiveNodeIdentity tree) const;
    int HasName() const;
    const char *Name() const;

private:
    CGameCtnReplayStaticSolidArchiveNodeIdentity tree =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    char name[64]{};
};

class CGameCtnReplayStaticSolidArchiveNamedTree {
public:
    void Install(
            CGameCtnReplayStaticSolidArchiveNodeIdentity tree,
            const char *name,
            const CGameCtnReplayStaticSolidArchiveTreeStateDefinition *state,
            const CGameCtnReplayStaticSolidArchiveSurfaceGeometryDefinition *geom);
    CGameCtnReplayStaticSolidArchiveNodeIdentity Tree() const;
    const char *Name() const;
    const CGameCtnReplayStaticSolidArchiveTreeStateDefinition *State() const;
    const CGameCtnReplayStaticSolidArchiveSurfaceGeometryDefinition *SurfaceGeom() const;
    int HasSurfaceGeometry() const;

private:
    CGameCtnReplayStaticSolidArchiveNodeIdentity tree =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    const char *name = nullptr;
    const CGameCtnReplayStaticSolidArchiveTreeStateDefinition *state = nullptr;
    const CGameCtnReplayStaticSolidArchiveSurfaceGeometryDefinition *geom = nullptr;
};

class CGameCtnReplayStaticSolidArchiveTreeChildLink {
public:
    void Install(CGameCtnReplayStaticSolidArchiveNodeIdentity parentTree,
                 CGameCtnReplayStaticSolidArchiveNodeIdentity childTree,
                 u32 linkKind);
    CGameCtnReplayStaticSolidArchiveNodeIdentity ParentTree() const;
    CGameCtnReplayStaticSolidArchiveNodeIdentity ChildTree() const;
    u32 LinkKind() const;

private:
    CGameCtnReplayStaticSolidArchiveNodeIdentity parentTree =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    CGameCtnReplayStaticSolidArchiveNodeIdentity childTree =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    u32 linkKind = 0u;
};

class CGameCtnReplayStaticSolidArchiveSolidTreeLink {
public:
    void Install(CGameCtnReplayStaticSolidArchiveNodeIdentity solid,
                 CGameCtnReplayStaticSolidArchiveNodeIdentity tree,
                 u32 chunkId);
    CGameCtnReplayStaticSolidArchiveNodeIdentity Solid() const;
    CGameCtnReplayStaticSolidArchiveNodeIdentity Tree() const;
    u32 ChunkId() const;
    int MatchesPayload(StaticSolidArchiveId payload) const;

private:
    CGameCtnReplayStaticSolidArchiveNodeIdentity solid =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    CGameCtnReplayStaticSolidArchiveNodeIdentity tree =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    u32 chunkId = 0u;
};

class CGameCtnReplayStaticSolidArchiveTreeSurfaceLink {
public:
    void Install(CGameCtnReplayStaticSolidArchiveNodeIdentity tree,
                 CGameCtnReplayStaticSolidArchiveNodeIdentity surface);
    CGameCtnReplayStaticSolidArchiveNodeIdentity Tree() const;
    CGameCtnReplayStaticSolidArchiveNodeIdentity Surface() const;

private:
    CGameCtnReplayStaticSolidArchiveNodeIdentity tree =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    CGameCtnReplayStaticSolidArchiveNodeIdentity surface =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
};

class CGameCtnReplayStaticSolidArchiveTreeSourceLink {
public:
    void Install(CGameCtnReplayStaticSolidArchiveNodeIdentity tree,
                 CGameCtnReplayStaticSolidArchiveNodeIdentity visual,
                 CGameCtnReplayStaticSolidArchiveNodeIdentity shader,
                 CGameCtnReplayStaticSolidArchiveNodeIdentity material,
                 CGameCtnReplayStaticSolidArchiveNodeIdentity surfaceSource,
                 CGameCtnReplayStaticSolidArchiveNodeIdentity generator,
                 u32 chunkId);
    CGameCtnReplayStaticSolidArchiveNodeIdentity Tree() const;
    CGameCtnReplayStaticSolidArchiveNodeIdentity Visual() const;
    CGameCtnReplayStaticSolidArchiveNodeIdentity Shader() const;
    CGameCtnReplayStaticSolidArchiveNodeIdentity Material() const;
    CGameCtnReplayStaticSolidArchiveNodeIdentity SurfaceSource() const;
    CGameCtnReplayStaticSolidArchiveNodeIdentity Generator() const;
    int HasVisual() const;
    int HasMaterialNode() const;
    int HasShaderNode() const;
    int HasMaterialOrShader() const;
    int HasSurfaceSource() const;
    int HasGenerator() const;
    u32 ChunkId() const;

private:
    CGameCtnReplayStaticSolidArchiveNodeIdentity tree =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    CGameCtnReplayStaticSolidArchiveNodeIdentity visual =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    CGameCtnReplayStaticSolidArchiveNodeIdentity shader =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    CGameCtnReplayStaticSolidArchiveNodeIdentity material =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    CGameCtnReplayStaticSolidArchiveNodeIdentity surfaceSource =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    CGameCtnReplayStaticSolidArchiveNodeIdentity generator =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    u32 chunkId = 0u;
};

class CGameCtnReplayStaticSolidArchiveSurfaceGeomLink {
public:
    void Install(CGameCtnReplayStaticSolidArchiveNodeIdentity surface,
                 CGameCtnReplayStaticSolidArchiveNodeIdentity geom,
                 u32 materialEntryCount,
                 u32 explicitMaterialRefCount,
                 u32 defaultMaterialIdCount);
    CGameCtnReplayStaticSolidArchiveNodeIdentity Surface() const;
    CGameCtnReplayStaticSolidArchiveNodeIdentity Geom() const;
    u32 MaterialEntryCount() const;
    u32 ExplicitMaterialRefCount() const;
    u32 DefaultMaterialIdCount() const;

private:
    CGameCtnReplayStaticSolidArchiveNodeIdentity surface =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    CGameCtnReplayStaticSolidArchiveNodeIdentity geom =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    u32 materialEntryCount = 0u;
    u32 explicitMaterialRefCount = 0u;
    u32 defaultMaterialIdCount = 0u;
};

class CGameCtnReplayStaticSolidArchiveSurfaceMaterialEntry {
public:
    void InstallExplicitRef(CGameCtnReplayStaticSolidArchiveNodeIdentity surface,
                            u32 materialSlotIndex,
                            CGameCtnReplayStaticSolidArchiveNodeIdentity
                                    material);
    void InstallDefaultMaterial(
                                CGameCtnReplayStaticSolidArchiveNodeIdentity
                                        surface,
                                u32 materialSlotIndex,
                                uint16_t defaultMaterialId);
    CGameCtnReplayStaticSolidArchiveNodeIdentity Surface() const;
    CGameCtnReplayStaticSolidArchiveNodeIdentity Material() const;
    int UsesExplicitRef() const;
    u32 MaterialSlotIndex() const;
    uint16_t DefaultMaterialId() const;

private:
    CGameCtnReplayStaticSolidArchiveNodeIdentity surface =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    u32 materialSlotIndex = 0u;
    bool usesExplicitRef = false;
    CGameCtnReplayStaticSolidArchiveNodeIdentity material =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    uint16_t defaultMaterialId = 0u;
};

class CGameCtnReplayStaticSolidDecoratorTreeDeclaration {
public:
    void Reset(CGameCtnReplayStaticSolidArchiveNodeIdentity decorator,
               u32 chunkId);
    CGameCtnReplayStaticSolidArchiveNodeIdentity Decorator() const;
    void SetTreeId(CMwId id);
    void SetMaterialNode(ArchiveNodeReference nodeRef);
    void SetVisualNode(ArchiveNodeReference nodeRef);
    void SetSolidSurfaceFromVisual(u32 enabled);
    void SetVisibilityConditions(u32 show,
                                 u32 visible,
                                 u32 applyVisibleChildren,
                                 u32 caster,
                                 u32 applyCasterChildren,
                                 u32 solidFromVisual);
    void SetNearlyEqualIdentityCondition(u32 enabled);
    void SetCollisionCondition(u32 condition);
    int SetExternalDescriptorPaths(const char *plainPath,
                                   const char *selectedPath);

    CMwId TreeId() const;
    CGameCtnReplayStaticSolidArchiveNodeIdentity Material() const;
    CGameCtnReplayStaticSolidArchiveNodeIdentity Visual() const;
    u32 ChunkId() const;
    int HasSelectedDescriptorPath() const;
    const char *SelectedDescriptorPath() const;
    const char *PlainDescriptorPath() const;
    int UsesVisualSurfaceAtQuality(u32 quality) const;
    int IsShownAtQuality(u32 quality) const;
    bool IsVisibleAtQuality(u32 quality) const;
    bool CastsShadowsAtQuality(u32 quality) const;
    bool HasCollisionAtQuality(u32 quality) const;
    int AppliesVisibleToChildren() const;
    int AppliesCasterToChildren() const;

private:
    static int IsConditionEnabled(u32 condition, u32 quality);

    CGameCtnReplayStaticSolidArchiveNodeIdentity decorator =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    CMwId treeId = {};
    CGameCtnReplayStaticSolidArchiveNodeIdentity material =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    CGameCtnReplayStaticSolidArchiveNodeIdentity visual =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    u32 showCondition = 6u;
    u32 visibleCondition = 6u;
    u32 applyVisibleToChildren = 0u;
    u32 casterCondition = 6u;
    u32 applyCasterToChildren = 0u;
    u32 solidSurfaceFromVisual = 0u;
    u32 collisionCondition = 0u;
    u32 nearlyEqualIdentityCondition = 0u;
    u32 chunkId = 0u;
    char sourcePlainPath[StaticSolidArchiveDescriptorPathCapacity]{};
    char sourceSelectedDescriptorPath
            [StaticSolidArchiveDescriptorPathCapacity]{};
};

class CGameCtnReplayStaticSolidDecoratorTreeCursor {
public:
    static CGameCtnReplayStaticSolidDecoratorTreeCursor FromDeclarationCount(
            u32 recordCount) {
        return CGameCtnReplayStaticSolidDecoratorTreeCursor(recordCount);
    }

    u32 DeclarationCountForGraphLookup() const {
        return recordCount;
    }

private:
    explicit CGameCtnReplayStaticSolidDecoratorTreeCursor(u32 recordCount)
            : recordCount(recordCount) {}

    u32 recordCount = 0u;
};

class CGameCtnReplayStaticSolidDecoratorTrees {
public:
    void Clear();
    u32 Count() const;
    int Empty() const;
    CGameCtnReplayStaticSolidDecoratorTreeCursor End() const;
    const CGameCtnReplayStaticSolidDecoratorTreeDeclaration *At(
            u32 index) const;
    int AddParsedTree(
            const CGameCtnReplayStaticSolidDecoratorTreeDeclaration
                    &declaration);
    int ResizePrefix(CGameCtnReplayStaticSolidDecoratorTreeCursor cursor);
    int RequestExternalDescriptorDependencies(
            const StaticSolidArchiveLoadSession *archive,
            CGameCtnReplayStaticSolidDescriptorDependencyQueue *dependencyQueue,
            CGameCtnReplayStaticSolidDecoratorTreeCursor first) const;

    template <typename Visitor>
    int ForEachDeclaration(Visitor visitor) const {
        for (const CGameCtnReplayStaticSolidDecoratorTreeDeclaration
                     &declaration : declarations) {
            if (!visitor(declaration)) {
                return 0;
            }
        }
        return 1;
    }

private:
    std::vector<CGameCtnReplayStaticSolidDecoratorTreeDeclaration>
            declarations;
};

enum class CGameCtnReplayStaticSolidArchiveGraphTail : u32 {
    Nodes = 0,
    TreeChildLinks,
    TreeStateDefinitions,
    SolidTreeLinks,
    TreeSurfaceLinks,
    TreeSourceLinks,
    SurfaceGeomLinks,
    SurfaceMaterialEntries,
    MaterialDefinitions,
    SurfaceGeometryDefinitions,
    VisualGeometryDefinitions,
    DecoratorTrees,
    TreeIdNames,
    SolidPhysicalDefinitions,
    Count
};

class CGameCtnReplayStaticSolidArchiveGraphRollbackMark {
private:
    static constexpr size_t TailCount =
            static_cast<size_t>(
                    CGameCtnReplayStaticSolidArchiveGraphTail::Count);
    std::array<u32, TailCount> tailCounts{};

    friend class CGameCtnReplayStaticSolidArchiveGraph;
};

template <typename Record>
class CGameCtnReplayStaticSolidArchiveSequence {
public:
    void Clear() {
        records.clear();
    }

    u32 Count() const {
        return records.size() <= UINT32_MAX ? (u32)records.size() : UINT32_MAX;
    }

    Record *MutableAt(u32 index) {
        return index < records.size() ? &records[index] : nullptr;
    }

    const Record *At(u32 index) const {
        return index < records.size() ? &records[index] : nullptr;
    }

    int Empty() const {
        return records.empty();
    }

    int ResizePrefix(u32 count) {
        if (count > records.size()) {
            return 0;
        }
        records.resize(count);
        return 1;
    }

    int Append(const Record &record) {
        if (records.size() >= UINT32_MAX) {
            return 0;
        }
        try {
            records.push_back(record);
            return 1;
        } catch (const std::bad_alloc &) {
            return 0;
        }
    }

    typename std::vector<Record>::const_iterator begin() const {
        return records.begin();
    }

    typename std::vector<Record>::const_iterator end() const {
        return records.end();
    }

private:
    std::vector<Record> records;
};

class CGameCtnReplayStaticSolidArchiveNodeDirectory {
public:
    void Clear();

    u32 Count() const;
    u32 TreeIdNameCount() const;
    int Empty() const;
    const CGameCtnReplayStaticSolidArchiveNode *At(u32 index) const;
    int Add(const CGameCtnReplayStaticSolidArchiveNode &node);
    int ResizePrefix(u32 count);
    int TruncateTreeIdNames(u32 count);

    template <typename Visitor>
    int ForEachNode(Visitor visitor) const {
        for (u32 i = 0u; i < nodes.Count(); i++) {
            const CGameCtnReplayStaticSolidArchiveNode *node = nodes.At(i);
            if (node != nullptr && !visitor(i, *node)) {
                return 0;
            }
        }
        return 1;
    }
    template <typename Visitor>
    int ForEachTree(Visitor visitor) const {
        return ForEachNode([&](u32 record,
                               const CGameCtnReplayStaticSolidArchiveNode
                                       &node) {
            return !node.IsTree() || visitor(record, node);
        });
    }
    template <typename Visitor>
    int ForEachSurface(Visitor visitor) const {
        return ForEachNode([&](u32 record,
                               const CGameCtnReplayStaticSolidArchiveNode
                                       &node) {
            return !node.IsSurface() || visitor(record, node);
        });
    }
    template <typename Visitor>
    int ForEachMaterial(Visitor visitor) const {
        return ForEachNode([&](u32 record,
                               const CGameCtnReplayStaticSolidArchiveNode
                                       &node) {
            return !node.IsMaterial() || visitor(record, node);
        });
    }
    template <typename Visitor>
    int ForEachNamedTree(
            StaticSolidArchiveId payload,
            Visitor visitor,
            const CGameCtnReplayStaticSolidArchiveTreeGraph &treeGraph,
            const CGameCtnReplayStaticSolidArchiveSurfaceGraph &surfaceGraph)
            const {
        for (const CGameCtnReplayStaticSolidArchiveNode &node : nodes) {
            if (!node.Identity().MatchesPayload(payload)) {
                continue;
            }
            const char *treeIdName = FindTreeIdName(node.Identity());
            if (treeIdName == nullptr) {
                continue;
            }
            CGameCtnReplayStaticSolidArchiveNamedTree namedTree;
            namedTree.Install(node.Identity(),
                              treeIdName,
                              FindTreeStateWithLocalIso(treeGraph,
                                                        node.Identity()),
                              FindTreeSurfaceGeometryDefinition(treeGraph,
                                                       surfaceGraph,
                                                       node.Identity()));
            if (!visitor(namedTree)) {
                return 0;
            }
        }
        return 1;
    }

    int FindClassId(CGameCtnReplayStaticSolidArchiveNodeIdentity identity,
                    u32 *classIdOut) const;
    void SetTreeId(CGameCtnReplayStaticSolidArchiveNodeIdentity tree,
                   const CMwId &treeId,
                   const char *treeIdName);

private:
    u32 FindRecord(CGameCtnReplayStaticSolidArchiveNodeIdentity identity) const;
    const CGameCtnReplayStaticSolidArchiveNode *Find(
            CGameCtnReplayStaticSolidArchiveNodeIdentity identity) const;
    int AddTreeIdName(const CGameCtnReplayStaticSolidArchiveTreeIdName &name) {
        return treeIdNames.Append(name);
    }
    const char *FindTreeIdName(
            CGameCtnReplayStaticSolidArchiveNodeIdentity tree) const;
    const CGameCtnReplayStaticSolidArchiveTreeStateDefinition *
    FindTreeStateWithLocalIso(
            const CGameCtnReplayStaticSolidArchiveTreeGraph &treeGraph,
            CGameCtnReplayStaticSolidArchiveNodeIdentity tree) const;
    const CGameCtnReplayStaticSolidArchiveSurfaceGeometryDefinition *
    FindTreeSurfaceGeometryDefinition(
            const CGameCtnReplayStaticSolidArchiveTreeGraph &treeGraph,
            const CGameCtnReplayStaticSolidArchiveSurfaceGraph &surfaceGraph,
            CGameCtnReplayStaticSolidArchiveNodeIdentity tree) const;

    CGameCtnReplayStaticSolidArchiveSequence<CGameCtnReplayStaticSolidArchiveNode>
            nodes;
    CGameCtnReplayStaticSolidArchiveSequence<
            CGameCtnReplayStaticSolidArchiveTreeIdName>
            treeIdNames;
};

class CGameCtnReplayStaticSolidArchiveTreeGraph {
public:
    void Clear();

    u32 ChildLinkCount() const;
    u32 TreeStateCount() const;
    u32 SolidRootLinkCount() const;
    u32 SolidPhysicalDefinitionCount() const;
    u32 TreeSurfaceLinkCount() const;
    u32 TreeSourceLinkCount() const;
    int HasSolidRootLinks() const;
    int HasTreeSourceLinks() const;

    template <typename Visitor>
    int ForEachChildLink(Visitor visitor) const {
        for (const CGameCtnReplayStaticSolidArchiveTreeChildLink &link :
             childLinks) {
            if (!visitor(link)) {
                return 0;
            }
        }
        return 1;
    }
    template <typename Visitor>
    int ForEachTreeState(Visitor visitor) const {
        for (const CGameCtnReplayStaticSolidArchiveTreeStateDefinition
                     &definition : treeStates) {
            if (!visitor(definition)) {
                return 0;
            }
        }
        return 1;
    }
    template <typename Visitor>
    int ForEachSolidRootLink(Visitor visitor) const {
        for (const CGameCtnReplayStaticSolidArchiveSolidTreeLink &link :
             solidRootLinks) {
            if (!visitor(link)) {
                return 0;
            }
        }
        return 1;
    }
    template <typename Visitor>
    int ForEachSolidPhysicalDefinition(Visitor visitor) const {
        for (const CGameCtnReplayStaticSolidArchiveSolidPhysicsDefinition
                     &definition : solidPhysicalDefinitions) {
            if (!visitor(definition)) {
                return 0;
            }
        }
        return 1;
    }
    template <typename Visitor>
    int ForEachTreeSurfaceLink(Visitor visitor) const {
        for (const CGameCtnReplayStaticSolidArchiveTreeSurfaceLink &link :
             treeSurfaceLinks) {
            if (!visitor(link)) {
                return 0;
            }
        }
        return 1;
    }
    template <typename Visitor>
    int ForEachTreeSourceLink(Visitor visitor) const {
        for (const CGameCtnReplayStaticSolidArchiveTreeSourceLink &link :
             treeSourceLinks) {
            if (!visitor(link)) {
                return 0;
            }
        }
        return 1;
    }

    int AddChildLink(
            const CGameCtnReplayStaticSolidArchiveTreeChildLink &link);
    int AddTreeState(
            const CGameCtnReplayStaticSolidArchiveTreeStateDefinition &definition);
    int AddSolidRootLink(
            const CGameCtnReplayStaticSolidArchiveSolidTreeLink &link);
    int AddSolidPhysicalDefinition(
            const CGameCtnReplayStaticSolidArchiveSolidPhysicsDefinition
                    &definition);
    int AddTreeSurfaceLink(
            const CGameCtnReplayStaticSolidArchiveTreeSurfaceLink &link);
    int AddTreeSourceLink(
            const CGameCtnReplayStaticSolidArchiveTreeSourceLink &link);

    int TruncateChildLinks(u32 count);
    int TruncateTreeStates(u32 count);
    int TruncateSolidRootLinks(u32 count);
    int TruncateSolidPhysicalDefinitions(u32 count);
    int TruncateTreeSurfaceLinks(u32 count);
    int TruncateTreeSourceLinks(u32 count);

private:
    CGameCtnReplayStaticSolidArchiveSequence<
            CGameCtnReplayStaticSolidArchiveTreeChildLink>
            childLinks;
    CGameCtnReplayStaticSolidArchiveSequence<
            CGameCtnReplayStaticSolidArchiveTreeStateDefinition>
            treeStates;
    CGameCtnReplayStaticSolidArchiveSequence<
            CGameCtnReplayStaticSolidArchiveSolidTreeLink>
            solidRootLinks;
    CGameCtnReplayStaticSolidArchiveSequence<
            CGameCtnReplayStaticSolidArchiveSolidPhysicsDefinition>
            solidPhysicalDefinitions;
    CGameCtnReplayStaticSolidArchiveSequence<
            CGameCtnReplayStaticSolidArchiveTreeSurfaceLink>
            treeSurfaceLinks;
    CGameCtnReplayStaticSolidArchiveSequence<
            CGameCtnReplayStaticSolidArchiveTreeSourceLink>
            treeSourceLinks;
};

class CGameCtnReplayStaticSolidArchiveSurfaceGraph {
public:
    void Clear();

    u32 SurfaceGeometryLinkCount() const;
    u32 SurfaceMaterialEntryCount() const;
    u32 MaterialDefinitionCount() const;
    u32 SurfaceGeometryDefinitionCount() const;
    u32 VisualProviderDefinitionCount() const;
    int HasMaterialDefinitions() const;

    template <typename Visitor>
    int ForEachSurfaceGeometryLink(Visitor visitor) const {
        for (const CGameCtnReplayStaticSolidArchiveSurfaceGeomLink &link :
             surfaceGeometryLinks) {
            if (!visitor(link)) {
                return 0;
            }
        }
        return 1;
    }
    template <typename Visitor>
    int ForEachSurfaceMaterialEntry(Visitor visitor) const {
        for (const CGameCtnReplayStaticSolidArchiveSurfaceMaterialEntry &entry :
             surfaceMaterialEntries) {
            if (!visitor(entry)) {
                return 0;
            }
        }
        return 1;
    }
    template <typename Visitor>
    int ForEachMaterialDefinition(Visitor visitor) const {
        for (const CGameCtnReplayStaticSolidArchiveMaterialDefinition &definition :
             materialDefinitions) {
            if (!visitor(definition)) {
                return 0;
            }
        }
        return 1;
    }
    template <typename Visitor>
    int ForEachSurfaceGeometryDefinition(Visitor visitor) const {
        for (const CGameCtnReplayStaticSolidArchiveSurfaceGeometryDefinition
                     &definition : surfaceGeometryDefinitions) {
            if (!visitor(definition)) {
                return 0;
            }
        }
        return 1;
    }
    template <typename Visitor>
    int ForEachVisualProviderDefinition(Visitor visitor) const {
        for (const CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition
                     &definition : visualProviderDefinitions) {
            if (!visitor(definition)) {
                return 0;
            }
        }
        return 1;
    }

    int AddSurfaceGeometryLink(
            const CGameCtnReplayStaticSolidArchiveSurfaceGeomLink &link);
    int AddSurfaceMaterialEntry(
            const CGameCtnReplayStaticSolidArchiveSurfaceMaterialEntry &entry);
    int AddMaterialDefinition(
            const CGameCtnReplayStaticSolidArchiveMaterialDefinition &definition);
    int AddSurfaceGeometryDefinition(
            const CGameCtnReplayStaticSolidArchiveSurfaceGeometryDefinition
                    &definition);
    int AddVisualProviderDefinition(
            const CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition
                    &definition);
    int UpdateVisualProviderDefinition(
            const CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition
                    &definition);

    int TruncateSurfaceGeometryLinks(u32 count);
    int TruncateSurfaceMaterialEntries(u32 count);
    int TruncateMaterialDefinitions(u32 count);
    int TruncateSurfaceGeometryDefinitions(u32 count);
    int TruncateVisualProviderDefinitions(u32 count);

private:
    CGameCtnReplayStaticSolidArchiveSequence<
            CGameCtnReplayStaticSolidArchiveSurfaceGeomLink>
            surfaceGeometryLinks;
    CGameCtnReplayStaticSolidArchiveSequence<
            CGameCtnReplayStaticSolidArchiveSurfaceMaterialEntry>
            surfaceMaterialEntries;
    CGameCtnReplayStaticSolidArchiveSequence<
            CGameCtnReplayStaticSolidArchiveMaterialDefinition>
            materialDefinitions;
    CGameCtnReplayStaticSolidArchiveSequence<
            CGameCtnReplayStaticSolidArchiveSurfaceGeometryDefinition>
            surfaceGeometryDefinitions;
    CGameCtnReplayStaticSolidArchiveSequence<
            CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition>
            visualProviderDefinitions;
};

class CGameCtnReplayStaticSolidArchiveGraph {
public:
    void Free();

    CGameCtnReplayStaticSolidArchiveNodeDirectory &Nodes();
    const CGameCtnReplayStaticSolidArchiveNodeDirectory &Nodes() const;
    CGameCtnReplayStaticSolidArchiveTreeGraph &TreeGraph();
    const CGameCtnReplayStaticSolidArchiveTreeGraph &TreeGraph() const;
    CGameCtnReplayStaticSolidArchiveSurfaceGraph &SurfaceGraph();
    const CGameCtnReplayStaticSolidArchiveSurfaceGraph &SurfaceGraph() const;
    const CGameCtnReplayStaticSolidDecoratorTrees &DecoratorTrees() const;
    int HasDecoratorTrees() const;
    int HasArchiveNodes() const;
    int FindNodeClassId(CGameCtnReplayStaticSolidArchiveNodeIdentity identity,
                        u32 *classIdOut) const;
    CGameCtnReplayStaticSolidDecoratorTreeCursor DecoratorTreeEnd() const;
    int QueueDecoratorTreeDependencies(
            const StaticSolidArchiveLoadSession *archive,
            CGameCtnReplayStaticSolidDescriptorDependencyQueue *dependencyQueue,
            CGameCtnReplayStaticSolidDecoratorTreeCursor first) const;
    int AddDecoratorTree(
            const CGameCtnReplayStaticSolidDecoratorTreeDeclaration
                    &declaration);
    int FindSolidRootTree(
            StaticSolidArchiveId payload,
            CGameCtnReplayStaticSolidArchiveNodeIdentity *treeOut) const;
    CGameCtnReplayStaticSolidArchiveNodeIdentity FindTreeForSolid(
            CGameCtnReplayStaticSolidArchiveNodeIdentity solid) const;
    int BuildTreePath(CGameCtnReplayStaticSolidArchiveNodeIdentity rootTree,
                      CGameCtnReplayStaticSolidArchiveNodeIdentity targetTree,
                      CGameCtnReplayStaticSolidArchiveTreePath *pathOut) const;
    template <typename Visitor>
    int ForEachNamedTree(StaticSolidArchiveId payload,
                         Visitor visitor) const {
        return nodeDirectory.ForEachNamedTree(payload,
                                              visitor,
                                              treeGraph,
                                              surfaceGraph);
    }
    CGameCtnReplayStaticSolidArchiveGraphRollbackMark MarkRollback() const;
    int CanRestoreRollback(
            const CGameCtnReplayStaticSolidArchiveGraphRollbackMark &mark) const;
    int RestoreRollback(
            const CGameCtnReplayStaticSolidArchiveGraphRollbackMark &mark);

private:
    u32 TailCount(CGameCtnReplayStaticSolidArchiveGraphTail tail) const;
    int RestoreTail(CGameCtnReplayStaticSolidArchiveGraphTail tail,
                    u32 recordCount);
    int BuildTreePathRec(CGameCtnReplayStaticSolidArchiveNodeIdentity current,
                         CGameCtnReplayStaticSolidArchiveNodeIdentity target,
                         u32 depth,
                         u32 *path,
                         u32 pathCapacity,
                         u32 *pathLengthOut) const;

    CGameCtnReplayStaticSolidArchiveNodeDirectory nodeDirectory;
    CGameCtnReplayStaticSolidArchiveTreeGraph treeGraph;
    CGameCtnReplayStaticSolidArchiveSurfaceGraph surfaceGraph;
    CGameCtnReplayStaticSolidDecoratorTrees decoratorTrees;
};

#endif
