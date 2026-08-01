#ifndef TMNF_STATIC_SOLID_ARCHIVE_PRIMITIVE_READER_H
#define TMNF_STATIC_SOLID_ARCHIVE_PRIMITIVE_READER_H

#include "engine/core/engine_types.h"

class CGameCtnReplayStaticSolidArchiveByteStream;

struct CGameCtnReplayStaticSolidArchivePrimitiveReader {
    static int SkipFloatBuffer(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream);
    static int SkipReals(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            u32 count);
    static int ReadIso4(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            float iso[12]);
    static int SkipMat3(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream);
    static int SkipIso4(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream);
    static int SkipBoxAligned(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream);
    static int SkipFrustumIso4(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream);
    static int SkipD3dFormatArray(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream);
};

#endif
