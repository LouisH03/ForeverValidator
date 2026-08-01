#ifndef TMNF_STATIC_SOLID_ARCHIVE_DECORATOR_TREE_READER_H
#define TMNF_STATIC_SOLID_ARCHIVE_DECORATOR_TREE_READER_H

#include "engine/core/engine_types.h"
#include "format/archive/archive_node_reference.h"
#include "format/static_solid/static_scene_archive_loader.h"
#include "format/static_solid/static_solid_archive_id.h"
#include "format/static_solid/static_solid_archive_node_ref_reader.h"
class CGameCtnReplayStaticSolidArchiveByteStream;
struct CGameCtnReplayStaticSolidArchiveCMwIdState;
class CGameCtnReplayStaticSolidArchiveNodeGraph;
class StaticSolidArchiveLoadSession;
struct SceneDescriptorFolderPaths;
struct CGameCtnReplayStaticSolidArchiveChunkDispatchContext;

class CPlugDecoratorTreeArchivePayload {
public:
  void Reset(StaticSolidArchiveId payload, u32 decoratorNodeIndex, u32 chunkId);
  int ReadFromArchive(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
                      CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
                      CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
  int Install(CGameCtnReplayStaticSolidArchiveNodeGraph *archiveNodeGraph,
              const SceneDescriptorFolderPaths *externalFolders,
              StaticSolidArchiveLoadSession *store) const;

private:
  CGameCtnReplayStaticSolidDecoratorTreeDeclaration declaration = {};

  int ReadBoolOnlySurfaceFromVisual(
      CGameCtnReplayStaticSolidArchiveByteStream *byteStream);
  int ReadTreeIdAndRefs(
      CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
      CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
      CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
  int ReadRootMaterialOnlyTail(
      CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
      CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
  int ReadLegacyVisibilityTail(
      CGameCtnReplayStaticSolidArchiveByteStream *byteStream);
  int ReadConditionalTail(
      CGameCtnReplayStaticSolidArchiveByteStream *byteStream);
};

struct CGameCtnReplayStaticSolidArchiveDecoratorTreeReader {
  static int ParseCPlugDecoratorTreeChunk(
      const CGameCtnReplayStaticSolidArchiveChunkDispatchContext &context,
      u32 chunkId);
};

#endif
