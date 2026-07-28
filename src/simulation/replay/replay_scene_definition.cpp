#include "simulation/replay/replay_scene_definition.h"
#include <utility>

#include "engine/resources/block_info_catalog.h"
#include "engine/game/game_ctn_block_info.h"
#include <new>
namespace {

}  // namespace

std::optional<ReplaySceneBlockDefinition>
ReplaySceneBlockDefinition::Authored(
        const CGameCtnReplayMapInputBlock &placement,
        const BlockInfoCatalogEntry &catalogEntry,
        std::string_view sceneCollection,
        CGameCtnBlockInfo &blockInfo) {
    if (!catalogEntry.asset.IsValid()) {
        return std::nullopt;
    }
    ReplaySceneBlockDefinition definition;
    definition.placementId_ = placement.Id();
    definition.origin_ = ReplaySceneBlockOrigin::Authored;
    definition.type_ = catalogEntry.blockType;
    definition.placement_ = placement.Placement();
    definition.asset_ = catalogEntry.asset;
    definition.blockInfo_.MwSetNod(&blockInfo);
    if (catalogEntry.collection == sceneCollection) {
        definition.collectionLandZoneHeight_ =
                catalogEntry.collectionLandZoneHeight;
    }
    return definition;
}

std::optional<ReplaySceneBlockDefinition>
ReplaySceneBlockDefinition::AutomaticBase(
        const BlockInfoCatalogEntry &catalogEntry,
        CGameCtnBlockInfo &blockInfo) {
    if (!catalogEntry.asset.IsValid()) {
        return std::nullopt;
    }
    ReplaySceneBlockDefinition definition;
    definition.origin_ = ReplaySceneBlockOrigin::AutomaticBase;
    definition.type_ = catalogEntry.blockType;
    definition.placement_ = BlockPlacementState::AutomaticBase();
    definition.asset_ = catalogEntry.asset;
    definition.blockInfo_.MwSetNod(&blockInfo);
    return definition;
}

const std::optional<CGameCtnReplayBlockPlacementId> &
ReplaySceneBlockDefinition::PlacementId() const {
    return placementId_;
}

ReplaySceneBlockOrigin ReplaySceneBlockDefinition::Origin() const {
    return origin_;
}

EBlockType ReplaySceneBlockDefinition::Type() const {
    return type_;
}

const BlockPlacementState &ReplaySceneBlockDefinition::Placement() const {
    return placement_;
}

BlockInfoAssetHandle ReplaySceneBlockDefinition::Asset() const {
    return asset_;
}

CGameCtnBlockInfo *ReplaySceneBlockDefinition::BlockInfo() const {
    return blockInfo_.Get();
}

const std::optional<u32> &
ReplaySceneBlockDefinition::CollectionLandZoneHeight() const {
    return collectionLandZoneHeight_;
}

bool ReplaySceneBlockDefinition::IsAuthored() const {
    return origin_ == ReplaySceneBlockOrigin::Authored &&
           placementId_.has_value();
}

bool ReplaySceneBlockDefinition::IsAutomaticBase() const {
    return origin_ == ReplaySceneBlockOrigin::AutomaticBase;
}

bool ReplaySceneBlockDefinition::IsTerrainBlock() const {
    return type_ <= EBlockType::Frontier;
}

bool ReplaySceneBlockDefinition::IsPlacedMobilBlock() const {
    return !IsTerrainBlock();
}

bool ReplaySceneBlockDefinition::NeedsDefaultUnitGeometry() const {
    return type_ == EBlockType::RectAsym;
}

bool ReplaySceneBlockDefinition::UsesCollectionLandZoneHeight() const {
    return type_ == EBlockType::Frontier &&
           collectionLandZoneHeight_.has_value();
}

bool ReplaySceneBlockDefinition::IsClip() const {
    return type_ == EBlockType::Clip;
}

bool ReplaySceneBlockDefinition::UsesReplacementMaterialRemap() const {
    return replacementMaterialRemap_;
}

bool ReplaySceneBlockDefinition::UsesDecorationSkinMaterialRemap() const {
    return decorationSkinMaterialRemap_;
}

void ReplaySceneBlockDefinition::SetMaterialRemaps(
        bool replacement,
        bool decorationSkin) {
    replacementMaterialRemap_ = replacement;
    decorationSkinMaterialRemap_ = decorationSkin;
}

const std::array<CMwNodRef<CSceneMobil>, 4> &
ReplaySceneBlockDefinition::ClipSourceMobils() const {
    return clipSourceMobils_;
}

std::array<CMwNodRef<CSceneMobil>, 4> &
ReplaySceneBlockDefinition::MutableClipSourceMobils() {
    return clipSourceMobils_;
}

bool ReplaySceneBlockDefinition::InstallDefaultClipSourceSides(
        CSceneMobil *mobil) {
    if (!((mobil) != nullptr && (mobil)->StaticSolidAsset().IsSpecified())) {
        return true;
    }
    for (CMwNodRef<CSceneMobil> &side : clipSourceMobils_) {
        side.MwSetNod(mobil);
    }
    return true;
}

ReplaySceneZoneClipDefinition::ReplaySceneZoneClipDefinition(
        std::string zoneIdentifier,
        BlockInfoAssetHandle asset,
        CGameCtnBlockInfoClip &blockInfo)
        : zoneIdentifier_(std::move(zoneIdentifier)),
          asset_(asset),
          blockInfo_(&blockInfo) {}

const std::string &ReplaySceneZoneClipDefinition::ZoneIdentifier() const {
    return zoneIdentifier_;
}

BlockInfoAssetHandle ReplaySceneZoneClipDefinition::Asset() const {
    return asset_;
}

CGameCtnBlockInfoClip *ReplaySceneZoneClipDefinition::BlockInfo() const {
    return blockInfo_.Get();
}

ReplaySceneConstructionZoneDefinition::
ReplaySceneConstructionZoneDefinition(
        std::string zoneIdentifier,
        BlockInfoAssetHandle asset,
        CGameCtnBlockInfo &blockInfo)
        : zoneIdentifier_(std::move(zoneIdentifier)),
          asset_(asset),
          blockInfo_(&blockInfo) {}

const std::string &
ReplaySceneConstructionZoneDefinition::ZoneIdentifier() const {
    return zoneIdentifier_;
}

BlockInfoAssetHandle ReplaySceneConstructionZoneDefinition::Asset() const {
    return asset_;
}

CGameCtnBlockInfo *
ReplaySceneConstructionZoneDefinition::BlockInfo() const {
    return blockInfo_.Get();
}

ReplaySceneZonePylonDefinition::ReplaySceneZonePylonDefinition(
        std::string zoneIdentifier,
        BlockInfoAssetHandle asset,
        CGameCtnBlockInfoPylon &blockInfo)
        : zoneIdentifier_(std::move(zoneIdentifier)),
          asset_(asset),
          blockInfo_(&blockInfo) {}

const std::string &ReplaySceneZonePylonDefinition::ZoneIdentifier() const {
    return zoneIdentifier_;
}

BlockInfoAssetHandle ReplaySceneZonePylonDefinition::Asset() const {
    return asset_;
}

CGameCtnBlockInfoPylon *ReplaySceneZonePylonDefinition::BlockInfo() const {
    return blockInfo_.Get();
}

void ReplaySceneDefinition::Clear() {
    blocks_.clear();
    skippedAuthoredBlocks_.clear();
    constructionZones_.clear();
    zoneClips_.clear();
    zonePylons_.clear();
    collection_.reset();
    decorationSize_.reset();
    defaultPylonAsset_ = {};
    defaultPylonBlockInfo_.MwSetNod(nullptr);
}

bool ReplaySceneDefinition::Reserve(std::size_t blockCount) {
    try {
        blocks_.reserve(blockCount);
        skippedAuthoredBlocks_.reserve(blockCount);
        return true;
    } catch (const std::bad_alloc &) {
        Clear();
        return false;
    }
}

bool ReplaySceneDefinition::Add(ReplaySceneBlockDefinition block) {
    try {
        blocks_.push_back(std::move(block));
        return true;
    } catch (const std::bad_alloc &) {
        return false;
    }
}

bool ReplaySceneDefinition::SkipAuthoredBlock(
        CGameCtnReplayBlockPlacementId id) {
    if (FindAuthoredBlock(id) != nullptr || IsAuthoredBlockSkipped(id)) {
        return false;
    }
    try {
        skippedAuthoredBlocks_.push_back(id);
        return true;
    } catch (const std::bad_alloc &) {
        return false;
    }
}

bool ReplaySceneDefinition::SetConstructionZones(
        std::vector<ReplaySceneConstructionZoneDefinition> zones) {
    for (std::size_t index = 0u; index < zones.size(); ++index) {
        const ReplaySceneConstructionZoneDefinition &zone = zones[index];
        if (zone.ZoneIdentifier().empty() || !zone.Asset().IsValid() ||
            zone.BlockInfo() == nullptr) {
            return false;
        }
        for (std::size_t previous = 0u; previous < index; ++previous) {
            if (zones[previous].ZoneIdentifier() == zone.ZoneIdentifier() ||
                zones[previous].Asset() == zone.Asset() ||
                zones[previous].BlockInfo() == zone.BlockInfo()) {
                return false;
            }
        }
    }
    constructionZones_ = std::move(zones);
    return true;
}

bool ReplaySceneDefinition::SetZoneClips(
        std::vector<ReplaySceneZoneClipDefinition> clips) {
    for (std::size_t index = 0u; index < clips.size(); ++index) {
        const ReplaySceneZoneClipDefinition &clip = clips[index];
        if (clip.ZoneIdentifier().empty() || !clip.Asset().IsValid() ||
            clip.BlockInfo() == nullptr) {
            return false;
        }
        for (std::size_t previous = 0u; previous < index; ++previous) {
            if (clips[previous].ZoneIdentifier() == clip.ZoneIdentifier()) {
                return false;
            }
        }
    }
    zoneClips_ = std::move(clips);
    return true;
}

bool ReplaySceneDefinition::SetZonePylons(
        std::vector<ReplaySceneZonePylonDefinition> pylons) {
    for (std::size_t index = 0u; index < pylons.size(); ++index) {
        const ReplaySceneZonePylonDefinition &pylon = pylons[index];
        if (pylon.ZoneIdentifier().empty() || !pylon.Asset().IsValid() ||
            pylon.BlockInfo() == nullptr) {
            return false;
        }
        for (std::size_t previous = 0u; previous < index; ++previous) {
            if (pylons[previous].ZoneIdentifier() ==
                pylon.ZoneIdentifier()) {
                return false;
            }
        }
    }
    zonePylons_ = std::move(pylons);
    return true;
}

std::size_t ReplaySceneDefinition::BlockCount() const {
    return blocks_.size();
}

const ReplaySceneBlockDefinition *ReplaySceneDefinition::BlockAt(
        std::size_t index) const {
    return index < blocks_.size() ? &blocks_[index] : nullptr;
}

ReplaySceneBlockDefinition *ReplaySceneDefinition::MutableBlockAt(
        std::size_t index) {
    return index < blocks_.size() ? &blocks_[index] : nullptr;
}

const ReplaySceneBlockDefinition *ReplaySceneDefinition::FindAuthoredBlock(
        CGameCtnReplayBlockPlacementId id) const {
    for (const ReplaySceneBlockDefinition &block : blocks_) {
        if (block.PlacementId() && *block.PlacementId() == id) {
            return &block;
        }
    }
    return nullptr;
}

bool ReplaySceneDefinition::IsAuthoredBlockSkipped(
        CGameCtnReplayBlockPlacementId id) const {
    for (CGameCtnReplayBlockPlacementId skipped :
         skippedAuthoredBlocks_) {
        if (skipped == id) {
            return true;
        }
    }
    return false;
}

std::size_t ReplaySceneDefinition::SkippedAuthoredBlockCount() const {
    return skippedAuthoredBlocks_.size();
}

const ReplaySceneBlockDefinition *
ReplaySceneDefinition::AutomaticBaseBlock() const {
    for (const ReplaySceneBlockDefinition &block : blocks_) {
        if (block.IsAutomaticBase()) {
            return &block;
        }
    }
    return nullptr;
}

const std::vector<ReplaySceneBlockDefinition> &
ReplaySceneDefinition::Blocks() const {
    return blocks_;
}

std::vector<ReplaySceneBlockDefinition> &
ReplaySceneDefinition::MutableBlocks() {
    return blocks_;
}

const std::vector<ReplaySceneConstructionZoneDefinition> &
ReplaySceneDefinition::ConstructionZones() const {
    return constructionZones_;
}

CGameCtnBlockInfo *ReplaySceneDefinition::ConstructionZoneBlockInfo(
        std::string_view zoneIdentifier) const {
    for (const ReplaySceneConstructionZoneDefinition &zone :
         constructionZones_) {
        if (zone.ZoneIdentifier() == zoneIdentifier) {
            return zone.BlockInfo();
        }
    }
    return nullptr;
}

const char *ReplaySceneDefinition::ConstructionZoneIdentifier(
        const CGameCtnBlockInfo *blockInfo) const {
    if (blockInfo == nullptr) {
        return nullptr;
    }
    for (const ReplaySceneConstructionZoneDefinition &zone :
         constructionZones_) {
        if (zone.BlockInfo() == blockInfo) {
            return zone.ZoneIdentifier().c_str();
        }
    }
    return nullptr;
}

BlockInfoAssetHandle ReplaySceneDefinition::ConstructionZoneAsset(
        std::string_view zoneIdentifier) const {
    for (const ReplaySceneConstructionZoneDefinition &zone :
         constructionZones_) {
        if (zone.ZoneIdentifier() == zoneIdentifier) {
            return zone.Asset();
        }
    }
    return {};
}

const std::vector<ReplaySceneZoneClipDefinition> &
ReplaySceneDefinition::ZoneClips() const {
    return zoneClips_;
}

CGameCtnBlockInfoClip *ReplaySceneDefinition::ZoneClipBlockInfo(
        std::string_view zoneIdentifier) const {
    for (const ReplaySceneZoneClipDefinition &clip : zoneClips_) {
        if (clip.ZoneIdentifier() == zoneIdentifier) {
            return clip.BlockInfo();
        }
    }
    return nullptr;
}

CGameCtnBlockInfoClip *ReplaySceneDefinition::DefaultClipBlockInfo() const {
    const std::optional<std::string> blockInfoIdentifier =
            DefaultBaseIdentifier();
    if (!collection_ || !collection_->water || !blockInfoIdentifier) {
        return nullptr;
    }
    for (const CatalogWaterZoneDefinition &zone :
         collection_->water->zones) {
        if (zone.blockInfoIdentifier == *blockInfoIdentifier) {
            return ZoneClipBlockInfo(zone.identifier);
        }
    }
    return nullptr;
}

const std::vector<ReplaySceneZonePylonDefinition> &
ReplaySceneDefinition::ZonePylons() const {
    return zonePylons_;
}

CGameCtnBlockInfoPylon *ReplaySceneDefinition::ZonePylonBlockInfo(
        std::string_view zoneIdentifier) const {
    for (const ReplaySceneZonePylonDefinition &pylon : zonePylons_) {
        if (pylon.ZoneIdentifier() == zoneIdentifier) {
            return pylon.BlockInfo();
        }
    }
    return nullptr;
}

BlockInfoAssetHandle ReplaySceneDefinition::ZonePylonAsset(
        std::string_view zoneIdentifier) const {
    for (const ReplaySceneZonePylonDefinition &pylon : zonePylons_) {
        if (pylon.ZoneIdentifier() == zoneIdentifier) {
            return pylon.Asset();
        }
    }
    return {};
}

void ReplaySceneDefinition::SetCollection(
        CatalogCollectionDefinition definition) {
    collection_ = std::move(definition);
}

const std::optional<CatalogCollectionDefinition> &
ReplaySceneDefinition::Collection() const {
    return collection_;
}

void ReplaySceneDefinition::SetDecorationSize(
        CatalogDecorationSizeDefinition definition) {
    decorationSize_ = definition;
}

const std::optional<CatalogDecorationSizeDefinition> &
ReplaySceneDefinition::DecorationSize() const {
    return decorationSize_;
}

std::optional<std::string> ReplaySceneDefinition::DefaultBaseIdentifier()
        const {
    return collection_ ? collection_->defaultBaseIdentifier : std::nullopt;
}

std::optional<std::string> ReplaySceneDefinition::DefaultPylonIdentifier()
        const {
    return collection_ ? collection_->defaultPylonIdentifier : std::nullopt;
}

CGameCtnBlockInfoPylon *ReplaySceneDefinition::DefaultPylonBlockInfo() const {
    return defaultPylonBlockInfo_.Get();
}

BlockInfoAssetHandle ReplaySceneDefinition::DefaultPylonAsset() const {
    return defaultPylonAsset_;
}

void ReplaySceneDefinition::SetDefaultPylon(
        BlockInfoAssetHandle asset,
        CGameCtnBlockInfoPylon &blockInfo) {
    defaultPylonAsset_ = asset;
    defaultPylonBlockInfo_.MwSetNod(&blockInfo);
}

CatalogConstructionZoneKind ReplaySceneDefinition::DefaultZoneKind() const {
    return collection_ ? collection_->defaultZoneKind
                       : CatalogConstructionZoneKind::None;
}
