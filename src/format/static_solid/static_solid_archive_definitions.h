#ifndef TMNF_STATIC_SOLID_ARCHIVE_DEFINITIONS_H
#define TMNF_STATIC_SOLID_ARCHIVE_DEFINITIONS_H

#include "engine/scene/plug_solid.h"
#include "engine/rendering/plug_material.h"
#include "engine/core/gm_types.h"
#include "engine/core/engine_types.h"
#include "engine/game/material_definition.h"
#include "engine/rendering/plug_tree.h"
#include "format/static_solid/static_solid_archive_identity.h"
#include "format/static_solid/static_solid_archive_payload_slice.h"
#include <vector>
class CGameCtnReplayStaticSolidArchiveMeshPayload {
public:
    static constexpr u32 VertexRecordBytes = 12u;
    static constexpr u32 TriangleRecordBytes = 32u;
    static constexpr u32 OctreeCellRecordBytes = 32u;

    static CGameCtnReplayStaticSolidArchiveMeshPayload Empty();
    static CGameCtnReplayStaticSolidArchiveMeshPayload FromArchiveRecords(
            u32 vertexByteOffset,
            u32 vertexCount,
            u32 triangleByteOffset,
            u32 triangleCount,
            u32 octreeCellByteOffset,
            u32 octreeCellCount);

    int HasMeshRecords() const;
    CGameCtnReplayStaticSolidArchivePayloadSlice Vertices() const;
    CGameCtnReplayStaticSolidArchivePayloadSlice Triangles() const;
    CGameCtnReplayStaticSolidArchivePayloadSlice OctreeCells() const;
    u32 VertexCount() const;
    u32 TriangleCount() const;
    u32 OctreeCellCount() const;

private:
    CGameCtnReplayStaticSolidArchivePayloadSlice vertices =
            CGameCtnReplayStaticSolidArchivePayloadSlice::Empty();
    CGameCtnReplayStaticSolidArchivePayloadSlice triangles =
            CGameCtnReplayStaticSolidArchivePayloadSlice::Empty();
    CGameCtnReplayStaticSolidArchivePayloadSlice octreeCells =
            CGameCtnReplayStaticSolidArchivePayloadSlice::Empty();
};

class CGameCtnReplayStaticSolidArchiveTreeStateDefinition {
public:
    enum class Scope {
        Complete,
        LocalTransform,
    };

    void Install(CGameCtnReplayStaticSolidArchiveNodeIdentity tree,
                 u32 chunkId,
                 Scope scope,
                 const CPlugTree::SFlags &state,
                 u32 hasLocalIso,
                 const GmIso4 *localIso);
    CGameCtnReplayStaticSolidArchiveNodeIdentity Tree() const;
    u32 ChunkId() const;
    int MatchesTree(CGameCtnReplayStaticSolidArchiveNodeIdentity tree) const;
    int HasLocalIso() const;
    const GmIso4 &LocalTransform() const;
    void ApplyTreeState(CPlugTree *tree) const;

private:
    CGameCtnReplayStaticSolidArchiveNodeIdentity tree =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    u32 chunkId = 0u;
    Scope scope = Scope::Complete;
    CPlugTree::SFlags state{};
    u32 hasLocalIso = 0u;
    GmIso4 localIso{};
};

class CGameCtnReplayStaticSolidArchiveSolidPhysicsDefinition {
public:
    void Begin(CGameCtnReplayStaticSolidArchiveNodeIdentity solid,
               u32 chunkId);
    void SetMassCenterAndImpulseInertia(float mass,
                                        const float localCenterOfMass[3],
                                        const float impulseInertia[9]);
    void SetLegacyResponseDefaults();
    void SetResponse(float linearFluidFriction,
                     float physicalResponseCoefA,
                     float physicalResponseCoefB);
    CGameCtnReplayStaticSolidArchiveNodeIdentity Solid() const;
    u32 ChunkId() const;
    int MatchesSolid(CGameCtnReplayStaticSolidArchiveNodeIdentity solid) const;
    void ApplyToSolid(CPlugSolid *solid) const;

private:
    CGameCtnReplayStaticSolidArchiveNodeIdentity solid =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    u32 chunkId = 0u;
    float mass = 0.0f;
    float impulseInertia[9]{};
    float linearFluidFriction = 0.0f;
    float physicalResponseCoefA = 0.0f;
    float physicalResponseCoefB = 0.0f;
    float vehicleContactFeedbackScale = 1.0f;
    float localCenterOfMass[3]{};
};

class CGameCtnReplayStaticSolidArchiveMaterialDefinition {
public:
    void InstallEmbedded(
            CGameCtnReplayStaticSolidArchiveNodeIdentity material,
            MaterialSurfaceDefinition surface);
    void InstallResolved(
            CGameCtnReplayStaticSolidArchiveNodeIdentity material,
            const ResolvedMaterialDefinition &definition);
    CGameCtnReplayStaticSolidArchiveNodeIdentity Material() const;
    int MatchesMaterial(
            CGameCtnReplayStaticSolidArchiveNodeIdentity material) const;
    int MatchesPayload(StaticSolidArchiveId payload) const;
    void ApplyToMaterial(CPlugMaterial *material) const;
    MaterialAssetHandle Asset() const;
    const MaterialSurfaceDefinition &Surface() const;
    const MaterialRenderDefinition &Render() const;
    const MaterialRemapCollection &Remaps() const;

private:
    CGameCtnReplayStaticSolidArchiveNodeIdentity material =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    MaterialAssetHandle asset;
    MaterialSurfaceDefinition surface;
    MaterialRenderDefinition render;
    MaterialRemapCollection remaps;
};

class CGameCtnReplayStaticSolidArchiveSurfaceGeometryDefinition {
public:
    void Install(CGameCtnReplayStaticSolidArchiveNodeIdentity geom,
                 u32 surfType,
                 uint16_t materialId,
                 const GmBoxAligned &boundingBox,
                 CGameCtnReplayStaticSolidArchiveMeshPayload meshPayload);
    CGameCtnReplayStaticSolidArchiveNodeIdentity Geom() const;
    int MatchesGeom(CGameCtnReplayStaticSolidArchiveNodeIdentity geom) const;
    u32 SurfType() const;
    uint16_t MaterialId() const;
    const GmBoxAligned &BoundingBox() const;
    int IsMesh() const;
    CGameCtnReplayStaticSolidArchiveMeshPayload MeshPayload() const;

private:
    CGameCtnReplayStaticSolidArchiveNodeIdentity geom =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    u32 surfType = 0u;
    uint16_t materialId = 0u;
    GmBoxAligned boundingBox{};
    CGameCtnReplayStaticSolidArchiveMeshPayload meshPayload =
            CGameCtnReplayStaticSolidArchiveMeshPayload::Empty();
};

class CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition {
public:
    struct TexCoordStream {
        u32 dimension = 2u;
        CGameCtnReplayStaticSolidArchivePayloadSlice records =
                CGameCtnReplayStaticSolidArchivePayloadSlice::Empty();
    };

    void BeginGeometry(CGameCtnReplayStaticSolidArchiveNodeIdentity visual,
                       u32 serializedFlags,
                       u32 vertexCount,
                       const GmBoxAligned &boundingBox);
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
    CGameCtnReplayStaticSolidArchiveNodeIdentity VisualProvider() const;
    int MatchesVisualProvider(
            CGameCtnReplayStaticSolidArchiveNodeIdentity visual) const;
    u32 SerializedFlags() const;
    u32 VertexCount() const;
    u32 IndexCount() const;
    CGameCtnReplayStaticSolidArchivePayloadSlice VertexRecords() const;
    const std::vector<TexCoordStream> &TexCoordStreams() const;
    CGameCtnReplayStaticSolidArchivePayloadSlice Tangents() const;
    CGameCtnReplayStaticSolidArchivePayloadSlice Binormals() const;
    CGameCtnReplayStaticSolidArchivePayloadSlice Indices() const;
    const GmBoxAligned &BoundingBox() const;

private:
    CGameCtnReplayStaticSolidArchiveNodeIdentity visual =
            CGameCtnReplayStaticSolidArchiveNodeIdentity::Invalid();
    u32 serializedFlags = 0u;
    u32 vertexCount = 0u;
    u32 indexCount = 0u;
    CGameCtnReplayStaticSolidArchivePayloadSlice vertexRecords =
            CGameCtnReplayStaticSolidArchivePayloadSlice::Empty();
    std::vector<TexCoordStream> texCoordStreams;
    CGameCtnReplayStaticSolidArchivePayloadSlice tangents =
            CGameCtnReplayStaticSolidArchivePayloadSlice::Empty();
    CGameCtnReplayStaticSolidArchivePayloadSlice binormals =
            CGameCtnReplayStaticSolidArchivePayloadSlice::Empty();
    CGameCtnReplayStaticSolidArchivePayloadSlice indices =
            CGameCtnReplayStaticSolidArchivePayloadSlice::Empty();
    GmBoxAligned boundingBox{};
};

#endif
