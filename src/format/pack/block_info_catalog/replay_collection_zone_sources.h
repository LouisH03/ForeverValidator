#ifndef TMNF_REPLAY_COLLECTION_ZONE_SOURCES_H
#define TMNF_REPLAY_COLLECTION_ZONE_SOURCES_H

#include "engine/core/engine_types.h"
#include <stddef.h>
#include <optional>
#include <string>
#include <vector>

#include "format/archive/archive_node_reference.h"
#include "engine/resources/catalog_asset_repository.h"
#include "format/replay/replay_static_descriptor_limits.h"
class BlockInfoDescriptorExternalRefs;
struct CPlugFilePack;
class BlockInfoCatalog;
class ReplayCollectionWaterChunkTestAccess;

enum class CGameCtnReplayConstructionZoneKind : u32 {
    None = 0u,
    Flat = 1u,
    Frontier = 2u,
    Other = 3u
};

enum class CGameCtnReplayZoneDescriptorRole : u32 {
    Flat = 0u,
    Clip = 1u,
    Road = 2u,
    Pylon = 3u,
};

class CGameCtnReplayCollectionZoneSourceInfo {
public:
    void Clear();
    int Configure(ArchiveNodeReference sourceNode,
                  CGameCtnReplayConstructionZoneKind kind,
                  u32 loadable,
                  const char *plainPath);

    int IsConstructionZone() const;
    int IsFlat() const;
    int IsFrontier() const;
    int IdentifierFromPlainPath(char *out, size_t outSize) const;

    ArchiveNodeReference SourceNode() const;
    CGameCtnReplayConstructionZoneKind Kind() const;
    u32 IsLoadable() const;
    const char *PlainPath() const;

private:
    ArchiveNodeReference sourceNode =
            ArchiveNodeReference::Invalid();
    CGameCtnReplayConstructionZoneKind kind =
            CGameCtnReplayConstructionZoneKind::None;
    u32 loadable = 0u;
    char plainPath[CGameCtnReplayStaticDescriptorPathCapacity]{};
};

class CGameCtnReplayZoneDescriptorSourceSet {
public:
    void Clear();
    char *ZoneIdNameArchiveBuffer();
    size_t ZoneIdNameArchiveBufferSize() const;
    const char *ZoneIdName() const;
    char *FrontierParentIdNameArchiveBuffer();
    char *FrontierChildIdNameArchiveBuffer();
    size_t FrontierIdNameArchiveBufferSize() const;
    const char *FrontierParentIdName() const;
    const char *FrontierChildIdName() const;
    char *DescriptorPathArchiveBuffer(u32 slot);
    size_t DescriptorPathArchiveBufferSize() const;
    int NoteDescriptorPathIfPresent(u32 slot);
    const char *FirstDescriptorPath() const;
    const char *DescriptorPath(
            CGameCtnReplayZoneDescriptorRole role) const;
    void SetCollectionLandZoneHeight(u32 height);
    const std::optional<u32> &CollectionLandZoneHeight() const;
    void SetZoneHeight(u32 height);
    u32 ZoneHeight() const;
    void SetZoneDepth(u32 depth);
    u32 ZoneDepth() const;
    void SetOldZone(bool value);
    bool IsOldZone() const;
    void SetHasWater(bool value);
    bool HasWater() const;

private:
    static constexpr u32 SourcePathCount = 4u;

    u32 parsedSourceCount = 0u;
    char zoneIdName[64]{};
    char frontierParentIdName[64]{};
    char frontierChildIdName[64]{};
    char descriptorPath[SourcePathCount]
            [CGameCtnReplayStaticDescriptorPathCapacity]{};
    std::optional<u32> collectionLandZoneHeight;
    u32 zoneHeight = 0u;
    u32 zoneDepth = 0u;
    bool oldZone = false;
    bool hasWater = false;
};

class CGameCtnReplayCollectionZoneSourceRef {
public:
    int Configure(const CGameCtnReplayCollectionZoneSourceInfo &source);
    int IsFlat() const;
    int IsFrontier() const;
    int IdentifierFromPlainPath(char *out, size_t outSize) const;
    int ParseZoneDescriptorSources(const unsigned char *bytes,
                                   u32 byteCount,
                                   const CPlugFilePack *installedPack);

    ArchiveNodeReference SourceNode() const;
    CGameCtnReplayConstructionZoneKind Kind() const;
    u32 IsLoadable() const;
    const char *PlainPath() const;
    const char *FirstDescriptorPath() const;
    const char *DescriptorPath(
            CGameCtnReplayZoneDescriptorRole role) const;
    bool HasParsedDescriptorSources() const;
    const std::optional<u32> &CollectionLandZoneHeight() const;
    u32 ZoneHeight() const;
    u32 ZoneDepth() const;
    bool IsOldZone() const;
    bool HasWater() const;
    const char *ZoneIdName() const;
    const char *FrontierParentIdName() const;
    const char *FrontierChildIdName() const;

private:
    CGameCtnReplayCollectionZoneSourceInfo source;
    CGameCtnReplayZoneDescriptorSourceSet descriptorSources;
    bool hasParsedDescriptorSources = false;
};

class CGameCtnReplayCollectionZoneSources {
public:
    void Clear();
    int CopyFrom(const CGameCtnReplayCollectionZoneSources &source);
    int LoadFromCatalogCollection(
            const CPlugFilePack *installedPack,
            const char *collectionName);
    std::optional<std::string> DefaultBaseIdentifier() const;
    std::optional<std::string> DefaultPylonIdentifier() const;
    std::optional<std::string> DefaultDescriptorPath(
            CGameCtnReplayZoneDescriptorRole role) const;
    CGameCtnReplayConstructionZoneKind DefaultZoneKind() const;
    float SquareSize() const;
    float SquareHeight() const;
    bool HasWaterDefinition() const;
    int BuildWaterDefinition(
            const CPlugFilePack *installedPack,
            const char *collectionName,
            CatalogCollectionWaterDefinition *out) const;
    int ApplyCollectionLandZoneHeights(
            const CPlugFilePack *installedPack,
            const char *collectionName,
            BlockInfoCatalog *catalog) const;

private:
    friend class ReplayCollectionWaterChunkTestAccess;

    std::vector<CGameCtnReplayCollectionZoneSourceRef> refs;
    u32 externalRefCount = 0u;
    u32 flatExternalRefCount = 0u;
    u32 frontierExternalRefCount = 0u;
    u32 loadableExternalRefCount = 0u;
    u32 nonLoadableExternalRefCount = 0u;
    u32 bufferArchiveTag = 0u;
    u32 bufferRefCount = 0u;
    u32 bufferConstructionRefCount = 0u;
    u32 bufferFlatRefCount = 0u;
    u32 bufferFrontierRefCount = 0u;
    ArchiveNodeReference defaultZoneNode =
            ArchiveNodeReference::Invalid();
    CGameCtnReplayConstructionZoneKind defaultZoneKind =
            CGameCtnReplayConstructionZoneKind::None;
    float squareSize = 0.0f;
    float squareHeight = 0.0f;
    float waterSurfaceOffset = -1000.0f;
    float waterSecondaryOffset = -1000.0f;
    float waterRenderCullOffset = -1000.0f;
    bool defaultWater = true;
    bool geometryWaterPlanes = false;
    bool hasWaterDefinition = false;
    std::string defaultBaseIdentifier;
    std::string defaultPylonIdentifier;

    int AppendRef(const CGameCtnReplayCollectionZoneSourceInfo &source);
    int ParseZoneBufferRefsChunk(
            const unsigned char *bytes,
            u32 byteCount,
            const BlockInfoDescriptorExternalRefs &externalRefs,
            u32 *nextOffsetOut,
            u32 *cmwIdModeOut,
            u32 *hasCmwIdModeOut);
    int ParseWaterDefinitionChunks(
            const unsigned char *bytes,
            u32 byteCount,
            u32 bodyOffset,
            u32 cmwIdMode,
            u32 hasCmwIdMode);
    int LoadZoneDescriptors(const CPlugFilePack *installedPack);
    int ResolveDefaultBaseIdentifier(const CPlugFilePack *installedPack);
    int ResolveDefaultPylonIdentifier(
            const CPlugFilePack *installedPack,
            const char *collectionName);
    int CountExternalConstructionZones(
            const BlockInfoDescriptorExternalRefs &externalRefs);
};

#endif
