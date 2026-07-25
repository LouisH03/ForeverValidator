#include "format/static_solid/static_solid_geometry_decoder.h"
#include <stdint.h>
#include <utility>

#include "format/static_solid/static_scene_archive_loader.h"
#include "format/static_solid/static_solid_archive_visual_provider_state.h"
#include "format/archive/archive_binary32.h"
namespace {

float ReadF32(const uint8_t *bytes) {
    return TmnfArchiveBinary32::ReadLittleEndian(bytes);
}

GmVec3 UnpackSnorm10(const uint8_t *bytes) {
    const u32 packed =
            TmnfArchiveBinary32::ReadU32LittleEndian(bytes);
    const int sx = (static_cast<int32_t>(packed << 22u)) >> 22;
    const int sy = (static_cast<int32_t>(packed << 12u)) >> 22;
    const int sz = (static_cast<int32_t>(packed << 2u)) >> 22;
    return {
            static_cast<float>(static_cast<double>(sx) / 511.0),
            static_cast<float>(static_cast<double>(sy) / 511.0),
            static_cast<float>(static_cast<double>(sz) / 511.0)};
}

}  // namespace

bool DecodeStaticSolidTexCoordStream(
        const uint8_t *bytes,
        u32 recordCount,
        u32 dimension,
        u32 recordStride,
        GxTexCoordSet *destination) {
    if (bytes == nullptr || destination == nullptr ||
        dimension < 2u || dimension > 4u ||
        recordStride != dimension * sizeof(float)) {
        return false;
    }
    GxTexCoordSet decoded = GxTexCoordSet::Create(
            static_cast<GxTexCoordDimension>(dimension - 2u),
            recordCount);
    for (u32 vertex = 0u; vertex < recordCount; ++vertex) {
        const uint8_t *source =
                bytes + static_cast<size_t>(vertex) * recordStride;
        GxTexCoord4 coordinate{
                ReadF32(source),
                ReadF32(source + 4u),
                0.0f,
                1.0f};
        if (dimension >= 3u) {
            coordinate.w = ReadF32(source + 8u);
        }
        if (dimension >= 4u) {
            coordinate.q = ReadF32(source + 12u);
        }
        decoded.SetCoordinate(vertex, coordinate);
    }
    *destination = std::move(decoded);
    return true;
}

StaticSolidDecodedPayloads::
        StaticSolidDecodedPayloads(
                StaticSolidArchiveLoadSession &archive)
        : store(archive) {}

StaticSolidArchiveDecodedBytes
StaticSolidDecodedPayloads::Slice(
        StaticSolidArchiveId payload,
        CGameCtnReplayStaticSolidArchivePayloadSlice slice) const {
    if (!slice.HasRecords()) {
        return StaticSolidArchiveDecodedBytes::Empty();
    }
    if (!payload.IsValid()) {
        return StaticSolidArchiveDecodedBytes::Missing();
    }
    return store.DecodedPayloadBytes(payload,
                                     slice.ByteOffset(),
                                     slice.ByteCount());
}

StaticSolidDecodedVisualGeometry
StaticSolidDecodedVisualGeometry::FromArchiveDefinition(
        const CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition &definition,
        const StaticSolidDecodedPayloads
                &decodedPayloads) {
    StaticSolidDecodedVisualGeometry geometry;
    geometry.BindBox(definition.BoundingBox());

    const CGameCtnReplayStaticSolidArchivePayloadSlice vertexRecords =
            definition.VertexRecords();
    const CGameCtnReplayStaticSolidArchivePayloadSlice indexRecords =
            definition.Indices();
    const StaticSolidArchiveDecodedBytes vertexBytes =
            decodedPayloads.Slice(
            definition.VisualProvider().Payload(),
            vertexRecords);
    const StaticSolidArchiveDecodedBytes indexBytes =
            decodedPayloads.Slice(
            definition.VisualProvider().Payload(),
            indexRecords);
    geometry.BindVertexStream(definition.SerializedFlags(),
                              definition.VertexCount(),
                              definition.VertexCount(),
                              vertexBytes.IsAvailable()
                                      ? vertexBytes.Bytes()
                                      : nullptr,
                              vertexRecords);
    geometry.BindIndexBuffer(definition.IndexCount(),
                             indexBytes.IsAvailable()
                                     ? indexBytes.Bytes()
                                     : nullptr,
                             indexRecords);

    for (const auto &stream : definition.TexCoordStreams()) {
        const StaticSolidArchiveDecodedBytes bytes =
                decodedPayloads.Slice(
                        definition.VisualProvider().Payload(),
                        stream.records);
        if (!bytes.IsAvailable()) {
            continue;
        }
        GxTexCoordSet set;
        if (DecodeStaticSolidTexCoordStream(
                    bytes.Bytes(),
                    definition.VertexCount(),
                    stream.dimension,
                    stream.records.RecordStride(),
                    &set)) {
            geometry.texCoordSets.push_back(std::move(set));
        }
    }

    const auto decodePackedVectors =
            [&](CGameCtnReplayStaticSolidArchivePayloadSlice records,
                std::vector<GmVec3> &destination) {
        const StaticSolidArchiveDecodedBytes bytes =
                decodedPayloads.Slice(
                        definition.VisualProvider().Payload(), records);
        if (!bytes.IsAvailable() || records.RecordStride() != 4u) {
            return;
        }
        destination.reserve(records.RecordCount());
        for (u32 index = 0u; index < records.RecordCount(); ++index) {
            destination.push_back(UnpackSnorm10(
                    bytes.Bytes() + static_cast<size_t>(index) * 4u));
        }
    };
    decodePackedVectors(definition.Tangents(), geometry.tangents);
    decodePackedVectors(definition.Binormals(), geometry.binormals);
    return geometry;
}

void StaticSolidDecodedVisualGeometry::BindBox(
        const GmBoxAligned &newBoundingBox) {
    boundingBox = newBoundingBox;
}

void StaticSolidDecodedVisualGeometry::BindVertexStream(
        u32 newSerializedFlags,
        u32 newVertexCount,
        u32 newTexCoordVertexCount,
        const uint8_t *newVertexRecordBytes,
        CGameCtnReplayStaticSolidArchivePayloadSlice newVertexRecords) {
    serializedFlags = newSerializedFlags;
    vertexCount = newVertexCount;
    texCoordVertexCount = newTexCoordVertexCount;
    vertexRecordBytes = newVertexRecordBytes;
    vertexRecords = newVertexRecords;
}

void StaticSolidDecodedVisualGeometry::BindIndexBuffer(
        u32 newIndexCount,
        const uint8_t *newIndexBytes,
        CGameCtnReplayStaticSolidArchivePayloadSlice newIndices) {
    indexCount = newIndexCount;
    indexBytes = newIndexBytes;
    indices = newIndices;
}

u32 StaticSolidDecodedVisualGeometry::VertexCount() const {
    return vertexCount;
}

u32 StaticSolidDecodedVisualGeometry::TexCoordVertexCount()
        const {
    return texCoordVertexCount;
}

u32 StaticSolidDecodedVisualGeometry::IndexCount() const {
    return indexCount;
}

int StaticSolidDecodedVisualGeometry::CopyVerticesToGx(
        GxVertex *destination) const {
    return CGameCtnReplayStaticSolidVisualVertexFormat::FromSerializedFlags(
            serializedFlags)
            .CopyArchiveVerticesToGx(vertexRecordBytes,
                                     vertexRecords.RecordStride(),
                                     vertexRecords.ByteCount(),
                                     vertexCount,
                                     destination);
}

void StaticSolidDecodedVisualGeometry::CopyIndicesTo(
        uint16_t *destination) const {
    if (destination == nullptr ||
        indexBytes == nullptr ||
        !indices.HasRecords()) {
        return;
    }
    if (indices.ByteCount() <
        static_cast<size_t>(indexCount) * sizeof(uint16_t)) {
        return;
    }
    for (u32 index = 0u; index < indexCount; index++) {
        const uint8_t *source =
                indexBytes + static_cast<size_t>(index) * sizeof(uint16_t);
        destination[index] = static_cast<uint16_t>(
                static_cast<uint16_t>(source[0]) |
                static_cast<uint16_t>(source[1]) << 8u);
    }
}

const GmBoxAligned &
StaticSolidDecodedVisualGeometry::BoundingBox() const {
    return boundingBox;
}

const std::vector<GxTexCoordSet> &
StaticSolidDecodedVisualGeometry::TexCoordSets() const {
    return texCoordSets;
}

const std::vector<GmVec3> &
StaticSolidDecodedVisualGeometry::Tangents() const {
    return tangents;
}

const std::vector<GmVec3> &
StaticSolidDecodedVisualGeometry::Binormals() const {
    return binormals;
}

void StaticSolidArchiveVisual::InstallGeometry(
        StaticSolidDecodedVisualGeometry newGeometry) {
    std::vector<GxVertex> ownedVertices(newGeometry.VertexCount());
    std::vector<uint16_t> ownedIndices(newGeometry.IndexCount());
    if (!ownedVertices.empty() &&
        !newGeometry.CopyVerticesToGx(ownedVertices.data())) {
        ownedVertices.clear();
    }
    if (!ownedIndices.empty()) {
        newGeometry.CopyIndicesTo(ownedIndices.data());
    }
    SetOwnedGeometry(std::move(ownedVertices), std::move(ownedIndices));
    RemoveTexCoordSetAll();
    for (const GxTexCoordSet &source : newGeometry.TexCoordSets()) {
        GxTexCoordSet copy = source;
        AddTexCoordSet(copy);
    }
    tangents = newGeometry.Tangents();
    binormals = newGeometry.Binormals();
    archiveTexCoordVertexCount = newGeometry.TexCoordVertexCount();
    SetBoundingBox(newGeometry.BoundingBox());
}

void StaticSolidArchiveVisual::ConfigureGeometry(
        StaticSolidDecodedVisualGeometry newGeometry) {
    InstallGeometry(newGeometry);
}

u32 StaticSolidArchiveVisual::TexCoordVertexCount() const {
    return archiveTexCoordVertexCount;
}

void StaticSolidArchiveVisual::ComputeBoundingBox(
        unsigned long flags,
        unsigned long subVisual) {
    (void)flags;
    (void)subVisual;
}

CPlugVisual *
StaticSolidArchiveVisual::Visual() {
    return this;
}
