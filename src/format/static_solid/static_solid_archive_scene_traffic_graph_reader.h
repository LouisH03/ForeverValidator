#ifndef TMNF_STATIC_SOLID_ARCHIVE_SCENE_TRAFFIC_GRAPH_READER_H
#define TMNF_STATIC_SOLID_ARCHIVE_SCENE_TRAFFIC_GRAPH_READER_H

#include <map>

#include "engine/core/engine_types.h"
#include "format/archive/archive_node_reference.h"

class CGameCtnReplayStaticSolidArchiveByteStream;
struct CGameCtnReplayStaticSolidArchiveCMwIdState;
class CGameCtnReplayStaticSolidArchiveNodeRefReader;

class CSceneTrafficGraphArchiveState {
public:
    void Reset();
    u32 HubCount(ArchiveNodeReference nodeRef) const;
    int ReplaceHubCount(ArchiveNodeReference nodeRef, u32 hubCount);

private:
    std::map<ArchiveNodeReference::IndexType, u32> nodeHubCounts_;
};

class CSceneTrafficGraphArchivePayload {
public:
    CSceneTrafficGraphArchivePayload(
            CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs,
            CSceneTrafficGraphArchiveState *state,
            ArchiveNodeReference currentArchiveNode);

    int Chunk(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
              u32 chunkId);

private:
    CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState_ = nullptr;
    CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs_ = nullptr;
    CSceneTrafficGraphArchiveState *state_ = nullptr;
    ArchiveNodeReference currentArchiveNode_ =
            ArchiveNodeReference::Invalid();

    int ReadHubsAndEdges(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream);
    int ReadLegacyHubRecords(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream);
};

#endif
