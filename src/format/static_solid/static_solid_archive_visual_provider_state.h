#ifndef TMNF_STATIC_SOLID_ARCHIVE_VISUAL_PROVIDER_STATE_H
#define TMNF_STATIC_SOLID_ARCHIVE_VISUAL_PROVIDER_STATE_H


#include "engine/rendering/plug_visual.h"
#include "engine/core/gm_types.h"
#include "engine/core/engine_types.h"
#include "format/static_solid/static_scene_archive_loader.h"
#include "format/static_solid/static_solid_archive_payload_slice.h"
#include "format/static_solid/static_solid_archive_id.h"
class CGameCtnReplayStaticSolidVisualVertexFormat {
public:
    static CGameCtnReplayStaticSolidVisualVertexFormat FromSerializedFlags(
            u32 serializedFlags);

    u32 FaceRecordStride() const;
    int CopyArchiveVerticesToGx(const uint8_t *sourceBytes,
                                u32 sourceStride,
                                u32 sourceByteCount,
                                u32 vertexCount,
                                GxVertex *destination) const;

private:
    CGameCtnReplayStaticSolidVisualVertexFormat(bool hasNormal,
                                                bool hasColor);

    bool hasNormal = false;
    bool hasColor = false;
};

class CGameCtnReplayStaticSolidVisualGeometryDefinition {
public:
    static CGameCtnReplayStaticSolidVisualGeometryDefinition FromArchiveHeader(
            StaticSolidArchiveId payload,
            u32 visualNodeIndex,
            u32 serializedFlags,
            u32 vertexCount,
            const GmBoxAligned &boundingBox);

    int IsReady() const;
    u32 VertexCount() const;
    u32 FaceRecordStride() const;
    void BindVertexRecords(
            CGameCtnReplayStaticSolidArchivePayloadSlice vertexRecords);
    bool AppendTexCoordStream(
            u32 dimension,
            CGameCtnReplayStaticSolidArchivePayloadSlice records);
    void BindTangents(
            CGameCtnReplayStaticSolidArchivePayloadSlice tangents,
            CGameCtnReplayStaticSolidArchivePayloadSlice binormals);
    void BindIndexBuffer(u32 indexCount,
                         CGameCtnReplayStaticSolidArchivePayloadSlice indices);
    const CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition &ArchiveDefinition()
            const;

private:
    int ready = 0;
    CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition definition = {};
};

class StaticSolidArchiveVisualState {
public:
    void StartGeometry(CGameCtnReplayStaticSolidVisualGeometryDefinition
                               geometry);
    int StartGeometryAndCommit(
            StaticSolidArchiveLoadSession *store,
            CGameCtnReplayStaticSolidVisualGeometryDefinition geometry);
    int HasActiveGeometry() const;
    CGameCtnReplayStaticSolidVisualGeometryDefinition &ActiveGeometry();
    const CGameCtnReplayStaticSolidVisualGeometryDefinition &ActiveGeometry()
            const;
    int CommitActiveGeometry(StaticSolidArchiveLoadSession *store)
            const;

private:
    CGameCtnReplayStaticSolidVisualGeometryDefinition activeGeometry;
};

#endif
