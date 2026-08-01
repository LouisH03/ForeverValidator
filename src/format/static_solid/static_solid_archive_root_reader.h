#ifndef TMNF_STATIC_SOLID_ARCHIVE_ROOT_READER_H
#define TMNF_STATIC_SOLID_ARCHIVE_ROOT_READER_H

#include "engine/core/engine_types.h"

class CGameCtnReplayStaticSolidArchiveByteStream;
class CGameCtnReplayStaticSolidArchiveNodeGraph;
struct SceneDescriptorFolderPaths;

struct CGameCtnReplayStaticSolidArchiveRootHeader {
    u32 version = 0u;
    u32 classId = 0u;
    u32 headerSize = 0u;
    u32 numNodes = 0u;

    void Clear();
};

struct CGameCtnReplayStaticSolidArchiveRootReader {
    static int ReadGbxRootArchive(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveNodeGraph *archiveNodeGraph,
            SceneDescriptorFolderPaths *externalFolders,
            u32 maxPayloadByteCount,
            CGameCtnReplayStaticSolidArchiveRootHeader *headerOut);

private:
    static int ParseRefSection(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveNodeGraph *archiveNodeGraph,
            SceneDescriptorFolderPaths *externalFolders,
            u32 version,
            u32 *numNodesOut);
};

#endif
