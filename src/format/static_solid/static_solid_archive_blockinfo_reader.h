#ifndef TMNF_STATIC_SOLID_ARCHIVE_BLOCKINFO_READER_H
#define TMNF_STATIC_SOLID_ARCHIVE_BLOCKINFO_READER_H


#include "engine/core/engine_types.h"
#include "engine/game/game_ctn_block_info.h"
#include "format/static_solid/static_solid_archive_node_ref_reader.h"
class CGameCtnReplayStaticSolidArchiveByteStream;
struct CGameCtnReplayStaticSolidArchiveCMwIdState;

struct CGameCtnBlockInfoDiscardedRefsPayload {
    static int ReadSingle(
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
    static int ReadUnitArray(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
    static int ReadMobilVariants(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
};

struct CGameCtnBlockInfoCommonArchivePayload {
    explicit CGameCtnBlockInfoCommonArchivePayload(u32 chunkId);

    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
             CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
             CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);

private:
    u32 chunkId_;
};

struct CGameCtnBlockUnitInfoArchivePayload {
    CGameCtnBlockUnitInfoArchivePayload(
            CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);

    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
             u32 chunkId);

private:
    int ReadSourceRefs(CGameCtnReplayStaticSolidArchiveByteStream *byteStream);

    CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState_;
    CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs_;
};

struct CGameCtnBlockInfoFamilyArchivePayload {
    CGameCtnBlockInfoFamilyArchivePayload(
            CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);

    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
             u32 chunkId);

private:
    int ReadCollectorLoadableNodRefAndId(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream);
    CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState_;
    CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs_;
};

struct CGameCtnReplayStaticSolidArchiveBlockInfoReader {
    static int ParseBlockInfoChunk(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
            u32 classId,
            u32 chunkId,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
};

#endif
