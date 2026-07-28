#include "engine/game/game_ctn_block_info.h"
#include "simulation/replay/replay_scene_surface_resolution.h"
#include <cstring>

#include "engine/game/game_ctn_utils.h"
#include <new>
namespace {

bool SameText(const char *lhs, const char *rhs) {
    return lhs != nullptr && rhs != nullptr && std::strcmp(lhs, rhs) == 0;
}
int32_t SignedCoordinate(u32 coordinate) {
    return static_cast<int32_t>(coordinate);
}

GmInt3 NeighbourCoordinate(
        const CGameCtnReplayMapInputBlock &block,
        u32 side) {
    const GmNat3 neighbor = CGameCtnUtils::GetNeighbourCoord(
            block.Coordinate(), static_cast<ECardinalDir>(side));
    return {SignedCoordinate(neighbor.x),
            SignedCoordinate(neighbor.y),
            SignedCoordinate(neighbor.z)};
}

GmInt3 RotatedOffset(
        const CGameCtnBlockInfo &blockInfo,
        const CGameCtnBlockUnitInfo &unit,
        ECardinalDir direction,
        bool ground) {
    const GmNat3 offset = blockInfo.RotatedUnitOffset(
            unit, direction, ground ? 1 : 0);
    return {SignedCoordinate(offset.x),
            SignedCoordinate(offset.y),
            SignedCoordinate(offset.z)};
}

bool IsBlockOnGround(
        const CGameCtnBlockInfo &blockInfo,
        const TerrainTopMarkers &terrainMarkers,
        const CGameCtnReplayMapInputBlock &block,
        EBlockType blockType) {
    const auto &groundUnits = blockInfo.BlockUnitInfos(1);
    if (groundUnits.empty()) {
        return false;
    }
    if (blockType < EBlockType::Classic) {
        return true;
    }
    bool hasUnderground = false;
    for (const auto &unit : groundUnits) {
        hasUnderground = hasUnderground || unit->IsUndergroundUnit();
    }
    u32 checkedLayer = 0u;
    if (hasUnderground) {
        bool found = false;
        for (const auto &unit : groundUnits) {
            if (unit->IsUndergroundUnit()) {
                continue;
            }
            if (!found || unit->UnitLayer() < checkedLayer) {
                checkedLayer = unit->UnitLayer();
                found = true;
            }
        }
    }
    for (const auto &unit : groundUnits) {
        if (unit->IsUndergroundUnit() || unit->UnitLayer() != checkedLayer) {
            continue;
        }
        const GmInt3 rotated = RotatedOffset(
                blockInfo, *unit, block.Direction(), true);
        const GmNat3 &coordinate = block.Coordinate();
        if (!terrainMarkers.Contains(
                    SignedCoordinate(coordinate.x) + rotated.x,
                    SignedCoordinate(coordinate.y) + rotated.y,
                    SignedCoordinate(coordinate.z) + rotated.z)) {
            return false;
        }
    }
    return true;
}

const char *SurfaceName(const CGameCtnBlockUnitInfo *unit) {
    return unit != nullptr ? unit->SurfaceIdName() : nullptr;
}

}  // namespace

int CollectionColumnSurface::Configure(
        int32_t columnX,
        int32_t columnZ,
        const char *newSurfaceIdName,
        const CGameCtnBlockInfo *newBlockInfo,
        CatalogConstructionZoneKind newZoneKind,
        u32 newTerrainBaseY,
        std::optional<u32> newCollectionLandZoneHeight) {
    x = columnX;
    z = columnZ;
    return SetSource(newSurfaceIdName,
                     newBlockInfo,
                     newZoneKind,
                     newTerrainBaseY,
                     newCollectionLandZoneHeight);
}

int CollectionColumnSurface::MatchesColumn(
        int32_t columnX,
        int32_t columnZ) const {
    return x == columnX && z == columnZ;
}

int CollectionColumnSurface::SetSource(
        const char *newSurfaceIdName,
        const CGameCtnBlockInfo *newBlockInfo,
        CatalogConstructionZoneKind newZoneKind,
        u32 newTerrainBaseY,
        std::optional<u32> newCollectionLandZoneHeight) {
    try {
        surfaceIdName = newSurfaceIdName != nullptr ? newSurfaceIdName : "";
        blockInfo = newBlockInfo;
        zoneKind = newZoneKind;
        terrainBaseY = newTerrainBaseY;
        collectionLandZoneHeight = newCollectionLandZoneHeight;
        return 1;
    } catch (const std::bad_alloc &) {
        return 0;
    }
}

const char *CollectionColumnSurface::SurfaceIdName() const {
    return surfaceIdName.empty() ? nullptr : surfaceIdName.c_str();
}

const CGameCtnBlockInfo *CollectionColumnSurface::BlockInfo() const {
    return blockInfo;
}

CatalogConstructionZoneKind CollectionColumnSurface::ZoneKind() const {
    return zoneKind;
}

bool CollectionColumnSurface::IsFullUnderground(u32 y) const {
    if (y > terrainBaseY) {
        return false;
    }
    if (zoneKind != CatalogConstructionZoneKind::Frontier) {
        return true;
    }
    return collectionLandZoneHeight.has_value() &&
           static_cast<uint64_t>(y) + *collectionLandZoneHeight <=
                   static_cast<uint64_t>(terrainBaseY);
}

int CollectionColumnSurfaces::Upsert(
        int32_t x,
        int32_t z,
        const char *surfaceIdName,
        const CGameCtnBlockInfo *blockInfo,
        CatalogConstructionZoneKind zoneKind,
        u32 terrainBaseY,
        std::optional<u32> collectionLandZoneHeight) {
    for (CollectionColumnSurface &column : columns) {
        if (column.MatchesColumn(x, z)) {
            return column.SetSource(surfaceIdName,
                                    blockInfo,
                                    zoneKind,
                                    terrainBaseY,
                                    collectionLandZoneHeight);
        }
    }
    try {
        CollectionColumnSurface column;
        if (!column.Configure(x,
                              z,
                              surfaceIdName,
                              blockInfo,
                              zoneKind,
                              terrainBaseY,
                              collectionLandZoneHeight)) {
            return 0;
        }
        columns.push_back(std::move(column));
        return 1;
    } catch (const std::bad_alloc &) {
        return 0;
    }
}

const char *CollectionColumnSurfaces::Find(int32_t x, int32_t z) const {
    for (const CollectionColumnSurface &column : columns) {
        if (column.MatchesColumn(x, z)) {
            return column.SurfaceIdName();
        }
    }
    return nullptr;
}

const CGameCtnBlockInfo *CollectionColumnSurfaces::FindBlockInfo(
        int32_t x,
        int32_t z) const {
    for (const CollectionColumnSurface &column : columns) {
        if (column.MatchesColumn(x, z)) {
            return column.BlockInfo();
        }
    }
    return nullptr;
}

CatalogConstructionZoneKind CollectionColumnSurfaces::FindZoneKind(
        int32_t x,
        int32_t z) const {
    for (const CollectionColumnSurface &column : columns) {
        if (column.MatchesColumn(x, z)) {
            return column.ZoneKind();
        }
    }
    return CatalogConstructionZoneKind::Flat;
}

bool CollectionColumnSurfaces::IsFullUnderground(
        int32_t x,
        u32 y,
        int32_t z) const {
    for (const CollectionColumnSurface &column : columns) {
        if (column.MatchesColumn(x, z)) {
            return column.IsFullUnderground(y);
        }
    }
    return false;
}

int CollectionColumnSurfaces::AddBlockSurfaces(
        const CGameCtnReplayMapInputBlock &block,
        const ReplaySceneBlockDefinition &definition,
        const CGameCtnBlockInfo *blockInfo) {
    if (blockInfo == nullptr || !definition.IsTerrainBlock()) {
        return 1;
    }
    const auto &groundUnits = blockInfo->BlockUnitInfos(1);
    const auto &units = !groundUnits.empty()
            ? groundUnits
            : blockInfo->BlockUnitInfos(
                    definition.Placement().UsesGroundMobilFamily() ? 1 : 0);
    const CatalogConstructionZoneKind zoneKind =
            definition.Type() == EBlockType::Frontier
            ? CatalogConstructionZoneKind::Frontier
            : CatalogConstructionZoneKind::Flat;
    if (zoneKind == CatalogConstructionZoneKind::Frontier &&
        !definition.CollectionLandZoneHeight().has_value()) {
        return 0;
    }
    if (units.empty()) {
        const GmNat3 &coordinate = block.Coordinate();
        return Upsert(SignedCoordinate(coordinate.x),
                      SignedCoordinate(coordinate.z),
                      nullptr,
                      blockInfo,
                      zoneKind,
                      coordinate.y,
                      definition.CollectionLandZoneHeight());
    }
    const char *defaultSurface = SurfaceName(units.front().get());
    for (const auto &unit : units) {
        const char *surface = SurfaceName(unit.get());
        if (surface == nullptr || surface[0] == '\0') {
            surface = defaultSurface;
        }
        const GmInt3 rotated = RotatedOffset(
                *blockInfo, *unit, block.Direction(), true);
        const GmNat3 &coordinate = block.Coordinate();
        if (!Upsert(SignedCoordinate(coordinate.x) + rotated.x,
                    SignedCoordinate(coordinate.z) + rotated.z,
                    surface,
                    blockInfo,
                    zoneKind,
                    coordinate.y,
                    definition.CollectionLandZoneHeight())) {
            return 0;
        }
    }
    return 1;
}

ReplayClipConstructionDecision ResolveClipConstructionDecision(
        CatalogConstructionZoneKind realZoneKind,
        bool usesGroundMobilFamily,
        bool isFullUnderground) {
    return !isFullUnderground && usesGroundMobilFamily &&
                   realZoneKind == CatalogConstructionZoneKind::Frontier
            ? ReplayClipConstructionDecision::NoMobil
            : ReplayClipConstructionDecision::ConstructMobil;
}

void ReplaySceneAssetResolver::ResolveJunctionSources(
        CGameCtnBlockInfo &blockInfo) const {
    for (int family = 0; family <= 1; ++family) {
        for (const auto &unitInfo : blockInfo.BlockUnitInfos(family)) {
            if (unitInfo == nullptr) {
                continue;
            }
            for (u32 side = 0u; side < 4u; ++side) {
                const ECardinalDir direction =
                        static_cast<ECardinalDir>(side);
                CGameCtnBlockInfoClip *source =
                        unitInfo->JunctionAt(direction);
                const BlockInfoAssetHandle sourceAsset =
                        source != nullptr
                        ? source->SourceAsset()
                        : BlockInfoAssetHandle{};
                if (!sourceAsset.IsValid()) {
                    continue;
                }
                CGameCtnBlockInfoClip *resolved =
                        ClipBlockInfo(sourceAsset);
                if (resolved != nullptr) {
                    unitInfo->SetJunction(direction, resolved);
                }
            }
        }
    }
}

CSceneMobil *ReplaySceneAssetResolver::SelectMobil(
        BlockInfoAssetHandle sourceAsset,
        u32 selectorGroup,
        u32 variantIndex) const {
    return assets != nullptr && sourceAsset.IsValid()
            ? assets->Mobil(sourceAsset, selectorGroup != 0u, variantIndex)
            : nullptr;
}

CGameCtnBlockInfoClip *ReplaySceneAssetResolver::ClipBlockInfo(
        BlockInfoAssetHandle sourceAsset) const {
    return assets != nullptr && sourceAsset.IsValid()
            ? dynamic_cast<CGameCtnBlockInfoClip *>(
                      assets->BlockInfo(sourceAsset))
            : nullptr;
}

std::optional<std::string> ReplaySceneAssetResolver::FirstGroundSurface(
        BlockInfoAssetHandle sourceAsset) const {
    return assets != nullptr && sourceAsset.IsValid()
            ? assets->FirstGroundSurface(sourceAsset)
            : std::nullopt;
}

int ReplaySceneUnit::Configure(
        const CGameCtnReplayMapInputBlock &block,
        const ReplaySceneBlockDefinition &definition,
        CGameCtnBlockUnitInfo &sourceUnitInfo,
        const GmInt3 &rotatedOffset) {
    try {
        const GmNat3 &coordinate = block.Coordinate();
        x = SignedCoordinate(coordinate.x) + rotatedOffset.x;
        y = SignedCoordinate(coordinate.y) + rotatedOffset.y;
        z = SignedCoordinate(coordinate.z) + rotatedOffset.z;
        placementId = block.Id();
        direction = block.Direction();
        blockType = definition.Type();
        unitInfo = &sourceUnitInfo;
        return 1;
    } catch (const std::bad_alloc &) {
        return 0;
    }
}

int ReplaySceneUnit::MatchesCoord(
        int32_t queryX,
        int32_t queryY,
        int32_t queryZ) const {
    return x == queryX && y == queryY && z == queryZ;
}

int ReplaySceneUnit::BelongsToPlacement(
        CGameCtnReplayBlockPlacementId id) const {
    return placementId == id;
}

BlockInfoAssetHandle ReplaySceneUnit::SourceAssetForSide(
        u32 side) const {
    CGameCtnBlockInfoClip *clip = unitInfo != nullptr && side < 4u
            ? unitInfo->JunctionAt(static_cast<ECardinalDir>(side))
            : nullptr;
    return clip != nullptr ? clip->SourceAsset() : BlockInfoAssetHandle{};
}

void ReplaySceneUnit::SetSourceClipForSide(
        u32 side,
        CGameCtnBlockInfoClip *clip) const {
    if (unitInfo != nullptr && side < 4u && clip != nullptr) {
        unitInfo->SetJunction(static_cast<ECardinalDir>(side), clip);
    }
}

int32_t ReplaySceneUnit::X() const { return x; }
int32_t ReplaySceneUnit::Y() const { return y; }
int32_t ReplaySceneUnit::Z() const { return z; }
ECardinalDir ReplaySceneUnit::Direction() const { return direction; }
EBlockType ReplaySceneUnit::BlockType() const { return blockType; }

int ReplaySceneUnits::Append(const ReplaySceneUnit &unit) {
    try {
        units.push_back(unit);
        return 1;
    } catch (const std::bad_alloc &) {
        return 0;
    }
}

int ReplaySceneUnits::AddBlockUnitSources(
        const CGameCtnReplayMapInputBlock &block,
        const ReplaySceneBlockDefinition &definition,
        const CGameCtnBlockInfo *blockInfo,
        const TerrainTopMarkers *terrainMarkers) {
    if (blockInfo == nullptr || terrainMarkers == nullptr) {
        return 0;
    }
    const bool ground = definition.IsTerrainBlock() ||
            IsBlockOnGround(*blockInfo,
                            *terrainMarkers,
                            block,
                            definition.Type());
    const auto &unitInfos = blockInfo->BlockUnitInfos(ground ? 1 : 0);
    for (const auto &unitInfo : unitInfos) {
        const GmInt3 rotated = RotatedOffset(
                *blockInfo, *unitInfo, block.Direction(), ground);
        ReplaySceneUnit unit;
        if (!unit.Configure(block, definition, *unitInfo, rotated) ||
            !Append(unit)) {
            return 0;
        }
    }
    return 1;
}

const ReplaySceneUnit *ReplaySceneUnits::FindAtCoord(
        int32_t x,
        int32_t y,
        int32_t z) const {
    // CGameCtnChallenge::UpdateFieldUnits replaces an earlier attachment, so
    // CGameCtnChallenge::GetBlockUnitFromPlayField sees the latest unit.
    for (auto it = units.rbegin(); it != units.rend(); ++it) {
        if (it->MatchesCoord(x, y, z)) {
            return &*it;
        }
    }
    return nullptr;
}

const ReplaySceneUnit *ReplaySceneUnits::FindFirstForPlacement(
        CGameCtnReplayBlockPlacementId id) const {
    for (const ReplaySceneUnit &unit : units) {
        if (unit.BelongsToPlacement(id)) {
            return &unit;
        }
    }
    return nullptr;
}

int ReplaySceneUnits::ApplyClipNeighbourSourceSides(
        const CGameCtnReplayMapInput &mapInput,
        const ReplaySceneAssetResolver &assetResolver,
        ReplaySceneDefinition &scene,
        const CollectionColumnSurfaces *columnSurfaces) const {
    for (ReplaySceneBlockDefinition &definition : scene.MutableBlocks()) {
        if (!definition.IsClip()) {
            continue;
        }
        const CGameCtnReplayMapInputBlock *block = definition.PlacementId()
                ? mapInput.FindBlock(*definition.PlacementId())
                : nullptr;
        if (block == nullptr) {
            continue;
        }
        auto &clipSources = definition.MutableClipSourceMobils();
        const GmNat3 &coordinate = block->Coordinate();
        const CatalogConstructionZoneKind realZoneKind =
                columnSurfaces != nullptr
                ? columnSurfaces->FindZoneKind(
                          SignedCoordinate(coordinate.x),
                          SignedCoordinate(coordinate.z))
                : CatalogConstructionZoneKind::Flat;
        const bool isFullUnderground = columnSurfaces != nullptr &&
                columnSurfaces->IsFullUnderground(
                        SignedCoordinate(coordinate.x),
                        coordinate.y,
                        SignedCoordinate(coordinate.z));
        if (ResolveClipConstructionDecision(
                    realZoneKind,
                    definition.Placement().UsesGroundMobilFamily(),
                    isFullUnderground) ==
                ReplayClipConstructionDecision::NoMobil) {
            for (CMwNodRef<CSceneMobil> &source : clipSources) {
                source.MwSetNod(nullptr);
            }
            continue;
        }
        for (u32 side = 0u; side < clipSources.size(); ++side) {
            const GmInt3 neighbor = NeighbourCoordinate(*block, side);
            const ReplaySceneUnit *neighborUnit =
                    FindAtCoord(neighbor.x, neighbor.y, neighbor.z);
            if (neighborUnit == nullptr ||
                neighborUnit->BlockType() == EBlockType::Road ||
                neighborUnit->BlockType() == EBlockType::Clip) {
                continue;
            }
            const u32 sourceSlot = static_cast<u32>(CGameCtnUtils::SubDirs(
                    CGameCtnUtils::GetOpposedDir(
                            static_cast<ECardinalDir>(side)),
                    neighborUnit->Direction()));
            const BlockInfoAssetHandle sourceAsset =
                    neighborUnit->SourceAssetForSide(sourceSlot);
            if (!sourceAsset.IsValid()) {
                continue;
            }
            CGameCtnBlockInfoClip *sourceClip =
                    assetResolver.ClipBlockInfo(sourceAsset);
            if (sourceClip != nullptr) {
                neighborUnit->SetSourceClipForSide(sourceSlot, sourceClip);
            }
            CSceneMobil *sourceMobil = assetResolver.SelectMobil(
                    sourceAsset,
                    definition.Placement().UsesGroundMobilFamily() ? 1u : 0u,
                    definition.Placement().VariantIndex());
            if (sourceMobil == nullptr) {
                continue;
            }
            clipSources[side].MwSetNod(sourceMobil);
        }
    }
    return 1;
}

std::optional<std::string>
ReplaySceneSurfaceResolver::SourceSurfaceIdName(
        int *okOut) const {
    if (okOut != nullptr) {
        *okOut = 1;
    }
    if (scene == nullptr || block == nullptr || definition == nullptr) {
        return std::nullopt;
    }
    const CGameCtnBlockInfo *blockInfo = definition->BlockInfo();
    const char *sourceSurface = blockInfo != nullptr &&
            blockInfo->HasBlockUnitInfos(true)
            ? SurfaceName(blockInfo->FirstBlockUnitInfo(1))
            : nullptr;
    if (!definition->IsClip()) {
        return sourceSurface != nullptr
                ? std::optional<std::string>(sourceSurface)
                : std::nullopt;
    }
    const char *realZoneSurface = RealZoneSurfaceIdName(okOut);
    if ((okOut != nullptr && *okOut == 0) || realZoneSurface == nullptr ||
        realZoneSurface[0] == '\0') {
        return sourceSurface != nullptr
                ? std::optional<std::string>(sourceSurface)
                : std::nullopt;
    }
    std::string resolvedSurface = realZoneSurface;
    if (units == nullptr) {
        return resolvedSurface;
    }
    if (assetResolver == nullptr) {
        if (okOut != nullptr) {
            *okOut = 0;
        }
        return std::nullopt;
    }
    for (u32 side = 0u; side < 4u; ++side) {
        const GmInt3 neighbor = NeighbourCoordinate(*block, side);
        const ReplaySceneUnit *neighborUnit =
                units->FindAtCoord(neighbor.x, neighbor.y, neighbor.z);
        if (neighborUnit == nullptr) {
            continue;
        }
        const u32 sourceSlot = static_cast<u32>(CGameCtnUtils::SubDirs(
                CGameCtnUtils::GetOpposedDir(
                        static_cast<ECardinalDir>(side)),
                neighborUnit->Direction()));
        const BlockInfoAssetHandle sourceAsset =
                neighborUnit->SourceAssetForSide(sourceSlot);
        if (!sourceAsset.IsValid()) {
            continue;
        }
        const std::optional<std::string> neighborSurface =
                assetResolver->FirstGroundSurface(sourceAsset);
        if (!neighborSurface) {
            if (okOut != nullptr) {
                *okOut = 0;
            }
            return std::nullopt;
        }
        if (!SameText(neighborSurface->c_str(), realZoneSurface)) {
            resolvedSurface = *neighborSurface;
        }
    }
    return resolvedSurface;
}

int ReplaySceneSurfaceResolver::ReplacementRemapApplies(
        int *okOut) const {
    if (okOut != nullptr) {
        *okOut = 1;
    }
    if (assets == nullptr || scene == nullptr || block == nullptr ||
        definition == nullptr ||
        !definition->Placement().UsesGroundMobilFamily() ||
        definition->Type() <= EBlockType::Frontier) {
        return 0;
    }
    const char *realZoneId = RealZoneIdName(okOut);
    if (okOut != nullptr && *okOut == 0) {
        return 0;
    }
    int sourceOk = 1;
    const std::optional<std::string> sourceSurface =
            SourceSurfaceIdName(&sourceOk);
    if (!sourceOk) {
        if (okOut != nullptr) {
            *okOut = 0;
        }
        return 0;
    }
    if (!sourceSurface || realZoneId == nullptr ||
        SameText(sourceSurface->c_str(), realZoneId)) {
        return 0;
    }
    return assets->HasSurfaceReplacement(collectionName,
                                         *sourceSurface,
                                         realZoneId);
}

const char *ReplaySceneSurfaceResolver::RealZoneIdName(
        int *okOut) const {
    if (okOut != nullptr) {
        *okOut = 1;
    }
    if (scene == nullptr || block == nullptr || definition == nullptr ||
        units == nullptr || columnSurfaces == nullptr ||
        !definition->PlacementId()) {
        if (okOut != nullptr) {
            *okOut = 0;
        }
        return nullptr;
    }
    const ReplaySceneUnit *firstUnit =
            units->FindFirstForPlacement(*definition->PlacementId());
    if (firstUnit == nullptr) {
        if (okOut != nullptr) {
            *okOut = 0;
        }
        return nullptr;
    }
    const CGameCtnBlockInfo *zoneBlockInfo =
            columnSurfaces->FindBlockInfo(firstUnit->X(), firstUnit->Z());
    const char *zoneId = scene->ConstructionZoneIdentifier(zoneBlockInfo);
    if ((zoneId == nullptr || zoneId[0] == '\0') && okOut != nullptr) {
        *okOut = 0;
    }
    return zoneId;
}

const char *ReplaySceneSurfaceResolver::RealZoneSurfaceIdName(
        int *okOut) const {
    if (okOut != nullptr) {
        *okOut = 1;
    }
    int32_t realZoneX = block != nullptr
            ? SignedCoordinate(block->Coordinate().x) : 0;
    int32_t realZoneZ = block != nullptr
            ? SignedCoordinate(block->Coordinate().z) : 0;
    if (scene == nullptr || block == nullptr || definition == nullptr) {
        if (okOut != nullptr) {
            *okOut = 0;
        }
        return nullptr;
    }
    const ReplaySceneUnit *firstUnit =
            units != nullptr && definition->PlacementId()
            ? units->FindFirstForPlacement(*definition->PlacementId())
            : nullptr;
    if (firstUnit != nullptr) {
        realZoneX = firstUnit->X();
        realZoneZ = firstUnit->Z();
    }
    const char *surface = columnSurfaces != nullptr
            ? columnSurfaces->Find(realZoneX, realZoneZ)
            : nullptr;
    if ((surface == nullptr || surface[0] == '\0') && okOut != nullptr) {
        *okOut = 0;
    }
    return surface;
}
