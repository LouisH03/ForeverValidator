#ifndef TMNF_STATIC_SOLID_ARCHIVE_VISUAL_SURFACE_READER_H
#define TMNF_STATIC_SOLID_ARCHIVE_VISUAL_SURFACE_READER_H

#include "engine/core/engine_types.h"
#include "format/archive/archive_node_reference.h"
#include "format/static_solid/static_solid_archive_id.h"
#include "format/static_solid/static_solid_archive_node_ref_reader.h"
class CGameCtnReplayStaticSolidArchiveByteStream;
struct CGameCtnReplayStaticSolidArchiveCMwIdState;
struct CGameCtnReplayStaticSolidArchiveDecodeProgress;
struct CGameCtnReplayStaticSolidArchiveFeedback;
class CGameCtnReplayStaticSolidArchiveNodeGraph;
class StaticSolidArchiveVisualState;
class StaticSolidArchiveLoadSession;
struct CGameCtnReplayStaticSolidArchiveChunkDispatchContext;

struct CPlugVisualGridArchivePayload {
    u32 nbPointX = 0u;
    u32 nbPointZ = 0u;
    float rangeX = 0.0f;
    float rangeZ = 0.0f;

    int Chunk(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
              u32 chunkId);
};

struct CGameCtnReplayStaticSolidArchiveVisualSurfaceReader {
  static int ParseVisualSurfaceChunk(
      const CGameCtnReplayStaticSolidArchiveChunkDispatchContext &context,
      u32 classId, u32 chunkId);
};

#endif
