#ifndef TMNF_STATIC_SOLID_ARCHIVE_SURFACE_GEOM_READER_H
#define TMNF_STATIC_SOLID_ARCHIVE_SURFACE_GEOM_READER_H

#include "engine/core/engine_types.h"
#include "engine/core/gm_types.h"
#include "format/archive/archive_node_reference.h"
#include "format/static_solid/static_solid_archive_definitions.h"
#include "format/static_solid/static_solid_archive_id.h"
class CGameCtnReplayStaticSolidArchiveByteStream;
struct CGameCtnReplayStaticSolidArchiveCMwIdState;
struct CGameCtnReplayStaticSolidArchiveDecodeProgress;
struct CGameCtnReplayStaticSolidArchiveFeedback;
class StaticSolidArchiveLoadSession;
struct CGameCtnReplayStaticSolidArchiveChunkDispatchContext;

class CGameCtnReplayStaticSolidSurfaceGeometryDefinition {
public:
  static CGameCtnReplayStaticSolidSurfaceGeometryDefinition
  FromArchive(StaticSolidArchiveId payload, ArchiveNodeReference geomNode,
              u32 surfaceType, uint16_t materialId,
              const GmBoxAligned &boundingBox,
              CGameCtnReplayStaticSolidArchiveMeshPayload meshPayload);

  int Commit(StaticSolidArchiveLoadSession *store) const;
  CGameCtnReplayStaticSolidArchiveSurfaceGeometryDefinition
  ArchiveDefinition() const;

private:
  CGameCtnReplayStaticSolidArchiveNodeIdentity geom =
      CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
  u32 surfaceType = 0u;
  uint16_t materialId = 0u;
  GmBoxAligned boundingBox{};
  CGameCtnReplayStaticSolidArchiveMeshPayload meshPayload =
      CGameCtnReplayStaticSolidArchiveMeshPayload::Empty();
};

struct CGameCtnReplayStaticSolidArchiveSurfaceGeomReader {
  static int ParseSurfaceGeometry(
      const CGameCtnReplayStaticSolidArchiveChunkDispatchContext &context);

private:
  struct GmSurfMeshArchiveRecords {
    u32 vertexCount = 0u;
    u32 triangleCount = 0u;
    u32 octreeCellCount = 0u;
    u32 vertexByteOffset = 0u;
    u32 triangleByteOffset = 0u;
    u32 octreeCellByteOffset = 0u;
  };

  static int ParseGmSurfMeshArchive(
      CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
      CGameCtnReplayStaticSolidArchiveDecodeProgress *decodeProgress,
      GmSurfMeshArchiveRecords *recordsOut);
};

#endif
