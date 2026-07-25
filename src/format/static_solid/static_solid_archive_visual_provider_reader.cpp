#include "format/static_solid/static_solid_archive_visual_provider_reader.h"
#include "format/static_solid/static_scene_archive_loader.h"
#include "format/static_solid/static_solid_archive_byte_stream.h"
#include "format/static_solid/static_solid_archive_class_info.h"
#include "format/static_solid/static_solid_archive_decode_progress.h"
#include "format/static_solid/static_solid_archive_feedback.h"
#include "format/static_solid/static_solid_archive_visual_provider_state.h"
#include "format/archive/tmnf_archive_ids.h"
#include "format/archive/gbx_archive_format.h"
#include <new>
#include <vector>
namespace {

class CPlugVisualTexCoordArchiveStream {
public:
    CPlugVisualTexCoordArchiveStream() = default;
    static int FromArchiveWord(u32 archiveWord,
                               CPlugVisualTexCoordArchiveStream *streamOut);
    u32 BytesPerVertex() const;
    u32 Dimension() const;

private:
    explicit CPlugVisualTexCoordArchiveStream(u32 bytesPerVertex);

    u32 bytesPerVertex = 0u;
};

struct CGameCtnReplayStaticSolidIndexBufferRecords {
    u32 indexCount = 0u;
    CGameCtnReplayStaticSolidArchivePayloadSlice indices =
            CGameCtnReplayStaticSolidArchivePayloadSlice::Empty();
};

int CPlugVisualTexCoordArchiveStream::FromArchiveWord(
        u32 archiveWord,
        CPlugVisualTexCoordArchiveStream *streamOut) {
    if (streamOut == nullptr) {
        return 0;
    }
    switch (archiveWord & 0xffu) {
        case 0u:
            *streamOut = CPlugVisualTexCoordArchiveStream(8u);
            return 1;
        case 1u:
            *streamOut = CPlugVisualTexCoordArchiveStream(12u);
            return 1;
        case 2u:
            *streamOut = CPlugVisualTexCoordArchiveStream(16u);
            return 1;
        default:
            return 0;
    }
}

CPlugVisualTexCoordArchiveStream::CPlugVisualTexCoordArchiveStream(
        u32 newBytesPerVertex)
        : bytesPerVertex(newBytesPerVertex) {}

u32 CPlugVisualTexCoordArchiveStream::BytesPerVertex() const {
    return bytesPerVertex;
}

u32 CPlugVisualTexCoordArchiveStream::Dimension() const {
    return bytesPerVertex / sizeof(float);
}

int ApplyIndexBufferFeedback(
        CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
        CGameCtnReplayStaticSolidArchiveFeedback *feedback) {
    if (byteStream == nullptr || feedback == nullptr) {
        return 0;
    }
    const u32 feedbackClass =
            CGameCtnReplayStaticSolidArchiveClassInfo::FeedbackClassId(
                    TMNF_CLASS_CPlugIndexBuffer);
    return feedbackClass == 0u ||
           byteStream->ApplyFeedbackValue(*feedback, feedbackClass, 1);
}

int ParseIndexBufferArchive(
        CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
        CGameCtnReplayStaticSolidArchiveFeedback *feedback,
        CGameCtnReplayStaticSolidArchiveDecodeProgress *decodeProgress,
        CGameCtnReplayStaticSolidIndexBufferRecords *recordsOut) {
    if (byteStream == nullptr || recordsOut == nullptr ||
        !ApplyIndexBufferFeedback(byteStream, feedback)) {
        return 0;
    }

    CGameCtnReplayStaticSolidIndexBufferRecords last;
    for (;;) {
        u32 chunkId = 0u;
        if (!byteStream->ReadU32(&chunkId)) {
            if (decodeProgress != nullptr) {
                decodeProgress->MarkFailure(TMNF_CLASS_CPlugIndexBuffer,
                                            0u,
                                            byteStream->Offset(),
                                            byteStream->CompressedRead());
            }
            return 0;
        }
        if (chunkId == CMwNodArchiveFacadeSentinel) {
            *recordsOut = last;
            return 1;
        }
        if (chunkId != TMNF_CLASS_CPlugIndexBuffer) {
            return 0;
        }

        u32 ignoredIndexBufferFormat = 0u;
        u32 indexCount = 0u;
        if (!byteStream->ReadU32(&ignoredIndexBufferFormat) ||
            !byteStream->ReadU32(&indexCount) ||
            indexCount > UINT32_MAX / 2u ||
            !byteStream->SkipCounted(indexCount, 2u)) {
            return 0;
        }
        (void)ignoredIndexBufferFormat;
        last.indexCount = indexCount;
        last.indices = CGameCtnReplayStaticSolidArchivePayloadSlice::
                FromRecords(byteStream->Offset() - indexCount * 2u,
                            indexCount,
                            2u);
    }
}

}  // namespace

int StaticSolidArchiveVisualReader::
        ParseVisualGeometryProvider(
                CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
        StaticSolidArchiveVisualState *state,
        StaticSolidArchiveLoadSession *store,
        StaticSolidArchiveId payload,
        ArchiveNodeReference visualNode) {
    if (byteStream == nullptr || state == nullptr) {
        return 0;
    }

    u32 flags = 0u;
    u32 texcoordStreamCount = 0u;
    u32 vertexCount = 0u;
    u32 vertexStreamRefCount = 0u;
    if (!byteStream->ReadU32(&flags) ||
        !byteStream->ReadU32(&texcoordStreamCount) ||
        !byteStream->ReadU32(&vertexCount) ||
        !byteStream->ReadU32(&vertexStreamRefCount) ||
        texcoordStreamCount > 0x100000u ||
        vertexCount > 0x1000000u ||
        vertexStreamRefCount != 0u) {
        return 0;
    }

    struct ParsedTexCoordStream {
        u32 dimension = 2u;
        CGameCtnReplayStaticSolidArchivePayloadSlice records =
                CGameCtnReplayStaticSolidArchivePayloadSlice::Empty();
    };
    std::vector<ParsedTexCoordStream> texCoordStreams;
    try {
        texCoordStreams.reserve(texcoordStreamCount);
    } catch (const std::bad_alloc &) {
        return 0;
    }
    for (u32 i = 0u; i < texcoordStreamCount; i++) {
        u32 texCoordArchiveWord = 0u;
        if (!byteStream->ReadU32(&texCoordArchiveWord)) {
            return 0;
        }
        CPlugVisualTexCoordArchiveStream texCoordStream;
        if (!CPlugVisualTexCoordArchiveStream::FromArchiveWord(
                    texCoordArchiveWord,
                    &texCoordStream)) {
            return 0;
        }
        const u32 recordOffset = byteStream->Offset();
        if (!byteStream->SkipCounted(vertexCount,
                                     texCoordStream.BytesPerVertex())) {
            return 0;
        }
        try {
            texCoordStreams.push_back({
                    texCoordStream.Dimension(),
                    CGameCtnReplayStaticSolidArchivePayloadSlice::
                            FromRecords(
                                    recordOffset,
                                    vertexCount,
                                    texCoordStream.BytesPerVertex())});
        } catch (const std::bad_alloc &) {
            return 0;
        }
    }
    if ((flags & 7u) != 0u) {
        return 0;
    }

    GmBoxAligned boundingBox;
    if (!byteStream->ReadF32(&boundingBox.center.x) ||
        !byteStream->ReadF32(&boundingBox.center.y) ||
        !byteStream->ReadF32(&boundingBox.center.z) ||
        !byteStream->ReadF32(&boundingBox.halfExtents.x) ||
        !byteStream->ReadF32(&boundingBox.halfExtents.y) ||
        !byteStream->ReadF32(&boundingBox.halfExtents.z)) {
        return 0;
    }

    CGameCtnReplayStaticSolidVisualGeometryDefinition geometry =
            CGameCtnReplayStaticSolidVisualGeometryDefinition::
                    FromArchiveHeader(payload,
                                      visualNode.Index(),
                                      flags,
                                      vertexCount,
                                      boundingBox);
    for (const ParsedTexCoordStream &stream : texCoordStreams) {
        if (!geometry.AppendTexCoordStream(
                    stream.dimension, stream.records)) {
            return 0;
        }
    }
    if (!state->StartGeometryAndCommit(store, geometry)) {
        return 0;
    }

    u32 bitmapPackCount = 0u;
    if (!byteStream->ReadU32(&bitmapPackCount) ||
        bitmapPackCount > 0x10000000u) {
        return 0;
    }
    return byteStream->SkipCounted(bitmapPackCount, 0x14u);
}

int StaticSolidArchiveVisualReader::
        ParseVisual3dFaceStream(
                CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
                StaticSolidArchiveVisualState *state,
                StaticSolidArchiveLoadSession *store,
                u32 classId) {
    if (byteStream == nullptr || state == nullptr || !state->HasActiveGeometry()) {
        return 0;
    }
    CGameCtnReplayStaticSolidVisualGeometryDefinition &geometry =
            state->ActiveGeometry();
    u32 stride = geometry.FaceRecordStride();
    if (classId == TMNF_CLASS_CPlugVisualSprite) {
        // CPlugVisualSprite serializes eight additional bytes per vertex.
        stride += 8u;
    }
    const u32 vertexCount = geometry.VertexCount();
    if (vertexCount > UINT32_MAX / stride) {
        return 0;
    }
    const u32 vertexRecordByteOffset = byteStream->Offset();
    if (!byteStream->SkipCounted(vertexCount, stride)) {
        return 0;
    }
    geometry.BindVertexRecords(
            CGameCtnReplayStaticSolidArchivePayloadSlice::FromRecords(
                    vertexRecordByteOffset,
                    vertexCount,
                    stride));
    if (!state->CommitActiveGeometry(store)) {
        return 0;
    }

    u32 tangentCount = 0u;
    if (!byteStream->ReadU32(&tangentCount)) {
        return 0;
    }
    const u32 tangentOffset = byteStream->Offset();
    if (!byteStream->SkipCounted(tangentCount, 4u)) {
        return 0;
    }
    u32 binormalCount = 0u;
    if (!byteStream->ReadU32(&binormalCount)) {
        return 0;
    }
    const u32 binormalOffset = byteStream->Offset();
    if (!byteStream->SkipCounted(binormalCount, 4u)) {
        return 0;
    }
    geometry.BindTangents(
            CGameCtnReplayStaticSolidArchivePayloadSlice::FromRecords(
                    tangentOffset, tangentCount, 4u),
            CGameCtnReplayStaticSolidArchivePayloadSlice::FromRecords(
                    binormalOffset, binormalCount, 4u));
    return state->CommitActiveGeometry(store);
}

int StaticSolidArchiveVisualReader::
        ParseVisualIndexBuffer(
                CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
                CGameCtnReplayStaticSolidArchiveFeedback *feedback,
                CGameCtnReplayStaticSolidArchiveDecodeProgress *decodeProgress,
                StaticSolidArchiveLoadSession *store,
                StaticSolidArchiveVisualState *state) {
    if (byteStream == nullptr || state == nullptr) {
        return 0;
    }
    u32 hasIndexBuffer = 0u;
    if (!byteStream->ReadBool(&hasIndexBuffer)) {
        return 0;
    }
    if (hasIndexBuffer == 0u) {
        state->ActiveGeometry().BindIndexBuffer(
                0u,
                CGameCtnReplayStaticSolidArchivePayloadSlice::Empty());
        return state->CommitActiveGeometry(store);
    }

    CGameCtnReplayStaticSolidIndexBufferRecords indexBuffer;
    if (!ParseIndexBufferArchive(byteStream,
                                 feedback,
                                 decodeProgress,
                                 &indexBuffer)) {
        return 0;
    }
    state->ActiveGeometry().BindIndexBuffer(indexBuffer.indexCount,
                                            indexBuffer.indices);
    return state->CommitActiveGeometry(store);
}
