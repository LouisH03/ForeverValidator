#ifndef TMNF_STATIC_SOLID_ARCHIVE_MATERIAL_READER_H
#define TMNF_STATIC_SOLID_ARCHIVE_MATERIAL_READER_H


#include "engine/core/engine_types.h"
#include "engine/game/material_definition.h"
#include "format/archive/archive_node_reference.h"
#include "format/static_solid/static_solid_archive_node_ref_reader.h"
#include "format/static_solid/static_solid_archive_id.h"
class CGameCtnReplayStaticSolidArchiveByteStream;
struct CGameCtnReplayStaticSolidArchiveCMwIdState;
class StaticSolidArchiveLoadSession;

struct CPlugMaterialDiscardedRefArchivePayload {
    static int Read(CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
};

struct CPlugMaterialDeviceMatsArchivePayload {
    explicit CPlugMaterialDeviceMatsArchivePayload(u32 chunkId);

    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
             CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
             CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);

private:
    int ReadDeviceMats(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
                       CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
                       CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);

    u32 chunkId_;
    ArchiveNodeReference materialModelNode_;
};

struct CPlugMaterialSurfaceFlagsArchivePayload {
    explicit CPlugMaterialSurfaceFlagsArchivePayload(u32 chunkId);

    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream);
    int AppendDefinition(StaticSolidArchiveLoadSession *store,
                    StaticSolidArchiveId payload,
                    u32 materialNodeIndex) const;

private:
    u32 chunkId_;
    MaterialSurfaceDefinition surface_;
};

struct CGameCtnReplayStaticSolidArchiveMaterialReader {
    static int ParseCPlugMaterialChunk(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
            u32 classId,
            StaticSolidArchiveLoadSession *store,
            StaticSolidArchiveId payload,
            u32 materialNodeIndex,
            u32 chunkId,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
};

#endif
