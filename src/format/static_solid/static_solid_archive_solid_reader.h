#ifndef TMNF_STATIC_SOLID_ARCHIVE_SOLID_READER_H
#define TMNF_STATIC_SOLID_ARCHIVE_SOLID_READER_H

#include "engine/core/engine_types.h"
#include "format/archive/archive_node_reference.h"
#include "format/static_solid/static_scene_archive_loader.h"
#include "format/static_solid/static_solid_archive_id.h"
#include "format/static_solid/static_solid_archive_node_ref_reader.h"
class CGameCtnReplayStaticSolidArchiveByteStream;
class CGameCtnReplayStaticSolidArchiveNodeGraph;
class StaticSolidArchiveLoadSession;
struct CGameCtnReplayStaticSolidArchiveChunkDispatchContext;

class CPlugSolidPhysicalArchivePayload {
public:
  void Reset(StaticSolidArchiveId payload, u32 solidNodeIndex, u32 chunkId);
  int ReadLegacyPhysicalDefinition(
      CGameCtnReplayStaticSolidArchiveByteStream *byteStream);
  int ReadPhysicalDefinition(
      CGameCtnReplayStaticSolidArchiveByteStream *byteStream);
  int Install(StaticSolidArchiveLoadSession *store) const;

private:
  CGameCtnReplayStaticSolidArchiveSolidPhysicsDefinition definition = {};

  int ReadMassCenterAndImpulseInertia(
      CGameCtnReplayStaticSolidArchiveByteStream *byteStream);
};

class CPlugSolidTreeLinkArchivePayload {
public:
  void Reset(StaticSolidArchiveId payload, u32 solidNodeIndex, u32 chunkId);
  int ReadTreeLinkWithMode(
      CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
      CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
  int ReadTreeLink(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
                   CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
  int ReadTreeLinkWithFidModel(
      CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
      CGameCtnReplayStaticSolidArchiveNodeGraph *archiveNodeGraph,
      CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
  int Install(StaticSolidArchiveLoadSession *store) const;

private:
  StaticSolidArchiveId payload = StaticSolidArchiveId::Invalid();
  ArchiveNodeReference solidNode = ArchiveNodeReference::Invalid();
  u32 chunkId = 0u;
  ArchiveNodeReference selectedTreeNode = ArchiveNodeReference::Invalid();

  int ReadUseModelFlags(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
                        u32 *useModel, u32 *hasModel) const;
  int ReadOptionalModelNode(
      u32 hasModel,
      CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs) const;
  int ReadTreeIfSolidDoesNotUseModel(
      u32 useModel, u32 hasModel,
      CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
};

struct CGameCtnReplayStaticSolidArchiveSolidReader {
  static int ParseCPlugSolidChunk(
      const CGameCtnReplayStaticSolidArchiveChunkDispatchContext &context,
      u32 chunkId);
};

#endif
