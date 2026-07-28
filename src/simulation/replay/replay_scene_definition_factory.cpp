#include "simulation/replay/replay_scene_definition_factory.h"
#include <cstdio>
#include <optional>
#include <string>

#include "engine/resources/block_info_catalog.h"
#include "engine/game/game_ctn_block_info.h"
#include "simulation/replay/replay_scene_surface_resolution.h"
#include "simulation/replay/replay_terrain_top_markers.h"
namespace {

const char *BlockTypeName(EBlockType type) {
    switch (type) {
        case EBlockType::Flat: return "flat";
        case EBlockType::Frontier: return "frontier";
        case EBlockType::Classic: return "classic";
        case EBlockType::Road: return "road";
        case EBlockType::Clip: return "clip";
        case EBlockType::Pylon: return "pylon";
        case EBlockType::Slope: return "slope";
        case EBlockType::RectAsym: return "rect-asym";
    }
    return "unknown";
}

std::string ResolveCollectionName(
        const CGameCtnReplayMapInput &mapInput,
        const BlockInfoCatalog &catalog) {
    std::string collection = mapInput.DefaultCollectionName();
    if (collection.empty() || collection.size() >= 64u) {
        collection = catalog.CollectionNameForMapInput(mapInput)
                             .value_or(std::string{});
    }
    if (collection.size() >= 64u) {
        collection.clear();
    }
    return collection;
}

bool PrepareBlockInfo(
        CGameCtnBlockInfo &blockInfo,
        const CGameCtnReplayMapInputBlock *placement,
        const BlockInfoCatalogEntry &catalogEntry,
        const BlockPlacementState &placementState) {
    blockInfo.ConfigurePlacement(catalogEntry.blockType,
                                 placementState.UsesCustomSize(),
                                 placementState.Source());
    if (placement != nullptr) {
        blockInfo.identifier.id.SetLocalName(
                placement->Identifier().Name().c_str());
    } else {
        blockInfo.identifier.id.SetLocalName(catalogEntry.identifier.c_str());
    }
    blockInfo.OnNodLoaded();
    return true;
}

bool ResolveConstructionZoneDefinitions(
        const CatalogCollectionDefinition &collection,
        const BlockInfoCatalog &catalog,
        CatalogAssetRepository &assets,
        std::vector<ReplaySceneConstructionZoneDefinition> *resolvedOut) {
    if (resolvedOut == nullptr || !collection.water.has_value() ||
        collection.water->zones.empty()) {
        return false;
    }

    struct Candidate {
        std::string zoneIdentifier;
        const BlockInfoCatalogEntry *entry = nullptr;
        CGameCtnBlockInfo *blockInfo = nullptr;
    };
    std::vector<Candidate> candidates;
    std::vector<ReplaySceneConstructionZoneDefinition> resolved;
    try {
        candidates.reserve(collection.water->zones.size());
        std::size_t defaultZoneCount = 0u;
        for (const CatalogWaterZoneDefinition &zone :
             collection.water->zones) {
            const EBlockType expectedType =
                    zone.kind == CatalogConstructionZoneKind::Flat
                    ? EBlockType::Flat
                    : EBlockType::Frontier;
            const bool hasFrontierParent =
                    zone.frontierParentIdentifier.has_value() &&
                    !zone.frontierParentIdentifier->empty();
            const bool hasFrontierChild =
                    zone.frontierChildIdentifier.has_value() &&
                    !zone.frontierChildIdentifier->empty();
            if (zone.identifier.empty() || zone.blockInfoIdentifier.empty() ||
                (zone.kind != CatalogConstructionZoneKind::Flat &&
                 zone.kind != CatalogConstructionZoneKind::Frontier) ||
                (zone.kind == CatalogConstructionZoneKind::Frontier
                         ? (!hasFrontierParent || !hasFrontierChild)
                         : (zone.frontierParentIdentifier.has_value() ||
                            zone.frontierChildIdentifier.has_value()))) {
                return false;
            }
            if (collection.defaultBaseIdentifier.has_value() &&
                zone.blockInfoIdentifier ==
                        *collection.defaultBaseIdentifier) {
                if (zone.kind != CatalogConstructionZoneKind::Flat ||
                    zone.oldZone) {
                    return false;
                }
                ++defaultZoneCount;
            }
            const BlockInfoCatalogEntry *entry =
                    catalog.FindForIdentifierAndCollection(
                            zone.blockInfoIdentifier, collection.name);
            CGameCtnBlockInfo *blockInfo = entry != nullptr
                    ? assets.BlockInfo(entry->asset)
                    : nullptr;
            if (entry == nullptr || entry->blockType != expectedType ||
                !entry->asset.IsValid() || blockInfo == nullptr) {
                return false;
            }
            for (const Candidate &candidate : candidates) {
                if (candidate.zoneIdentifier == zone.identifier ||
                    candidate.entry->asset == entry->asset ||
                    candidate.blockInfo == blockInfo) {
                    return false;
                }
            }
            candidates.push_back({zone.identifier, entry, blockInfo});
        }
        if (collection.defaultZoneKind !=
                    CatalogConstructionZoneKind::Flat ||
            !collection.defaultBaseIdentifier.has_value() ||
            defaultZoneCount != 1u) {
            return false;
        }
        resolved.reserve(candidates.size());
        const BlockPlacementState placement =
                BlockPlacementState::AutomaticBase();
        for (Candidate &candidate : candidates) {
            if (!PrepareBlockInfo(*candidate.blockInfo,
                                  nullptr,
                                  *candidate.entry,
                                  placement)) {
                return false;
            }
            resolved.emplace_back(std::move(candidate.zoneIdentifier),
                                  candidate.entry->asset,
                                  *candidate.blockInfo);
        }
    } catch (const std::bad_alloc &) {
        return false;
    }
    if (resolved.size() != collection.water->zones.size()) {
        return false;
    }
    *resolvedOut = std::move(resolved);
    return true;
}

bool ResolveZoneClipDefinitions(
        const CatalogCollectionDefinition &collection,
        const BlockInfoCatalog &catalog,
        CatalogAssetRepository &assets,
        std::vector<ReplaySceneZoneClipDefinition> *resolvedOut) {
    if (resolvedOut == nullptr || !collection.water.has_value() ||
        collection.defaultZoneKind != CatalogConstructionZoneKind::Flat ||
        !collection.defaultBaseIdentifier.has_value()) {
        return false;
    }

    const CatalogWaterZoneDefinition *defaultZone = nullptr;
    std::size_t flatZoneCount = 0u;
    for (const CatalogWaterZoneDefinition &zone : collection.water->zones) {
        if (zone.kind == CatalogConstructionZoneKind::Flat) {
            ++flatZoneCount;
        }
        if (zone.blockInfoIdentifier == *collection.defaultBaseIdentifier) {
            defaultZone = &zone;
        }
    }
    const BlockInfoCatalogEntry *defaultEntry =
            defaultZone != nullptr && defaultZone->clipIdentifier.has_value()
            ? catalog.FindForIdentifierAndCollection(
                      *defaultZone->clipIdentifier, collection.name)
            : nullptr;
    CGameCtnBlockInfo *defaultBase = defaultEntry != nullptr
            ? assets.BlockInfo(defaultEntry->asset)
            : nullptr;
    auto *defaultClip =
            dynamic_cast<CGameCtnBlockInfoClip *>(defaultBase);
    const BlockPlacementState placement =
            BlockPlacementState::AutomaticBase();
    if (defaultZone == nullptr ||
        defaultZone->kind != CatalogConstructionZoneKind::Flat ||
        !defaultZone->selectedClipDescriptorPath.has_value() ||
        defaultZone->selectedClipDescriptorPath->empty() ||
        defaultEntry == nullptr || defaultEntry->blockType != EBlockType::Clip ||
        defaultEntry->selectedDescriptorPath !=
                *defaultZone->selectedClipDescriptorPath ||
        defaultClip == nullptr ||
        defaultClip->SourceAsset() != defaultEntry->asset ||
        !PrepareBlockInfo(*defaultClip, nullptr, *defaultEntry, placement)) {
        return false;
    }

    std::vector<ReplaySceneZoneClipDefinition> resolved;
    try {
        resolved.reserve(flatZoneCount);
        for (const CatalogWaterZoneDefinition &zone :
             collection.water->zones) {
            if (zone.kind != CatalogConstructionZoneKind::Flat) {
                if (zone.clipIdentifier.has_value() ||
                    zone.selectedClipDescriptorPath.has_value()) {
                    return false;
                }
                continue;
            }
            const bool hasIdentifier = zone.clipIdentifier.has_value();
            const bool hasPath = zone.selectedClipDescriptorPath.has_value();
            if (zone.identifier.empty() || zone.blockInfoIdentifier.empty() ||
                hasIdentifier != hasPath) {
                return false;
            }
            const BlockInfoCatalogEntry *entry = hasIdentifier
                    ? catalog.FindForIdentifierAndCollection(
                              *zone.clipIdentifier, collection.name)
                    : defaultEntry;
            CGameCtnBlockInfo *base = entry != nullptr
                    ? assets.BlockInfo(entry->asset)
                    : nullptr;
            auto *clip = hasIdentifier
                    ? dynamic_cast<CGameCtnBlockInfoClip *>(base)
                    : defaultClip;
            if (entry == nullptr || entry->blockType != EBlockType::Clip ||
                clip == nullptr ||
                clip->SourceAsset() != entry->asset ||
                (hasIdentifier &&
                 (zone.selectedClipDescriptorPath->empty() ||
                  entry->selectedDescriptorPath !=
                          *zone.selectedClipDescriptorPath ||
                  !PrepareBlockInfo(
                          *clip, nullptr, *entry, placement)))) {
                return false;
            }
            resolved.emplace_back(zone.identifier, entry->asset, *clip);
        }
    } catch (const std::bad_alloc &) {
        return false;
    }
    if (resolved.size() != flatZoneCount || resolved.empty()) {
        return false;
    }
    *resolvedOut = std::move(resolved);
    return true;
}

bool ResolveZonePylonDefinitions(
        const CatalogCollectionDefinition &collection,
        const BlockInfoCatalog &catalog,
        CatalogAssetRepository &assets,
        std::vector<ReplaySceneZonePylonDefinition> *resolvedOut) {
    if (resolvedOut == nullptr || !collection.water.has_value()) {
        return false;
    }

    std::vector<ReplaySceneZonePylonDefinition> resolved;
    try {
        resolved.reserve(collection.water->zones.size());
        const BlockPlacementState placement =
                BlockPlacementState::AutomaticBase();
        for (const CatalogWaterZoneDefinition &zone :
             collection.water->zones) {
            const bool hasIdentifier = zone.pylonIdentifier.has_value();
            const bool hasPath = zone.selectedPylonDescriptorPath.has_value();
            if (zone.kind != CatalogConstructionZoneKind::Flat) {
                if (hasIdentifier || hasPath) {
                    return false;
                }
                continue;
            }
            if (zone.identifier.empty() || zone.blockInfoIdentifier.empty() ||
                hasIdentifier != hasPath) {
                return false;
            }
            if (!hasIdentifier) {
                continue;
            }

            const BlockInfoCatalogEntry *entry =
                    catalog.FindForIdentifierAndCollection(
                            *zone.pylonIdentifier, collection.name);
            CGameCtnBlockInfo *base = entry != nullptr
                    ? assets.BlockInfo(entry->asset)
                    : nullptr;
            auto *pylon = dynamic_cast<CGameCtnBlockInfoPylon *>(base);
            if (entry == nullptr || entry->blockType != EBlockType::Pylon ||
                zone.selectedPylonDescriptorPath->empty() ||
                entry->selectedDescriptorPath !=
                        *zone.selectedPylonDescriptorPath ||
                pylon == nullptr ||
                !PrepareBlockInfo(*pylon, nullptr, *entry, placement)) {
                return false;
            }
            resolved.emplace_back(zone.identifier, entry->asset, *pylon);
        }
    } catch (const std::bad_alloc &) {
        return false;
    }
    *resolvedOut = std::move(resolved);
    return true;
}

class ReplaySceneDefinitionFactory {
public:
    ReplaySceneDefinitionFactory(const CGameCtnReplayMapInput &mapInput,
                                 CatalogAssetRepository &mapAssets,
                                 CatalogAssetRepository &decorationAssets,
                                 ReplaySceneDefinition &scene)
            : mapInput_(mapInput),
              mapAssets_(mapAssets),
              decorationAssets_(decorationAssets),
              scene_(scene) {}

    bool Build();

private:
    bool LoadCollection();
    bool LoadConstructionZones();
    bool LoadZoneClips();
    bool LoadZonePylons();
    bool LoadAuthoredBlocks();
    bool BuildSpatialSources();
    bool ResolveClipSources();
    bool ResolveMaterialRemaps();
    bool Fail(const char *reason,
              const CGameCtnReplayMapInputBlock *placement = nullptr,
              const BlockInfoCatalogEntry *catalogEntry = nullptr);

    const CGameCtnReplayMapInput &mapInput_;
    CatalogAssetRepository &mapAssets_;
    CatalogAssetRepository &decorationAssets_;
    ReplaySceneDefinition &scene_;
    const BlockInfoCatalog *catalog_ = nullptr;
    std::string collectionName_;
    TerrainTopMarkers terrainMarkers_;
    CollectionColumnSurfaces columnSurfaces_;
    ReplaySceneUnits units_;
};

bool ReplaySceneDefinitionFactory::Fail(
        const char *reason,
        const CGameCtnReplayMapInputBlock *placement,
        const BlockInfoCatalogEntry *catalogEntry) {
    std::fprintf(stderr,
                 "replay scene definition failed reason=%s placement=%u "
                 "block=%s collection=%s type=%s variant=%u family=%s\n",
                 reason != nullptr ? reason : "unspecified",
                 placement != nullptr ? placement->Id().Ordinal() : UINT32_MAX,
                 placement != nullptr
                         ? placement->Identifier().Name().c_str()
                         : "",
                 collectionName_.c_str(),
                 catalogEntry != nullptr
                         ? BlockTypeName(catalogEntry->blockType)
                         : "unknown",
                 placement != nullptr
                         ? placement->Placement().VariantIndex()
                         : 0u,
                 placement != nullptr &&
                                 placement->Placement()
                                         .UsesGroundMobilFamily()
                         ? "ground"
                         : "air");
    scene_.Clear();
    return false;
}

bool ReplaySceneDefinitionFactory::LoadCollection() {
    collectionName_ = ResolveCollectionName(mapInput_, *catalog_);
    if (collectionName_.empty()) {
        return true;
    }
    std::optional<CatalogCollectionDefinition> collection =
            mapAssets_.Collection(collectionName_);
    if (!collection) {
        return Fail("collection");
    }
    const auto decorationSize = decorationAssets_.DecorationSize(mapInput_);
    if (!decorationSize || !ReplayDecorationSizeMatchesMap(
                                   mapInput_, decorationSize->mapSize)) {
        return Fail("decoration-size");
    }
    scene_.SetCollection(std::move(*collection));
    scene_.SetDecorationSize(*decorationSize);
    return true;
}

bool ReplaySceneDefinitionFactory::LoadConstructionZones() {
    const auto &collection = scene_.Collection();
    if (!collection.has_value() || !collection->water.has_value()) {
        return true;
    }
    std::vector<ReplaySceneConstructionZoneDefinition> resolved;
    if (!ResolveConstructionZoneDefinitions(
                *collection, *catalog_, mapAssets_, &resolved) ||
        !scene_.SetConstructionZones(std::move(resolved))) {
        return Fail("construction-zones");
    }
    return true;
}

bool ReplaySceneDefinitionFactory::LoadZonePylons() {
    const auto &collection = scene_.Collection();
    if (!collection.has_value() || !collection->water.has_value()) {
        return true;
    }

    std::vector<ReplaySceneZonePylonDefinition> resolved;
    if (!ResolveZonePylonDefinitions(
                *collection, *catalog_, mapAssets_, &resolved) ||
        !scene_.SetZonePylons(std::move(resolved))) {
        return Fail("zone-pylons");
    }

    const CatalogWaterZoneDefinition *defaultZone = nullptr;
    for (const CatalogWaterZoneDefinition &zone :
         collection->water->zones) {
        if (collection->defaultBaseIdentifier.has_value() &&
            zone.blockInfoIdentifier ==
                    *collection->defaultBaseIdentifier) {
            defaultZone = &zone;
            break;
        }
    }
    const std::optional<std::string> identifier =
            scene_.DefaultPylonIdentifier();
    if (defaultZone == nullptr ||
        identifier.has_value() != defaultZone->pylonIdentifier.has_value()) {
        return Fail("default-pylon");
    }
    if (!identifier.has_value()) {
        return true;
    }
    if (*identifier != *defaultZone->pylonIdentifier) {
        return Fail("default-pylon");
    }
    CGameCtnBlockInfoPylon *pylon =
            scene_.ZonePylonBlockInfo(defaultZone->identifier);
    const BlockInfoAssetHandle asset =
            scene_.ZonePylonAsset(defaultZone->identifier);
    if (pylon == nullptr || !asset.IsValid()) {
        return Fail("default-pylon");
    }
    scene_.SetDefaultPylon(asset, *pylon);
    return true;
}

bool ReplaySceneDefinitionFactory::LoadZoneClips() {
    const auto &collection = scene_.Collection();
    if (!collection.has_value()) {
        return true;
    }
    return ResolveReplaySceneZoneClips(
                   *collection, *catalog_, mapAssets_, scene_) ||
           Fail("zone-clips");
}

bool ReplaySceneDefinitionFactory::LoadAuthoredBlocks() {
    for (const CGameCtnReplayMapInputBlock &placement : mapInput_.Blocks()) {
        bool ambiguous = false;
        const BlockInfoCatalogEntry *catalogEntry =
                catalog_->FindForBlock(placement, &ambiguous);
        if (catalogEntry == nullptr || !catalogEntry->asset.IsValid()) {
            return Fail(ambiguous ? "ambiguous-block" : "unknown-block",
                        &placement,
                        catalogEntry);
        }
        CGameCtnBlockInfo *blockInfo =
                mapAssets_.BlockInfo(catalogEntry->asset);
        if (blockInfo == nullptr ||
            !PrepareBlockInfo(*blockInfo,
                              &placement,
                              *catalogEntry,
                              placement.Placement())) {
            return Fail("block-asset", &placement, catalogEntry);
        }
        auto definition = ReplaySceneBlockDefinition::Authored(
                placement, *catalogEntry, collectionName_, *blockInfo);
        if (!definition) {
            return Fail("block-definition", &placement, catalogEntry);
        }
        if (definition->IsClip() &&
            !definition->InstallDefaultClipSourceSides(mapAssets_.Mobil(
                    catalogEntry->asset,
                    placement.Placement().UsesGroundMobilFamily(),
                    placement.Placement().VariantIndex()))) {
            return Fail("clip-source", &placement, catalogEntry);
        }
        if (!scene_.Add(std::move(*definition))) {
            return Fail("block-storage", &placement, catalogEntry);
        }
    }
    return true;
}

bool ReplaySceneDefinitionFactory::BuildSpatialSources() {
    if (!terrainMarkers_.BuildFromSceneDefinition(scene_, mapInput_)) {
        return Fail("terrain-markers");
    }
    for (const ReplaySceneBlockDefinition &definition : scene_.Blocks()) {
        if (!definition.IsAuthored()) {
            continue;
        }
        const CGameCtnReplayMapInputBlock *placement =
                mapInput_.FindBlock(*definition.PlacementId());
        if (placement == nullptr ||
            !columnSurfaces_.AddBlockSurfaces(
                    *placement, definition, definition.BlockInfo()) ||
            !units_.AddBlockUnitSources(*placement,
                                        definition,
                                        definition.BlockInfo(),
                                        &terrainMarkers_)) {
            return Fail("spatial-sources", placement);
        }
    }
    return true;
}

bool ReplaySceneDefinitionFactory::ResolveClipSources() {
    ReplaySceneAssetResolver resolver;
    resolver.assets = &mapAssets_;
    for (ReplaySceneBlockDefinition &definition : scene_.MutableBlocks()) {
        CGameCtnBlockInfo *blockInfo = definition.BlockInfo();
        if (blockInfo != nullptr) {
            resolver.ResolveJunctionSources(*blockInfo);
        }
    }
    for (const ReplaySceneConstructionZoneDefinition &zone :
         scene_.ConstructionZones()) {
        CGameCtnBlockInfo *blockInfo = zone.BlockInfo();
        if (blockInfo != nullptr) {
            resolver.ResolveJunctionSources(*blockInfo);
        }
    }
    return units_.ApplyClipNeighbourSourceSides(
                   mapInput_, resolver, scene_, &columnSurfaces_) ||
           Fail("clip-neighbours");
}

bool ReplaySceneDefinitionFactory::ResolveMaterialRemaps() {
    ReplaySceneAssetResolver assetResolver;
    assetResolver.assets = &mapAssets_;
    for (ReplaySceneBlockDefinition &definition : scene_.MutableBlocks()) {
        const CGameCtnReplayMapInputBlock *placement =
                definition.PlacementId()
                        ? mapInput_.FindBlock(*definition.PlacementId())
                        : nullptr;
        ReplaySceneSurfaceResolver resolver;
        resolver.assets = &mapAssets_;
        resolver.collectionName = collectionName_;
        resolver.scene = &scene_;
        resolver.units = &units_;
        resolver.columnSurfaces = &columnSurfaces_;
        resolver.block = placement;
        resolver.definition = &definition;
        resolver.assetResolver = &assetResolver;
        int remapResolved = 1;
        const bool replacement =
                resolver.ReplacementRemapApplies(&remapResolved) != 0;
        definition.SetMaterialRemaps(remapResolved && replacement, false);
    }
    return true;
}

bool ReplaySceneDefinitionFactory::Build() {
    scene_.Clear();
    catalog_ = mapAssets_.Catalog();
    if (catalog_ == nullptr || !scene_.Reserve(mapInput_.BlockCount())) {
        return Fail("catalog");
    }
    return LoadCollection() && LoadConstructionZones() && LoadZoneClips() &&
           LoadZonePylons() &&
           LoadAuthoredBlocks() && BuildSpatialSources() && ResolveClipSources() &&
           ResolveMaterialRemaps() &&
           (scene_.BlockCount() == mapInput_.BlockCount() ||
            Fail("block-coverage"));
}

}  // namespace

bool ResolveReplaySceneConstructionZones(
        const CatalogCollectionDefinition &collection,
        const BlockInfoCatalog &catalog,
        CatalogAssetRepository &assets,
        ReplaySceneDefinition &scene) {
    std::vector<ReplaySceneConstructionZoneDefinition> resolved;
    return ResolveConstructionZoneDefinitions(
                   collection, catalog, assets, &resolved) &&
           scene.SetConstructionZones(std::move(resolved));
}

bool ResolveReplaySceneZoneClips(
        const CatalogCollectionDefinition &collection,
        const BlockInfoCatalog &catalog,
        CatalogAssetRepository &assets,
        ReplaySceneDefinition &scene) {
    std::vector<ReplaySceneZoneClipDefinition> resolved;
    return ResolveZoneClipDefinitions(
                   collection, catalog, assets, &resolved) &&
           scene.SetZoneClips(std::move(resolved));
}

bool ResolveReplaySceneZonePylons(
        const CatalogCollectionDefinition &collection,
        const BlockInfoCatalog &catalog,
        CatalogAssetRepository &assets,
        ReplaySceneDefinition &scene) {
    std::vector<ReplaySceneZonePylonDefinition> resolved;
    return ResolveZonePylonDefinitions(
                   collection, catalog, assets, &resolved) &&
           scene.SetZonePylons(std::move(resolved));
}

bool BuildReplaySceneDefinition(
        const CGameCtnReplayMapInput &mapInput,
        CatalogAssetRepository &assets,
        ReplaySceneDefinition &scene) {
    return BuildReplaySceneDefinition(mapInput, assets, assets, scene);
}

bool BuildReplaySceneDefinition(
        const CGameCtnReplayMapInput &mapInput,
        CatalogAssetRepository &mapAssets,
        CatalogAssetRepository &decorationAssets,
        ReplaySceneDefinition &scene) {
    return ReplaySceneDefinitionFactory(
            mapInput, mapAssets, decorationAssets, scene).Build();
}

bool ReplaySceneDefinition::AppendAutomaticBase(
        const CGameCtnReplayMapInput &mapInput,
        CatalogAssetRepository &assets) {
    if (AutomaticBaseBlock() != nullptr ||
        DefaultZoneKind() != CatalogConstructionZoneKind::Flat) {
        return AutomaticBaseBlock() != nullptr;
    }
    const std::optional<std::string> identifier = DefaultBaseIdentifier();
    const BlockInfoCatalog *catalog = assets.Catalog();
    const auto &decorationSize = DecorationSize();
    if (!identifier || !Collection() || !decorationSize ||
        !ReplayDecorationSizeMatchesMap(
                mapInput, decorationSize->mapSize) ||
        catalog == nullptr) {
        return false;
    }
    const BlockInfoCatalogEntry *entry =
            catalog->FindForIdentifierAndCollection(*identifier,
                                                    Collection()->name);
    CGameCtnBlockInfo *blockInfo = entry != nullptr
            ? assets.BlockInfo(entry->asset)
            : nullptr;
    const BlockPlacementState placement = BlockPlacementState::AutomaticBase();
    if (entry == nullptr || blockInfo == nullptr ||
        !PrepareBlockInfo(*blockInfo, nullptr, *entry, placement)) {
        return false;
    }
    auto definition = ReplaySceneBlockDefinition::AutomaticBase(
            *entry, *blockInfo);
    if (!definition) {
        return false;
    }
    if (definition->IsClip() &&
        !definition->InstallDefaultClipSourceSides(
                assets.Mobil(entry->asset,
                             true,
                             placement.VariantIndex()))) {
        return false;
    }
    return Add(std::move(*definition));
}
