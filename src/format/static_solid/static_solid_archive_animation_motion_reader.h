#ifndef TMNF_STATIC_SOLID_ARCHIVE_ANIMATION_MOTION_READER_H
#define TMNF_STATIC_SOLID_ARCHIVE_ANIMATION_MOTION_READER_H

#include <array>
#include <map>
#include <optional>

#include "engine/core/engine_types.h"
#include "format/archive/archive_node_reference.h"
#include "format/static_solid/static_solid_archive_node_ref_reader.h"
#include "format/static_solid/static_solid_archive_stream_roles.h"
class CGameCtnReplayStaticSolidArchiveByteStream;
struct CGameCtnReplayStaticSolidArchiveCMwIdState;

struct CMotionEmitterLeavesArchiveDefinition {
    ArchiveNodeReference managerModel = ArchiveNodeReference::Invalid();
    std::array<float, 3u> pos{};
    std::array<float, 3u> radius{{1.0f, 1.0f, 1.0f}};
    u32 sourceChunkId = 0u;
};

struct CMotionManagerLeavesArchiveDefinition {
    ArchiveNodeReference mobilModel = ArchiveNodeReference::Invalid();
};

class CGameCtnReplayStaticSolidArchiveAnimationMotionState {
public:
    void Reset();
    int RememberCFuncSkelBoneCount(
            ArchiveNodeReference skeletonNode,
            u32 boneCount);
    int RememberCFuncKeysSkelSkeleton(
            ArchiveNodeReference keysNode,
            ArchiveNodeReference skeletonNode);
    std::optional<u32> CFuncKeysSkelBoneCount(
            ArchiveNodeReference keysNode) const;
    int RememberCMotionEmitterLeaves(
            ArchiveNodeReference emitterNode,
            const CMotionEmitterLeavesArchiveDefinition &definition);
    int RememberCMotionManagerLeaves(
            ArchiveNodeReference managerNode,
            const CMotionManagerLeavesArchiveDefinition &definition);
    std::optional<CMotionEmitterLeavesArchiveDefinition>
    CMotionEmitterLeavesDefinition(
            ArchiveNodeReference emitterNode) const;
    std::optional<CMotionManagerLeavesArchiveDefinition>
    CMotionManagerLeavesDefinition(
            ArchiveNodeReference managerNode) const;

private:
    std::map<ArchiveNodeReference::IndexType, u32> skeletonBoneCounts_;
    std::map<ArchiveNodeReference::IndexType,
             ArchiveNodeReference::IndexType> keysSkeletonNodes_;
    std::map<ArchiveNodeReference::IndexType,
             CMotionEmitterLeavesArchiveDefinition> emitterLeaves_;
    std::map<ArchiveNodeReference::IndexType,
             CMotionManagerLeavesArchiveDefinition> managerLeaves_;
};

struct CAnimationMotionDiscardedRefsPayload {
    static int ReadSingle(
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
    static int ReadCountedArray(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
};

struct CFuncKeysNaturalArchivePayload {
    explicit CFuncKeysNaturalArchivePayload(
            CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState);

    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
             u32 chunkId);

private:
    CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState_;
};

struct CFuncSkelArchivePayload {
    CFuncSkelArchivePayload(
            CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
            CGameCtnReplayStaticSolidArchiveAnimationMotionState *state,
            ArchiveNodeReference currentArchiveNode);

    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
             u32 chunkId);

private:
    int ReadLegacyTable(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            int idsInsteadOfStrings);
    int ReadBones(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            int hasElementArrays);

    CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState_;
    CGameCtnReplayStaticSolidArchiveAnimationMotionState *state_;
    ArchiveNodeReference currentArchiveNode_;
};

struct CFuncKeysSkelArchivePayload {
    CFuncKeysSkelArchivePayload(
            CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs,
            CGameCtnReplayStaticSolidArchiveAnimationMotionState *state,
            ArchiveNodeReference currentArchiveNode);

    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
             u32 chunkId);

private:
    CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState_;
    CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs_;
    CGameCtnReplayStaticSolidArchiveAnimationMotionState *state_;
    ArchiveNodeReference currentArchiveNode_;
};

struct CFuncTreeSubVisualSequenceArchivePayload {
    CFuncTreeSubVisualSequenceArchivePayload(
            CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs,
            CGameCtnReplayStaticSolidArchiveEmbeddedNodeParser *embeddedNodes);

    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
             u32 chunkId);

private:
    CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState_;
    CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs_;
    CGameCtnReplayStaticSolidArchiveEmbeddedNodeParser *embeddedNodes_;
};

struct CMotionPlayerArchivePayload {
    CMotionPlayerArchivePayload(
            CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs,
            CGameCtnReplayStaticSolidArchiveEmbeddedNodeParser *embeddedNodes);

    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
             u32 chunkId);

private:
    int ReadOptionalRef(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream);
    int ReadBaseCommandAndRefArray(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            int hasTrackRef,
            int hasStateBool);

    CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState_;
    CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs_;
    CGameCtnReplayStaticSolidArchiveEmbeddedNodeParser *embeddedNodes_;
};

struct CMotionsArchivePayload {
    CMotionsArchivePayload(
            CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);

    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
             u32 chunkId);

private:
    CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState_;
    CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs_;
};

struct CMotionSkelArchivePayload {
    explicit CMotionSkelArchivePayload(
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);

    int Read(u32 chunkId);

private:
    CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs_;
};

struct CMotionEmitterLeavesArchivePayload {
    CMotionEmitterLeavesArchivePayload(
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs,
            CGameCtnReplayStaticSolidArchiveAnimationMotionState *state,
            ArchiveNodeReference currentArchiveNode);

    int Chunk(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
              u32 chunkId);

private:
    CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs_;
    CGameCtnReplayStaticSolidArchiveAnimationMotionState *state_;
    ArchiveNodeReference currentArchiveNode_;
};

struct CMotionManagerLeavesArchivePayload {
    CMotionManagerLeavesArchivePayload(
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs,
            CGameCtnReplayStaticSolidArchiveAnimationMotionState *state,
            ArchiveNodeReference currentArchiveNode);

    int Chunk(u32 chunkId);

private:
    CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs_;
    CGameCtnReplayStaticSolidArchiveAnimationMotionState *state_;
    ArchiveNodeReference currentArchiveNode_;
};

struct CMotionDayTimeArchivePayload {
    CMotionDayTimeArchivePayload(
            CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);

    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
             u32 chunkId);

private:
    CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState_;
    CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs_;
};

struct CMotionWeatherArchivePayload {
    CMotionWeatherArchivePayload(
            CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);

    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
             u32 chunkId);

private:
    CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState_;
    CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs_;
};

struct CMotionCmdBaseArchivePayload {
    int Read(CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
             u32 chunkId);
};

struct CMotionShaderArchivePayload {
    explicit CMotionShaderArchivePayload(
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);

    int Read(u32 chunkId);

private:
    CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs_;
};

struct CGameCtnReplayStaticSolidArchiveAnimationMotionReader {
    static int ParseAnimationMotionChunk(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
            u32 classId,
            u32 chunkId,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs,
            CGameCtnReplayStaticSolidArchiveEmbeddedNodeParser *embeddedNodes,
            CGameCtnReplayStaticSolidArchiveAnimationMotionState *state =
                    nullptr,
            ArchiveNodeReference currentArchiveNode =
                    ArchiveNodeReference::Invalid());
};

#endif
