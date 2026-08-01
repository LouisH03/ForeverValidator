#ifndef TMNF_STATIC_SOLID_ARCHIVE_HMS_READER_H
#define TMNF_STATIC_SOLID_ARCHIVE_HMS_READER_H


#include "engine/core/engine_types.h"
#include "format/archive/archive_node_reference.h"
#include "format/static_solid/static_solid_archive_node_ref_reader.h"
class CGameCtnReplayStaticSolidArchiveByteStream;
class CGameCtnReplayStaticSolidArchiveNodeGraph;

struct CHmsArchiveDiscardedRefsPayload {
    static int ReadSingle(
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
    static int ReadFastBuffer(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
};

struct CHmsReflectGroundArchivePayload {
    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream);
};

struct CHmsItemSolidArchivePayload {
    explicit CHmsItemSolidArchivePayload(u32 hmsNodeIndex);

    int Read(CGameCtnReplayStaticSolidArchiveNodeGraph *archiveNodeGraph,
             CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);

private:
    ArchiveNodeReference hmsNode_;
    ArchiveNodeReference solidNode_;
};

struct CHmsSoundSourceArchivePayload {
    static int ReadSoundRef(
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
    static int ReadSoundRefRealBool(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
};

struct CHmsLightArchivePayload {
    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
             u32 chunkId,
             CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);

private:
};

struct CGameCtnReplayStaticSolidArchiveHmsReader {
    static int ParseHmsChunk(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveNodeGraph *archiveNodeGraph,
            u32 hmsNodeIndex,
            u32 classId,
            u32 chunkId,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);

private:
    static int ParseCHmsZoneChunk(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            u32 chunkId,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
    static int ParseCHmsItemChunk(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveNodeGraph *archiveNodeGraph,
            u32 hmsNodeIndex,
            u32 chunkId,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
    static int ParseCHmsSoundSourceChunk(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            u32 chunkId,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
};

#endif
