#ifndef TMNF_STATIC_SOLID_ARCHIVE_SURFACE_READER_H
#define TMNF_STATIC_SOLID_ARCHIVE_SURFACE_READER_H

#include "engine/core/engine_types.h"
#include <vector>


#include "format/static_solid/static_scene_archive_loader.h"
#include "format/archive/archive_node_reference.h"
#include "format/static_solid/static_solid_archive_node_ref_reader.h"
#include "format/static_solid/static_solid_archive_id.h"
class CGameCtnReplayStaticSolidArchiveByteStream;
class CGameCtnReplayStaticSolidArchiveNodeGraph;
class StaticSolidArchiveLoadSession;
struct SceneDescriptorFolderPaths;

class CGameCtnReplayStaticSolidSurfaceDefinition {
public:
    static CGameCtnReplayStaticSolidSurfaceDefinition ForArchiveSurface(
            StaticSolidArchiveId payload,
            u32 surfaceNodeIndex);
    int ReadFromArchive(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
    int Commit(
            CGameCtnReplayStaticSolidArchiveNodeGraph *archiveNodeGraph,
            const SceneDescriptorFolderPaths *externalFolders,
            StaticSolidArchiveLoadSession *store) const;

private:
    StaticSolidArchiveId payload =
            StaticSolidArchiveId::Invalid();
    ArchiveNodeReference surfaceNode =
            ArchiveNodeReference::Invalid();
    ArchiveNodeReference geomNode =
            ArchiveNodeReference::Invalid();
    std::vector<CGameCtnReplayStaticSolidArchiveSurfaceMaterialEntry>
            materialEntries;

    CGameCtnReplayStaticSolidArchiveNodeIdentity NodeIdentity(
            ArchiveNodeReference nodeRef) const {
        return CGameCtnReplayStaticSolidArchiveNodeIdentity::FromPayloadAndArchiveIndex(
                payload,
                nodeRef.Index());
    }
    u32 ExplicitMaterialRefCount() const;
    u32 DefaultMaterialIdCount() const;
};

struct CGameCtnReplayStaticSolidArchiveSurfaceReader {
    static int ParseSurfaceDefinition(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveNodeGraph *archiveNodeGraph,
            const SceneDescriptorFolderPaths *externalFolders,
            StaticSolidArchiveLoadSession *store,
            StaticSolidArchiveId payload,
            u32 surfaceNodeIndex,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
};

#endif
