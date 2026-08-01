#ifndef TMNF_STATIC_SOLID_ARCHIVE_NODE_GRAPH_H
#define TMNF_STATIC_SOLID_ARCHIVE_NODE_GRAPH_H

#include "engine/core/engine_types.h"
#include <optional>
#include <string>
#include <vector>

#include "format/archive/hms_item_archive_state.h"
#include "format/static_solid/static_scene_archive_loader.h"
#include "format/archive/archive_node_reference.h"
#include "format/static_solid/static_solid_external_node_paths.h"
#include "format/static_solid/static_solid_material_definition_resolver.h"
#include "format/static_solid/static_solid_archive_id.h"
struct CGameCtnReplayStaticSolidDescriptorDependencyQueue;
class StaticSolidArchiveLoadSession;
struct CPlugFilePack;
struct SceneDescriptorFolderPaths;

class CGameCtnReplayStaticSolidArchiveNodeGraph {
public:
    class SceneMobilInternalRefKey {
    public:
        static SceneMobilInternalRefKey FromSceneObjectRef(u32 archiveWord) {
            return SceneMobilInternalRefKey(archiveWord);
        }

        int IsStoredRef() const {
            return archiveWord !=
                           ArchiveNodeReference::
                                   InvalidIndex &&
                   archiveWord !=
                           ArchiveNodeReference::
                                   DeferredIndex;
        }

        u32 StorageIndex() const {
            return archiveWord;
        }

    private:
        explicit SceneMobilInternalRefKey(u32 archiveWord)
            : archiveWord(archiveWord) {}

        u32 archiveWord = ArchiveNodeReference::
                InvalidIndex;
    };

    class Node {
    public:
        int IsPresent() const;
        int IsExternal() const;
        int IsLoadable() const;
        u32 ClassId() const;
        u32 FolderIndex() const;
        const std::string &Name() const;
        int HasExternalName() const;
        int SetName(const std::string &name);
        void SetHmsItemState(const HmsItemArchiveWords &state);
        const std::optional<HmsItemArchiveWords> &HmsItemState() const;
        void MarkInline(u32 classId);
        int MarkExternal(u32 loadable, u32 folderIndex, const std::string &name);
        void SetClassId(u32 classId);
        CGameCtnReplayStaticSolidExternalNodeRef ExternalNodeRef() const;

    private:
        u32 present = 0u;
        u32 classId = 0u;
        u32 external = 0u;
        u32 loadable = 0u;
        u32 folderIndex = 0xffffffffu;
        std::string name;
        std::optional<HmsItemArchiveWords> hmsItemState;
    };

    int EnsureNodeCapacity(u32 index);
    u32 NodeCount() const;
    int HasNode(ArchiveNodeReference nodeRef) const;
    const Node *FindNode(
            ArchiveNodeReference nodeRef) const;
    int SetNodeName(ArchiveNodeReference nodeRef,
                    const std::string &name);
    int RememberHmsItemState(ArchiveNodeReference nodeRef,
                             const HmsItemArchiveWords &state);
    const HmsItemArchiveWords *HmsItemStateForSceneMobil(
            ArchiveNodeReference nodeRef) const;

    void MarkInlineNode(ArchiveNodeReference nodeRef,
                        u32 classId);
    int MarkExternalNode(
            ArchiveNodeReference nodeRef,
            u32 loadable,
            u32 folderIndex,
            const std::string &name);
    void SetNodeClassId(
            ArchiveNodeReference nodeRef,
            u32 classId);

    CGameCtnReplayStaticSolidExternalNodeRef ExternalNodeRef(
            ArchiveNodeReference nodeRef) const;
    int CopyExternalNodeRefs(
            std::vector<CGameCtnReplayStaticSolidExternalNodeRef> *refsOut)
            const;

    int RememberSceneMobilRef(
            SceneMobilInternalRefKey ref,
            ArchiveNodeReference mobilNode);
    ArchiveNodeReference SceneMobilRefNode(
            SceneMobilInternalRefKey ref) const;
    int RememberMobilSolidLink(
            ArchiveNodeReference mobilNode,
            ArchiveNodeReference solidNode);
    ArchiveNodeReference SolidForSceneMobil(
            ArchiveNodeReference mobilNode) const;
    int RememberSolidModelFidLink(
            ArchiveNodeReference solidNode,
            ArchiveNodeReference modelFidNode);
    ArchiveNodeReference ModelFidForSolid(
            ArchiveNodeReference solidNode) const;
    ArchiveNodeReference TreeForSolid(
            const StaticSolidArchiveLoadSession *store,
            StaticSolidArchiveId payload,
            ArchiveNodeReference solidNode) const;

    int BuildMaterialRef(
            StaticSolidArchiveId payload,
            ArchiveNodeReference materialNode,
            StaticSolidMaterialReference *materialRefOut) const;

private:
    class MobilSolidLink {
    public:
        void Install(ArchiveNodeReference mobilNode,
                     ArchiveNodeReference solidNode);
        int MatchesMobil(
                ArchiveNodeReference mobilNode) const;
        void SetSolidNode(
                ArchiveNodeReference solidNode);
        ArchiveNodeReference SolidNode() const;

    private:
        ArchiveNodeReference mobilNode =
                ArchiveNodeReference::Invalid();
        ArchiveNodeReference solidNode =
                ArchiveNodeReference::Invalid();
    };

    class SolidModelFidLink {
    public:
        void Install(ArchiveNodeReference solidNode,
                     ArchiveNodeReference modelFidNode);
        int MatchesSolid(
                ArchiveNodeReference solidNode) const;
        void SetModelFidNode(
                ArchiveNodeReference modelFidNode);
        ArchiveNodeReference ModelFidNode() const;

    private:
        ArchiveNodeReference solidNode =
                ArchiveNodeReference::Invalid();
        ArchiveNodeReference modelFidNode =
                ArchiveNodeReference::Invalid();
    };

    class SceneMobilInternalRef {
    public:
        void Install(ArchiveNodeReference mobilNode);
        ArchiveNodeReference MobilNode() const;

    private:
        ArchiveNodeReference mobilNode =
                ArchiveNodeReference::Invalid();
    };

    std::vector<Node> nodes;
    std::vector<SceneMobilInternalRef> sceneMobilInternalRefs;
    std::vector<MobilSolidLink> mobilSolidLinks;
    std::vector<SolidModelFidLink> solidModelFidLinks;

    int EnsureSceneMobilRefCapacity(u32 index);
};

#endif
