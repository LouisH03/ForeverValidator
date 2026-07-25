#include "format/static_solid/static_solid_archive_visual_provider_state.h"
#include <stdint.h>

#include "format/archive/archive_binary32.h"
#include "format/static_solid/static_scene_archive_loader.h"
#include "format/static_solid/static_solid_archive_graph.h"
namespace {

constexpr u32 kArchiveVertexHasNormal = 0x20u;
constexpr u32 kArchiveVertexHasColor = 0x40u;

u32 ReadU32(const uint8_t *bytes) {
    return TmnfArchiveBinary32::ReadU32LittleEndian(bytes);
}

float ReadF32(const uint8_t *bytes) {
    return TmnfArchiveBinary32::ReadLittleEndian(bytes);
}

void UnpackSnorm10(u32 packed, float *x, float *y, float *z) {
    const int sx = ((int32_t)(packed << 22u)) >> 22;
    const int sy = ((int32_t)(packed << 12u)) >> 22;
    const int sz = ((int32_t)(packed << 2u)) >> 22;
    *x = (float)((double)sx / 511.0);
    *y = (float)((double)sy / 511.0);
    *z = (float)((double)sz / 511.0);
}

void WriteColorFromBgra(float (&color)[4], u32 bgra) {
    const float scale = 1.0f / 255.0f;
    color[0] = (float)((double)((bgra >> 16u) & 0xffu) * (double)scale);
    color[1] = (float)((double)((bgra >> 8u) & 0xffu) * (double)scale);
    color[2] = (float)((double)(bgra & 0xffu) * (double)scale);
    color[3] = (float)((double)((bgra >> 24u) & 0xffu) * (double)scale);
}

} // namespace

CGameCtnReplayStaticSolidVisualVertexFormat
CGameCtnReplayStaticSolidVisualVertexFormat::FromSerializedFlags(
        u32 serializedFlags) {
    return CGameCtnReplayStaticSolidVisualVertexFormat(
            (serializedFlags & kArchiveVertexHasNormal) != 0u,
            (serializedFlags & kArchiveVertexHasColor) != 0u);
}

CGameCtnReplayStaticSolidVisualVertexFormat::
CGameCtnReplayStaticSolidVisualVertexFormat(bool newHasNormal,
                                            bool newHasColor)
        : hasNormal(newHasNormal), hasColor(newHasColor) {}

u32 CGameCtnReplayStaticSolidVisualVertexFormat::FaceRecordStride() const {
    u32 stride = 12u;
    if (hasNormal) {
        stride += 4u;
    }
    if (hasColor) {
        stride += 4u;
    }
    return stride;
}

int CGameCtnReplayStaticSolidVisualVertexFormat::CopyArchiveVerticesToGx(
        const uint8_t *sourceBytes,
        u32 sourceStride,
        u32 sourceByteCount,
        u32 vertexCount,
        GxVertex *destination) const {
    if (destination == nullptr ||
        sourceBytes == nullptr ||
        vertexCount == 0u ||
        sourceStride == 0u ||
        sourceByteCount < (size_t)vertexCount * (size_t)sourceStride ||
        sourceStride != FaceRecordStride()) {
        return 0;
    }

    for (u32 i = 0; i < vertexCount; i++) {
        const uint8_t *src =
                sourceBytes + (size_t)i * (size_t)sourceStride;
        GxVertex &vertex = destination[i];
        u32 srcOffset = 0u;

        vertex.position.x = ReadF32(src + srcOffset + 0u);
        vertex.position.y = ReadF32(src + srcOffset + 4u);
        vertex.position.z = ReadF32(src + srcOffset + 8u);
        srcOffset += 12u;

        if (hasNormal) {
            UnpackSnorm10(ReadU32(src + srcOffset),
                          &vertex.normal.x,
                          &vertex.normal.y,
                          &vertex.normal.z);
            srcOffset += 4u;
        } else {
            vertex.normal.x = 0.0f;
            vertex.normal.y = 1.0f;
            vertex.normal.z = 0.0f;
        }

        if (hasColor) {
            WriteColorFromBgra(vertex.color, ReadU32(src + srcOffset));
        } else {
            vertex.color[0] = 1.0f;
            vertex.color[1] = 1.0f;
            vertex.color[2] = 1.0f;
            vertex.color[3] = 1.0f;
        }
    }
    return 1;
}

CGameCtnReplayStaticSolidVisualGeometryDefinition
CGameCtnReplayStaticSolidVisualGeometryDefinition::FromArchiveHeader(
        StaticSolidArchiveId payload,
        u32 visualNodeIndex,
        u32 serializedFlags,
        u32 vertexCount,
        const GmBoxAligned &boundingBox) {
    CGameCtnReplayStaticSolidVisualGeometryDefinition geometry;
    geometry.definition.BeginGeometry(
            CGameCtnReplayStaticSolidArchiveNodeIdentity::FromPayloadAndArchiveIndex(
                    payload,
                    visualNodeIndex),
            serializedFlags,
            vertexCount,
            boundingBox);
    geometry.ready = 1;
    return geometry;
}

int CGameCtnReplayStaticSolidVisualGeometryDefinition::IsReady() const {
    return ready != 0;
}

u32 CGameCtnReplayStaticSolidVisualGeometryDefinition::VertexCount() const {
    return definition.VertexCount();
}

u32 CGameCtnReplayStaticSolidVisualGeometryDefinition::FaceRecordStride()
        const {
    return CGameCtnReplayStaticSolidVisualVertexFormat::FromSerializedFlags(
            definition.SerializedFlags()).FaceRecordStride();
}

void CGameCtnReplayStaticSolidVisualGeometryDefinition::BindVertexRecords(
        CGameCtnReplayStaticSolidArchivePayloadSlice vertexRecords) {
    definition.BindVertexRecords(vertexRecords);
}

bool CGameCtnReplayStaticSolidVisualGeometryDefinition::
        AppendTexCoordStream(
                u32 dimension,
                CGameCtnReplayStaticSolidArchivePayloadSlice records) {
    return definition.AppendTexCoordStream(dimension, records);
}

void CGameCtnReplayStaticSolidVisualGeometryDefinition::BindTangents(
        CGameCtnReplayStaticSolidArchivePayloadSlice tangents,
        CGameCtnReplayStaticSolidArchivePayloadSlice binormals) {
    definition.BindTangents(tangents, binormals);
}

void CGameCtnReplayStaticSolidVisualGeometryDefinition::BindIndexBuffer(
        u32 indexCount,
        CGameCtnReplayStaticSolidArchivePayloadSlice indices) {
    definition.BindIndexBuffer(indexCount, indices);
}

const CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition &
CGameCtnReplayStaticSolidVisualGeometryDefinition::ArchiveDefinition() const {
    return definition;
}

void StaticSolidArchiveVisualState::StartGeometry(
        CGameCtnReplayStaticSolidVisualGeometryDefinition geometry) {
    activeGeometry = geometry;
}

int StaticSolidArchiveVisualState::
        StartGeometryAndCommit(
                StaticSolidArchiveLoadSession *store,
                CGameCtnReplayStaticSolidVisualGeometryDefinition geometry) {
    StartGeometry(geometry);
    return store == nullptr ||
           store->MutableArchiveGraph()
                   .SurfaceGraph()
                   .AddVisualProviderDefinition(activeGeometry.ArchiveDefinition());
}

int StaticSolidArchiveVisualState::HasActiveGeometry()
        const {
    return activeGeometry.IsReady();
}

CGameCtnReplayStaticSolidVisualGeometryDefinition &
StaticSolidArchiveVisualState::ActiveGeometry() {
    return activeGeometry;
}

const CGameCtnReplayStaticSolidVisualGeometryDefinition &
StaticSolidArchiveVisualState::ActiveGeometry() const {
    return activeGeometry;
}

int StaticSolidArchiveVisualState::
        CommitActiveGeometry(StaticSolidArchiveLoadSession *store)
                const {
    return store == nullptr ||
           store->MutableArchiveGraph()
                   .SurfaceGraph()
                   .UpdateVisualProviderDefinition(
                           activeGeometry.ArchiveDefinition());
}
