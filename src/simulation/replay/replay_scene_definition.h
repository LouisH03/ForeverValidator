#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/resources/catalog_asset_repository.h"
#include "engine/game/game_ctn_block.h"
#include "engine/game/replay_challenge_map_input.h"
struct BlockInfoCatalogEntry;
class CGameCtnBlockInfoPylon;
class CGameCtnBlockInfoClip;

enum class ReplaySceneBlockOrigin {
    Authored,
    AutomaticBase,
};

class ReplaySceneBlockDefinition {
public:
    static std::optional<ReplaySceneBlockDefinition> Authored(
            const CGameCtnReplayMapInputBlock &placement,
            const BlockInfoCatalogEntry &catalogEntry,
            std::string_view sceneCollection,
            CGameCtnBlockInfo &blockInfo);
    static std::optional<ReplaySceneBlockDefinition> AutomaticBase(
            const BlockInfoCatalogEntry &catalogEntry,
            CGameCtnBlockInfo &blockInfo);

    const std::optional<CGameCtnReplayBlockPlacementId> &PlacementId() const;
    ReplaySceneBlockOrigin Origin() const;
    EBlockType Type() const;
    const BlockPlacementState &Placement() const;
    BlockInfoAssetHandle Asset() const;
    CGameCtnBlockInfo *BlockInfo() const;
    const std::optional<u32> &CollectionLandZoneHeight() const;

    bool IsAuthored() const;
    bool IsAutomaticBase() const;
    bool IsTerrainBlock() const;
    bool IsPlacedMobilBlock() const;
    bool NeedsDefaultUnitGeometry() const;
    bool UsesCollectionLandZoneHeight() const;
    bool IsClip() const;

    bool UsesReplacementMaterialRemap() const;
    bool UsesDecorationSkinMaterialRemap() const;
    void SetMaterialRemaps(bool replacement, bool decorationSkin);

    const std::array<CMwNodRef<CSceneMobil>, 4> &ClipSourceMobils() const;
    std::array<CMwNodRef<CSceneMobil>, 4> &MutableClipSourceMobils();
    bool InstallDefaultClipSourceSides(CSceneMobil *mobil);

private:
    std::optional<CGameCtnReplayBlockPlacementId> placementId_;
    ReplaySceneBlockOrigin origin_ = ReplaySceneBlockOrigin::Authored;
    EBlockType type_ = EBlockType::Classic;
    BlockPlacementState placement_;
    BlockInfoAssetHandle asset_;
    CMwNodRef<CGameCtnBlockInfo> blockInfo_;
    std::optional<u32> collectionLandZoneHeight_;
    bool replacementMaterialRemap_ = false;
    bool decorationSkinMaterialRemap_ = false;
    std::array<CMwNodRef<CSceneMobil>, 4> clipSourceMobils_;
};

class ReplaySceneZoneClipDefinition {
public:
    ReplaySceneZoneClipDefinition(
            std::string zoneIdentifier,
            BlockInfoAssetHandle asset,
            CGameCtnBlockInfoClip &blockInfo);

    const std::string &ZoneIdentifier() const;
    BlockInfoAssetHandle Asset() const;
    CGameCtnBlockInfoClip *BlockInfo() const;

private:
    std::string zoneIdentifier_;
    BlockInfoAssetHandle asset_;
    CMwNodRef<CGameCtnBlockInfoClip> blockInfo_;
};

class ReplaySceneConstructionZoneDefinition {
public:
    ReplaySceneConstructionZoneDefinition(
            std::string zoneIdentifier,
            BlockInfoAssetHandle asset,
            CGameCtnBlockInfo &blockInfo);

    const std::string &ZoneIdentifier() const;
    BlockInfoAssetHandle Asset() const;
    CGameCtnBlockInfo *BlockInfo() const;

private:
    std::string zoneIdentifier_;
    BlockInfoAssetHandle asset_;
    CMwNodRef<CGameCtnBlockInfo> blockInfo_;
};

class ReplaySceneZonePylonDefinition {
public:
    ReplaySceneZonePylonDefinition(
            std::string zoneIdentifier,
            BlockInfoAssetHandle asset,
            CGameCtnBlockInfoPylon &blockInfo);

    const std::string &ZoneIdentifier() const;
    BlockInfoAssetHandle Asset() const;
    CGameCtnBlockInfoPylon *BlockInfo() const;

private:
    std::string zoneIdentifier_;
    BlockInfoAssetHandle asset_;
    CMwNodRef<CGameCtnBlockInfoPylon> blockInfo_;
};

class ReplaySceneDefinition {
public:
    void Clear();
    bool Reserve(std::size_t blockCount);
    bool Add(ReplaySceneBlockDefinition block);
    bool SkipAuthoredBlock(CGameCtnReplayBlockPlacementId id);
    bool SetConstructionZones(
            std::vector<ReplaySceneConstructionZoneDefinition> zones);
    bool SetZoneClips(std::vector<ReplaySceneZoneClipDefinition> clips);
    bool SetZonePylons(std::vector<ReplaySceneZonePylonDefinition> pylons);

    std::size_t BlockCount() const;
    const ReplaySceneBlockDefinition *BlockAt(std::size_t index) const;
    ReplaySceneBlockDefinition *MutableBlockAt(std::size_t index);
    const ReplaySceneBlockDefinition *FindAuthoredBlock(
            CGameCtnReplayBlockPlacementId id) const;
    bool IsAuthoredBlockSkipped(
            CGameCtnReplayBlockPlacementId id) const;
    std::size_t SkippedAuthoredBlockCount() const;
    const ReplaySceneBlockDefinition *AutomaticBaseBlock() const;

    const std::vector<ReplaySceneBlockDefinition> &Blocks() const;
    std::vector<ReplaySceneBlockDefinition> &MutableBlocks();
    const std::vector<ReplaySceneConstructionZoneDefinition> &
    ConstructionZones() const;
    CGameCtnBlockInfo *ConstructionZoneBlockInfo(
            std::string_view zoneIdentifier) const;
    const char *ConstructionZoneIdentifier(
            const CGameCtnBlockInfo *blockInfo) const;
    BlockInfoAssetHandle ConstructionZoneAsset(
            std::string_view zoneIdentifier) const;
    const std::vector<ReplaySceneZoneClipDefinition> &ZoneClips() const;
    CGameCtnBlockInfoClip *ZoneClipBlockInfo(
            std::string_view zoneIdentifier) const;
    CGameCtnBlockInfoClip *DefaultClipBlockInfo() const;
    const std::vector<ReplaySceneZonePylonDefinition> &ZonePylons() const;
    CGameCtnBlockInfoPylon *ZonePylonBlockInfo(
            std::string_view zoneIdentifier) const;
    BlockInfoAssetHandle ZonePylonAsset(
            std::string_view zoneIdentifier) const;

    void SetCollection(CatalogCollectionDefinition definition);
    const std::optional<CatalogCollectionDefinition> &Collection() const;
    void SetDecorationSize(CatalogDecorationSizeDefinition definition);
    const std::optional<CatalogDecorationSizeDefinition> &DecorationSize()
            const;
    std::optional<std::string> DefaultBaseIdentifier() const;
    std::optional<std::string> DefaultPylonIdentifier() const;
    CGameCtnBlockInfoPylon *DefaultPylonBlockInfo() const;
    BlockInfoAssetHandle DefaultPylonAsset() const;
    void SetDefaultPylon(
            BlockInfoAssetHandle asset,
            CGameCtnBlockInfoPylon &blockInfo);
    CatalogConstructionZoneKind DefaultZoneKind() const;

    bool AppendAutomaticBase(
            const CGameCtnReplayMapInput &mapInput,
            CatalogAssetRepository &assets);

private:
    std::vector<ReplaySceneBlockDefinition> blocks_;
    std::vector<CGameCtnReplayBlockPlacementId> skippedAuthoredBlocks_;
    std::vector<ReplaySceneConstructionZoneDefinition> constructionZones_;
    std::vector<ReplaySceneZoneClipDefinition> zoneClips_;
    std::vector<ReplaySceneZonePylonDefinition> zonePylons_;
    std::optional<CatalogCollectionDefinition> collection_;
    std::optional<CatalogDecorationSizeDefinition> decorationSize_;
    BlockInfoAssetHandle defaultPylonAsset_;
    CMwNodRef<CGameCtnBlockInfoPylon> defaultPylonBlockInfo_;
};
