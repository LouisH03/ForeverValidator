#ifndef TMNF_STATIC_SOLID_ARCHIVE_SCENE_READER_H
#define TMNF_STATIC_SOLID_ARCHIVE_SCENE_READER_H

#include "engine/core/engine_types.h"
#include "format/archive/archive_node_reference.h"
#include "format/static_solid/static_solid_archive_id.h"
#include "format/static_solid/static_solid_archive_node_ref_reader.h"
#include "format/static_solid/static_solid_archive_stream_roles.h"
class CGameCtnReplayStaticSolidArchiveByteStream;
struct CGameCtnReplayStaticSolidArchiveCMwIdState;
class CGameCtnReplayStaticSolidArchiveNodeGraph;
class CGameCtnReplayArchiveStaticModelCollection;
class StaticSolidArchiveLoadSession;
struct SceneDescriptorFolderPaths;
struct CGameCtnReplayStaticSolidArchiveChunkDispatchContext;

struct CGameCtnReplayStaticSolidArchiveSceneReader {
  static int ParseSceneChunk(
      const CGameCtnReplayStaticSolidArchiveChunkDispatchContext &context,
      u32 classId, u32 chunkId);
};

#endif
