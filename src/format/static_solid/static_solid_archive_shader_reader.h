#ifndef TMNF_STATIC_SOLID_ARCHIVE_SHADER_READER_H
#define TMNF_STATIC_SOLID_ARCHIVE_SHADER_READER_H

#include "engine/core/engine_types.h"

class CGameCtnReplayStaticSolidArchiveByteStream;
struct CGameCtnReplayStaticSolidArchiveCMwIdState;
class CGameCtnReplayStaticSolidArchiveNodeRefReader;

struct CGameCtnReplayStaticSolidArchiveShaderReader {
    static int ParseCPlugShaderChunk(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
            u32 classId,
            u32 chunkId,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
};

#endif
