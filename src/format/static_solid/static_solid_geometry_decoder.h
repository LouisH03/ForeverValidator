#pragma once

#include "engine/rendering/plug_visual.h"
#include "engine/core/gm_types.h"
#include "engine/core/cmw_nod.h"
#include "engine/core/engine_types.h"
#include "format/static_solid/static_solid_archive_payload_slice.h"
#include "format/static_solid/static_solid_archive_id.h"
#include <stdint.h>
#include <deque>
#include <memory>
#include <new>
#include <vector>

class StaticSolidArchiveLoadSession;
class StaticSolidArchiveDecodedBytes;
class StaticSolidDecodedPayloads;
class CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition;

enum class StaticSolidMaterialRemap {
    Replacement,
    DecorationSkin,
};

bool DecodeStaticSolidTexCoordStream(
        const uint8_t *bytes,
        u32 recordCount,
        u32 dimension,
        u32 recordStride,
        GxTexCoordSet *destination);

class StaticSolidDecodedPayloads {
public:
    explicit StaticSolidDecodedPayloads(
            StaticSolidArchiveLoadSession &archive);

    StaticSolidArchiveDecodedBytes Slice(
            StaticSolidArchiveId payload,
            CGameCtnReplayStaticSolidArchivePayloadSlice slice) const;

private:
    StaticSolidArchiveLoadSession &store;
};

template <typename T>
class StaticSolidObjects {
public:
    int Allocate(u32 count) {
        try {
            objects.clear();
            objects.resize(count);
        } catch (const std::bad_alloc &) {
            objects.clear();
            return 0;
        }
        return 1;
    }

    void Clear() {
        objects.clear();
    }

    T *At(u32 index) {
        return index < objects.size()
                ? &objects[index]
                : nullptr;
    }

    const T *At(u32 index) const {
        return index < objects.size()
                ? &objects[index]
                : nullptr;
    }

    int HasObjects() const {
        return !objects.empty();
    }

private:
    std::deque<T> objects;
};

template <typename T>
class StaticSolidNodes {
public:
    int Allocate(u32 count) {
        try {
            objects.clear();
            objects.reserve(count);
            for (u32 index = 0u; index < count; ++index) {
                objects.push_back(MakeMwNod<T>());
            }
        } catch (const std::bad_alloc &) {
            objects.clear();
            return 0;
        }
        return 1;
    }

    void Clear() {
        objects.clear();
    }

    T *At(u32 index) {
        return index < objects.size() ? objects[index].Get() : nullptr;
    }

    const T *At(u32 index) const {
        return index < objects.size() ? objects[index].Get() : nullptr;
    }

    int HasObjects() const {
        return !objects.empty();
    }

private:
    std::vector<CMwNodRef<T>> objects;
};

class StaticSolidDecodedVisualGeometry {
public:
    static StaticSolidDecodedVisualGeometry FromArchiveDefinition(
            const CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition &definition,
            const StaticSolidDecodedPayloads
                    &decodedPayloads);

    u32 VertexCount() const;
    u32 TexCoordVertexCount() const;
    u32 IndexCount() const;
    int CopyVerticesToGx(GxVertex *destination) const;
    void CopyIndicesTo(uint16_t *destination) const;
    const GmBoxAligned &BoundingBox() const;
    const std::vector<GxTexCoordSet> &TexCoordSets() const;
    const std::vector<GmVec3> &Tangents() const;
    const std::vector<GmVec3> &Binormals() const;

private:
    void BindBox(const GmBoxAligned &boundingBox);
    void BindVertexStream(u32 serializedFlags,
                          u32 vertexCount,
                          u32 texCoordVertexCount,
                          const uint8_t *vertexRecordBytes,
                          CGameCtnReplayStaticSolidArchivePayloadSlice
                                  vertexRecords);
    void BindIndexBuffer(u32 indexCount,
                         const uint8_t *indexBytes,
                         CGameCtnReplayStaticSolidArchivePayloadSlice indices);

    u32 serializedFlags = 0u;
    u32 vertexCount = 0u;
    u32 texCoordVertexCount = 0u;
    u32 indexCount = 0u;
    const uint8_t *vertexRecordBytes = nullptr;
    CGameCtnReplayStaticSolidArchivePayloadSlice vertexRecords =
            CGameCtnReplayStaticSolidArchivePayloadSlice::Empty();
    const uint8_t *indexBytes = nullptr;
    CGameCtnReplayStaticSolidArchivePayloadSlice indices =
            CGameCtnReplayStaticSolidArchivePayloadSlice::Empty();
    GmBoxAligned boundingBox{};
    std::vector<GxTexCoordSet> texCoordSets;
    std::vector<GmVec3> tangents;
    std::vector<GmVec3> binormals;
};

class StaticSolidArchiveVisual
        : public CPlugVisualIndexedTriangles {
public:
    void ConfigureGeometry(
            StaticSolidDecodedVisualGeometry geometry);

    u32 TexCoordVertexCount() const override;
    void ComputeBoundingBox(unsigned long flags,
                            unsigned long subVisual) override;
    CPlugVisual *Visual();

private:
    void InstallGeometry(
            StaticSolidDecodedVisualGeometry geometry);

    u32 archiveTexCoordVertexCount = 0u;
};
