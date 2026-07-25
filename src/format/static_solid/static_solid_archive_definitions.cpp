#include "format/static_solid/static_solid_archive_definitions.h"
#include <new>
#include "format/archive/gm_wire_conversion.h"
#include "engine/scene/plug_solid.h"
namespace {

void CopyArchiveFloatRows(float *dst, const float *src, size_t count) {
    for (size_t index = 0u; index < count; index++) {
        dst[index] = src[index];
    }
}

}  // namespace

CGameCtnReplayStaticSolidArchiveMeshPayload
CGameCtnReplayStaticSolidArchiveMeshPayload::Empty() {
    return CGameCtnReplayStaticSolidArchiveMeshPayload();
}

CGameCtnReplayStaticSolidArchiveMeshPayload
CGameCtnReplayStaticSolidArchiveMeshPayload::FromArchiveRecords(
        u32 vertexByteOffset,
        u32 vertexCount,
        u32 triangleByteOffset,
        u32 triangleCount,
        u32 octreeCellByteOffset,
        u32 octreeCellCount) {
    CGameCtnReplayStaticSolidArchiveMeshPayload payload;
    payload.vertices = CGameCtnReplayStaticSolidArchivePayloadSlice::FromRecords(
            vertexByteOffset,
            vertexCount,
            CGameCtnReplayStaticSolidArchiveMeshPayload::VertexRecordBytes);
    payload.triangles = CGameCtnReplayStaticSolidArchivePayloadSlice::FromRecords(
            triangleByteOffset,
            triangleCount,
            CGameCtnReplayStaticSolidArchiveMeshPayload::TriangleRecordBytes);
    payload.octreeCells =
            CGameCtnReplayStaticSolidArchivePayloadSlice::FromRecords(
                    octreeCellByteOffset,
                    octreeCellCount,
                    CGameCtnReplayStaticSolidArchiveMeshPayload::
                            OctreeCellRecordBytes);
    return payload;
}

int CGameCtnReplayStaticSolidArchiveMeshPayload::HasMeshRecords() const {
    return vertices.HasRecords() ||
           triangles.HasRecords() ||
           octreeCells.HasRecords();
}

CGameCtnReplayStaticSolidArchivePayloadSlice
CGameCtnReplayStaticSolidArchiveMeshPayload::Vertices() const {
    return vertices;
}

CGameCtnReplayStaticSolidArchivePayloadSlice
CGameCtnReplayStaticSolidArchiveMeshPayload::Triangles() const {
    return triangles;
}

CGameCtnReplayStaticSolidArchivePayloadSlice
CGameCtnReplayStaticSolidArchiveMeshPayload::OctreeCells() const {
    return octreeCells;
}

u32 CGameCtnReplayStaticSolidArchiveMeshPayload::VertexCount() const {
    return vertices.RecordCount();
}

u32 CGameCtnReplayStaticSolidArchiveMeshPayload::TriangleCount() const {
    return triangles.RecordCount();
}

u32 CGameCtnReplayStaticSolidArchiveMeshPayload::OctreeCellCount() const {
    return octreeCells.RecordCount();
}

void CGameCtnReplayStaticSolidArchiveMaterialDefinition::InstallEmbedded(
        CGameCtnReplayStaticSolidArchiveNodeIdentity material,
        MaterialSurfaceDefinition newSurface) {
    *this = CGameCtnReplayStaticSolidArchiveMaterialDefinition{};
    this->material = material;
    surface = newSurface;
}

void CGameCtnReplayStaticSolidArchiveMaterialDefinition::InstallResolved(
        CGameCtnReplayStaticSolidArchiveNodeIdentity material,
        const ResolvedMaterialDefinition &definition) {
    *this = CGameCtnReplayStaticSolidArchiveMaterialDefinition{};
    this->material = material;
    asset = definition.material.asset;
    surface = definition.material.surface;
    render = definition.material.render;
    remaps = definition.remaps;
}

CGameCtnReplayStaticSolidArchiveNodeIdentity
CGameCtnReplayStaticSolidArchiveMaterialDefinition::Material() const {
    return material;
}

int CGameCtnReplayStaticSolidArchiveMaterialDefinition::MatchesMaterial(
        CGameCtnReplayStaticSolidArchiveNodeIdentity material) const {
    return this->material.Matches(material);
}

int CGameCtnReplayStaticSolidArchiveMaterialDefinition::MatchesPayload(
        StaticSolidArchiveId payload) const {
    return material.MatchesPayload(payload);
}

void CGameCtnReplayStaticSolidArchiveMaterialDefinition::ApplyToMaterial(
        CPlugMaterial *material) const {
    if (material != nullptr) {
        surface.ApplyTo(*material);
        material->SetReplayRenderDefinition(render);
    }
}

MaterialAssetHandle
CGameCtnReplayStaticSolidArchiveMaterialDefinition::Asset() const {
    return asset;
}

const MaterialSurfaceDefinition &
CGameCtnReplayStaticSolidArchiveMaterialDefinition::Surface() const {
    return surface;
}

const MaterialRenderDefinition &
CGameCtnReplayStaticSolidArchiveMaterialDefinition::Render() const {
    return render;
}

const MaterialRemapCollection &
CGameCtnReplayStaticSolidArchiveMaterialDefinition::Remaps() const {
    return remaps;
}

void CGameCtnReplayStaticSolidArchiveTreeStateDefinition::Install(
        CGameCtnReplayStaticSolidArchiveNodeIdentity tree,
        u32 newChunkId,
        Scope newScope,
        const CPlugTree::SFlags &newState,
        u32 newHasLocalIso,
        const GmIso4 *newLocalIso) {
    *this = CGameCtnReplayStaticSolidArchiveTreeStateDefinition{};
    this->tree = tree;
    chunkId = newChunkId;
    scope = newScope;
    state = newState;
    hasLocalIso = newHasLocalIso != 0u ? 1u : 0u;
    if (hasLocalIso != 0u && newLocalIso != nullptr) {
        localIso = *newLocalIso;
    }
}

CGameCtnReplayStaticSolidArchiveNodeIdentity
CGameCtnReplayStaticSolidArchiveTreeStateDefinition::Tree() const {
    return tree;
}

u32 CGameCtnReplayStaticSolidArchiveTreeStateDefinition::ChunkId() const {
    return chunkId;
}

int CGameCtnReplayStaticSolidArchiveTreeStateDefinition::MatchesTree(
        CGameCtnReplayStaticSolidArchiveNodeIdentity tree) const {
    return this->tree.Matches(tree);
}

int CGameCtnReplayStaticSolidArchiveTreeStateDefinition::HasLocalIso() const {
    return hasLocalIso != 0u;
}

const GmIso4 &
CGameCtnReplayStaticSolidArchiveTreeStateDefinition::LocalTransform() const {
    return localIso;
}

void CGameCtnReplayStaticSolidArchiveTreeStateDefinition::ApplyTreeState(
        CPlugTree *tree) const {
    if (tree == nullptr) {
        return;
    }

    CPlugTree::SFlags appliedState = state;
    if (scope == Scope::LocalTransform) {
        appliedState = tree->State();
        appliedState.usesLocation = state.usesLocation;
    }

    if (hasLocalIso != 0u) {
        tree->SetLocation(localIso);
    }
    tree->ApplyLoadedState(appliedState);
}

void CGameCtnReplayStaticSolidArchiveSolidPhysicsDefinition::Begin(
        CGameCtnReplayStaticSolidArchiveNodeIdentity solid,
        u32 newChunkId) {
    *this = CGameCtnReplayStaticSolidArchiveSolidPhysicsDefinition{};
    this->solid = solid;
    chunkId = newChunkId;
    vehicleContactFeedbackScale = 1.0f;
}

CGameCtnReplayStaticSolidArchiveNodeIdentity
CGameCtnReplayStaticSolidArchiveSolidPhysicsDefinition::Solid() const {
    return solid;
}

void CGameCtnReplayStaticSolidArchiveSolidPhysicsDefinition::
        SetMassCenterAndImpulseInertia(float newMass,
                                       const float newLocalCenterOfMass[3],
                                       const float newImpulseInertia[9]) {
    mass = newMass;
    if (newLocalCenterOfMass != nullptr) {
        CopyArchiveFloatRows(localCenterOfMass, newLocalCenterOfMass, 3u);
    }
    if (newImpulseInertia != nullptr) {
        CopyArchiveFloatRows(impulseInertia, newImpulseInertia, 9u);
    }
}

void CGameCtnReplayStaticSolidArchiveSolidPhysicsDefinition::
        SetLegacyResponseDefaults() {
    linearFluidFriction = 0.1f;
    physicalResponseCoefA = 0.3f;
    physicalResponseCoefB = 0.3f;
}

void CGameCtnReplayStaticSolidArchiveSolidPhysicsDefinition::SetResponse(
        float newLinearFluidFriction,
        float newPhysicalResponseCoefA,
        float newPhysicalResponseCoefB) {
    linearFluidFriction = newLinearFluidFriction;
    physicalResponseCoefA = newPhysicalResponseCoefA;
    physicalResponseCoefB = newPhysicalResponseCoefB;
}

u32 CGameCtnReplayStaticSolidArchiveSolidPhysicsDefinition::ChunkId() const {
    return chunkId;
}

int CGameCtnReplayStaticSolidArchiveSolidPhysicsDefinition::MatchesSolid(
        CGameCtnReplayStaticSolidArchiveNodeIdentity solid) const {
    return this->solid.Matches(solid);
}

void CGameCtnReplayStaticSolidArchiveSolidPhysicsDefinition::ApplyToSolid(
        CPlugSolid *solid) const {
    if (solid == nullptr) {
        return;
    }
    CPlugPhysicalParameters parameters;
    parameters.mass = mass;
    parameters.impulseInertia = GmWire::DecodeMat3(impulseInertia);
    parameters.linearFluidFriction = linearFluidFriction;
    parameters.physicalResponseCoefA = physicalResponseCoefA;
    parameters.physicalResponseCoefB = physicalResponseCoefB;
    parameters.vehicleContactFeedbackScale = vehicleContactFeedbackScale;
    parameters.localCenterOfMass = {
        localCenterOfMass[0],
        localCenterOfMass[1],
        localCenterOfMass[2],
    };
    solid->Physical().Configure(parameters);
}

void CGameCtnReplayStaticSolidArchiveSurfaceGeometryDefinition::Install(
        CGameCtnReplayStaticSolidArchiveNodeIdentity geom,
        u32 newSurfType,
        uint16_t newMaterialId,
        const GmBoxAligned &newBoundingBox,
        CGameCtnReplayStaticSolidArchiveMeshPayload newMeshPayload) {
    *this = CGameCtnReplayStaticSolidArchiveSurfaceGeometryDefinition{};
    this->geom = geom;
    surfType = newSurfType;
    materialId = newMaterialId;
    boundingBox = newBoundingBox;
    meshPayload = newMeshPayload;
}

CGameCtnReplayStaticSolidArchiveNodeIdentity
CGameCtnReplayStaticSolidArchiveSurfaceGeometryDefinition::Geom() const {
    return geom;
}

int CGameCtnReplayStaticSolidArchiveSurfaceGeometryDefinition::MatchesGeom(
        CGameCtnReplayStaticSolidArchiveNodeIdentity geom) const {
    return this->geom.Matches(geom);
}

u32 CGameCtnReplayStaticSolidArchiveSurfaceGeometryDefinition::SurfType() const {
    return surfType;
}

uint16_t CGameCtnReplayStaticSolidArchiveSurfaceGeometryDefinition::MaterialId()
        const {
    return materialId;
}

const GmBoxAligned &
CGameCtnReplayStaticSolidArchiveSurfaceGeometryDefinition::BoundingBox()
        const {
    return boundingBox;
}

int CGameCtnReplayStaticSolidArchiveSurfaceGeometryDefinition::IsMesh() const {
    return surfType == static_cast<u32>(GmSurf::EGmSurfType::Mesh);
}

CGameCtnReplayStaticSolidArchiveMeshPayload
CGameCtnReplayStaticSolidArchiveSurfaceGeometryDefinition::MeshPayload() const {
    return meshPayload;
}

void CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition::BeginGeometry(
        CGameCtnReplayStaticSolidArchiveNodeIdentity visual,
        u32 newSerializedFlags,
        u32 newVertexCount,
        const GmBoxAligned &newBoundingBox) {
    *this = CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition{};
    this->visual = visual;
    serializedFlags = newSerializedFlags;
    vertexCount = newVertexCount;
    boundingBox = newBoundingBox;
}

CGameCtnReplayStaticSolidArchiveNodeIdentity
CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition::VisualProvider() const {
    return visual;
}

void CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition::BindVertexRecords(
        CGameCtnReplayStaticSolidArchivePayloadSlice newVertexRecords) {
    vertexRecords = newVertexRecords;
}

bool CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition::
        AppendTexCoordStream(
                u32 dimension,
                CGameCtnReplayStaticSolidArchivePayloadSlice records) {
    try {
        texCoordStreams.push_back({dimension, records});
        return true;
    } catch (const std::bad_alloc &) {
        return false;
    }
}

void CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition::BindTangents(
        CGameCtnReplayStaticSolidArchivePayloadSlice newTangents,
        CGameCtnReplayStaticSolidArchivePayloadSlice newBinormals) {
    tangents = newTangents;
    binormals = newBinormals;
}

void CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition::BindIndexBuffer(
        u32 newIndexCount,
        CGameCtnReplayStaticSolidArchivePayloadSlice newIndices) {
    indexCount = newIndexCount;
    indices = newIndices;
}

int CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition::
        MatchesVisualProvider(
                CGameCtnReplayStaticSolidArchiveNodeIdentity visual) const {
    return this->visual.Matches(visual);
}

u32 CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition::SerializedFlags()
        const {
    return serializedFlags;
}

u32 CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition::VertexCount() const {
    return vertexCount;
}

u32 CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition::IndexCount() const {
    return indexCount;
}

CGameCtnReplayStaticSolidArchivePayloadSlice
CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition::VertexRecords()
        const {
    return vertexRecords;
}

const std::vector<
        CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition::
                TexCoordStream> &
CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition::TexCoordStreams()
        const {
    return texCoordStreams;
}

CGameCtnReplayStaticSolidArchivePayloadSlice
CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition::Tangents() const {
    return tangents;
}

CGameCtnReplayStaticSolidArchivePayloadSlice
CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition::Binormals() const {
    return binormals;
}

CGameCtnReplayStaticSolidArchivePayloadSlice
CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition::Indices()
        const {
    return indices;
}

const GmBoxAligned &
CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition::BoundingBox() const {
    return boundingBox;
}
