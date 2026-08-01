#ifndef TMNF_STATIC_SOLID_ARCHIVE_SCENE_PAYLOADS_H
#define TMNF_STATIC_SOLID_ARCHIVE_SCENE_PAYLOADS_H


#include "engine/core/engine_types.h"
#include "format/static_solid/static_solid_archive_node_ref_reader.h"
#include "format/archive/archive_node_reference.h"
#include "format/static_solid/static_solid_archive_stream_roles.h"
#include "format/static_solid/static_solid_archive_id.h"
class CGameCtnReplayStaticSolidArchiveByteStream;
struct CGameCtnReplayStaticSolidArchiveCMwIdState;
class CGameCtnReplayStaticSolidArchiveNodeGraph;
class CGameCtnReplayArchiveStaticModelCollection;
class StaticSolidArchiveLoadSession;
struct SceneDescriptorFolderPaths;

struct CSceneArchivePayloadContext {
    CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState = nullptr;
    CGameCtnReplayStaticSolidArchiveNodeGraph *archiveNodeGraph = nullptr;
    const SceneDescriptorFolderPaths *externalFolders = nullptr;
    StaticSolidArchiveLoadSession *materialStore = nullptr;
    CGameCtnReplayArchiveStaticModelCollection *archiveModels = nullptr;
    StaticSolidArchiveId payload =
            StaticSolidArchiveId::Invalid();
    ArchiveNodeReference currentArchiveNode =
            ArchiveNodeReference::Invalid();
    CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs = nullptr;
    CGameCtnReplayStaticSolidArchiveNodeParser *nodeParser = nullptr;

    int HasNodeArchiveCore() const;
    int HasSceneObjectInstallTargets() const;
};

struct CSceneArchiveNodeRefPayload {
    static int ReadSingle(
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
    static int ReadCountedArray(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
    static int ReadFastBuffer(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
};

struct CSceneMobilReferencePayload {
    CSceneMobilReferencePayload(
            CGameCtnReplayStaticSolidArchiveNodeGraph *archiveNodeGraph,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);

    int ReadBeforeIso(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
                      CGameCtnReplayStaticSolidArchiveNodeParser *nodeParser);
    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
             int internalRefMode,
             ArchiveNodeReference *mobilNodeOut);

private:
    CGameCtnReplayStaticSolidArchiveNodeGraph *archiveNodeGraph_;
    CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs_;
};

struct CSceneObjectPlacementArchivePayload {
    explicit CSceneObjectPlacementArchivePayload(
            const CSceneArchivePayloadContext &context);

    int Append(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
               ArchiveNodeReference mobilNode,
               const float sceneIso[12]);

private:
    CSceneArchivePayloadContext context_;
};

struct CSceneObjectBufferArchivePayload {
    explicit CSceneObjectBufferArchivePayload(
            const CSceneArchivePayloadContext &context);

    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
             int mobilBuffer);

private:
    CSceneArchivePayloadContext context_;
};

struct CScene3dObjectBuffersArchivePayload {
    explicit CScene3dObjectBuffersArchivePayload(
            const CSceneArchivePayloadContext &context);

    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
             u32 chunkId);

private:
    int ReadTrailingMemoryArchiveChunks(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream);

    CSceneArchivePayloadContext context_;
};

struct CSceneSectorArchivePayload {
    CSceneSectorArchivePayload(
            CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);

    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
             u32 chunkId);

private:
    CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState_;
    CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs_;
};

struct CScene3dArchivePayload {
    explicit CScene3dArchivePayload(
            const CSceneArchivePayloadContext &context);

    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
             u32 chunkId);

private:
    int ReadRefLocBuffer(CGameCtnReplayStaticSolidArchiveByteStream *byteStream);
    int ReadTrafficPairLocs(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream);
    CSceneArchivePayloadContext context_;
};

struct CSceneObjectOrMobilArchivePayload {
    explicit CSceneObjectOrMobilArchivePayload(
            const CSceneArchivePayloadContext &context);

    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
             u32 chunkId);

private:
    int ReadSceneObjectId(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream);
    int ReadVehicleRefCluster(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            u32 chunkId);

    CSceneArchivePayloadContext context_;
};

struct CSceneMobilLeavesArchivePayload {
    explicit CSceneMobilLeavesArchivePayload(
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);

    int Chunk(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
              u32 chunkId);

private:
    CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs_;
};

struct CSceneVehicleEnvironmentArchivePayload {
    explicit CSceneVehicleEnvironmentArchivePayload(
            const CSceneArchivePayloadContext &context);

    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
             u32 chunkId);

private:
    CSceneArchivePayloadContext context_;
};

struct CSceneObjectLinkArchivePayload {
    explicit CSceneObjectLinkArchivePayload(
            const CSceneArchivePayloadContext &context);

    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
             u32 chunkId);

private:
    CSceneArchivePayloadContext context_;
};

#endif
