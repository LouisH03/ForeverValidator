#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/resources/catalog_asset_handle.h"
#include "engine/core/gm_types.h"
class BlockInfoCatalog;
class CGameCtnBlockInfo;
struct CGameCtnReplayMapInput;
class CSceneMobil;

enum class CatalogConstructionZoneKind {
    None,
    Flat,
    Frontier,
    Other,
};

struct CatalogWaterZoneDefinition {
    std::string identifier;
    std::string blockInfoIdentifier;
    std::optional<std::string> clipIdentifier;
    std::optional<std::string> selectedClipDescriptorPath;
    std::optional<std::string> pylonIdentifier;
    std::optional<std::string> selectedPylonDescriptorPath;
    std::optional<std::string> frontierParentIdentifier;
    std::optional<std::string> frontierChildIdentifier;
    CatalogConstructionZoneKind kind =
            CatalogConstructionZoneKind::None;
    u32 height = 0u;
    u32 depth = 0u;
    bool oldZone = false;
    bool hasWater = false;
};

struct CatalogCollectionWaterDefinition {
    float surfaceOffset = -1000.0f;
    float secondaryOffset = -1000.0f;
    float renderCullOffset = -1000.0f;
    bool defaultWater = true;
    bool geometryWaterPlanes = false;
    bool defaultZoneHasWater = false;
    std::vector<CatalogWaterZoneDefinition> zones;
};

struct CatalogSurfaceReplacementDefinition {
    std::string sourceSurfaceId;
    std::string targetSurfaceId;
};

struct CatalogCollectionDefinition {
    std::string name;
    std::optional<std::string> defaultBaseIdentifier;
    std::optional<std::string> defaultPylonIdentifier;
    CatalogConstructionZoneKind defaultZoneKind =
            CatalogConstructionZoneKind::None;
    float squareSize = 32.0f;
    float squareHeight = 8.0f;
    std::vector<CatalogSurfaceReplacementDefinition> surfaceReplacements;
    std::optional<CatalogCollectionWaterDefinition> water;
};

struct CatalogDecorationSizeDefinition {
    GmNat3 mapSize{};
    u32 defaultZoneHeight = 0u;
    bool decorationSizeEmbedded = false;
    std::string selectedDecorationPath;
    std::string selectedDecorationSizePath;
    std::string selectedBaseScenePath;
};

class CatalogAssetRepository {
public:
    virtual ~CatalogAssetRepository() = default;

    virtual const BlockInfoCatalog *Catalog() = 0;
    virtual CGameCtnBlockInfo *BlockInfo(BlockInfoAssetHandle asset) = 0;
    virtual CSceneMobil *Mobil(BlockInfoAssetHandle asset,
                              bool ground,
                              u32 variant) = 0;
    virtual std::optional<std::string> FirstGroundSurface(
            BlockInfoAssetHandle asset) = 0;
    virtual std::optional<CatalogCollectionDefinition> Collection(
            std::string_view name) = 0;
    virtual std::optional<CatalogDecorationSizeDefinition> DecorationSize(
            const CGameCtnReplayMapInput &mapInput) = 0;
    virtual bool HasSurfaceReplacement(std::string_view collection,
                                       std::string_view sourceId,
                                       std::string_view targetId) = 0;
};
