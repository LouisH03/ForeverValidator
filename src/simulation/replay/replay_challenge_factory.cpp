#include "engine/game/game_ctn_block_info.h"
#include "simulation/replay/replay_challenge_factory.h"
#include <algorithm>
#include <memory>
#include <vector>

#include "engine/game/game_ctn_collection.h"
#include "engine/game/game_ctn_decoration.h"
#include "engine/game/game_ctn_zone.h"
#include "engine/core/finite_values.h"
#include "engine/game/block_race_role.h"
#include "simulation/replay/replay_challenge_construction.h"
#include "simulation/replay/replay_challenge_field_units.h"
namespace {

bool IsFiniteLogicalStart(const GmIso4 &location) {
    return FiniteValues::IsFinite(location.rotation.basisX) &&
           FiniteValues::IsFinite(location.rotation.basisY) &&
           FiniteValues::IsFinite(location.rotation.basisZ) &&
           FiniteValues::IsFinite(location.translation);
}

bool ChallengeContainsBlock(const CGameCtnChallenge &challenge,
                            const CGameCtnBlock *block) {
    return block != nullptr && std::any_of(
            challenge.Blocks().begin(),
            challenge.Blocks().end(),
            [block](const std::unique_ptr<CGameCtnBlock> &owned) {
                return owned.get() == block;
            });
}

bool CreateInitialBlockMobil(CGameCtnChallenge &challenge,
                             CGameCtnBlock *block) {
    if (!ChallengeContainsBlock(challenge, block)) {
        return false;
    }
    challenge.CreateMobilForBlock(block);
    return true;
}

bool QueueInitialBlockMobil(CGameCtnChallenge &challenge,
                            CGameCtnBlock *block,
                            bool mobilAlreadyCreated = false) {
    if (!ChallengeContainsBlock(challenge, block)) {
        return false;
    }
    if (!mobilAlreadyCreated) {
        challenge.CreateMobilForBlock(block);
    }
    challenge.AddBlockToAddList(block);
    return true;
}

bool InitializeConstructedChallengeZoneGrid(
        CGameCtnChallenge &challenge,
        CGameCtnCollection &collection) {
    for (const std::unique_ptr<CGameCtnBlock> &block : challenge.Blocks()) {
        if (block == nullptr || block->GetType() > EBlockType::Frontier) {
            continue;
        }
        (void)challenge.CheckTerrainBlock(block.get());
        CGameCtnZone *realZone =
                collection.GetZoneFromLandBlockInfo(block->BlockInfoRef());
        CGameCtnZoneFlat *basicZone = realZone != nullptr
                ? collection.GetBasicZone(realZone->zoneId)
                : nullptr;
        if (basicZone == nullptr) {
            return false;
        }
        challenge.SetZoneId(block->BaseCoord(), basicZone->zoneId);
        challenge.SetZoneHeight(block->BaseCoord(), block->BaseCoord().y);
    }
    return true;
}

bool InstallConstructedChallengeZones(
        CGameCtnChallenge &challenge,
        const ReplaySceneDefinition &scene,
        const std::vector<CGameCtnBlock *> &immediateInitialMobils,
        std::vector<CGameCtnBlock *> *removedInitialBlocks,
        std::vector<CGameCtnBlock *> *createdInitialMobils) {
    removedInitialBlocks->clear();
    createdInitialMobils->clear();
    const auto &collectionDefinition = scene.Collection();
    const auto &decorationDefinition = scene.DecorationSize();
    if (!collectionDefinition.has_value() ||
        !collectionDefinition->water.has_value() ||
        !decorationDefinition.has_value()) {
        return true;
    }

    const CatalogCollectionWaterDefinition &water =
            *collectionDefinition->water;
    CMwNodRef<CGameCtnCollection> collection =
            MakeMwNod<CGameCtnCollection>();
    CMwNodRef<CGameCtnDecoration> decoration =
            MakeMwNod<CGameCtnDecoration>();
    if (!collection || !decoration) {
        return false;
    }
    for (const CatalogSurfaceReplacementDefinition &replacement :
         collectionDefinition->surfaceReplacements) {
        if (replacement.sourceSurfaceId.empty() ||
            replacement.targetSurfaceId.empty()) {
            return false;
        }
        CMwId sourceSurfaceId;
        CMwId targetSurfaceId;
        sourceSurfaceId.SetLocalName(
                replacement.sourceSurfaceId.c_str());
        targetSurfaceId.SetLocalName(
                replacement.targetSurfaceId.c_str());
        collection->AddSurfaceReplacement(sourceSurfaceId, targetSurfaceId);
    }
    decoration->Size().mapWidth = decorationDefinition->mapSize.x;
    decoration->Size().mapHeight = decorationDefinition->mapSize.y;
    decoration->Size().mapDepth = decorationDefinition->mapSize.z;
    decoration->Size().defaultZoneHeight =
            decorationDefinition->defaultZoneHeight;

    if (scene.ConstructionZones().size() != water.zones.size()) {
        return false;
    }
    bool installedDefaultZone = false;
    for (const CatalogWaterZoneDefinition &zoneDefinition : water.zones) {
        CGameCtnBlockInfo *blockInfo =
                scene.ConstructionZoneBlockInfo(zoneDefinition.identifier);
        const EBlockType expectedType =
                zoneDefinition.kind == CatalogConstructionZoneKind::Flat
                ? EBlockType::Flat
                : EBlockType::Frontier;
        const char *blockInfoIdentifier = blockInfo != nullptr
                ? blockInfo->identifier.id.GetString()
                : nullptr;
        if (zoneDefinition.identifier.empty() ||
            zoneDefinition.blockInfoIdentifier.empty() ||
            (zoneDefinition.kind != CatalogConstructionZoneKind::Flat &&
             zoneDefinition.kind != CatalogConstructionZoneKind::Frontier) ||
            blockInfo == nullptr || blockInfo->Type() != expectedType ||
            blockInfoIdentifier == nullptr ||
            zoneDefinition.blockInfoIdentifier != blockInfoIdentifier ||
            !scene.ConstructionZoneAsset(zoneDefinition.identifier).IsValid()) {
            return false;
        }

        CGameCtnZone *zone = nullptr;
        CMwNodRef<CGameCtnZoneFlat> flatZone;
        CMwNodRef<CGameCtnZoneFrontier> frontierZone;
        if (zoneDefinition.kind == CatalogConstructionZoneKind::Flat) {
            CGameCtnBlockInfoClip *clipInfo =
                    scene.ZoneClipBlockInfo(zoneDefinition.identifier);
            if (clipInfo == nullptr) {
                return false;
            }
            flatZone = MakeMwNod<CGameCtnZoneFlat>();
            flatZone->blockInfoFlat.MwSetNod(blockInfo);
            flatZone->blockInfoClip.MwSetNod(clipInfo);
            flatZone->blockInfoPylon.MwSetNod(
                    scene.ZonePylonBlockInfo(zoneDefinition.identifier));
            zone = flatZone.Get();
        } else {
            if (!zoneDefinition.frontierParentIdentifier.has_value() ||
                zoneDefinition.frontierParentIdentifier->empty() ||
                !zoneDefinition.frontierChildIdentifier.has_value() ||
                zoneDefinition.frontierChildIdentifier->empty()) {
                return false;
            }
            frontierZone = MakeMwNod<CGameCtnZoneFrontier>();
            frontierZone->blockInfoFrontier.MwSetNod(blockInfo);
            frontierZone->parentZoneId.SetLocalName(
                    zoneDefinition.frontierParentIdentifier->c_str());
            frontierZone->childZoneId.SetLocalName(
                    zoneDefinition.frontierChildIdentifier->c_str());
            zone = frontierZone.Get();
        }
        if (zone == nullptr) {
            return false;
        }
        zone->zoneId.SetLocalName(zoneDefinition.identifier.c_str());
        zone->height = zoneDefinition.height;
        zone->depth = zoneDefinition.depth;
        zone->oldZone = zoneDefinition.oldZone;
        zone->hasWater = zoneDefinition.hasWater;
        if (zoneDefinition.oldZone) {
            collection->AddObsoleteZone(zone);
        } else {
            collection->AddZone(zone);
        }
        if (flatZone && collectionDefinition->defaultBaseIdentifier &&
            zoneDefinition.blockInfoIdentifier ==
                    *collectionDefinition->defaultBaseIdentifier) {
            if (installedDefaultZone || zoneDefinition.oldZone ||
                flatZone->blockInfoClip.Get() !=
                        scene.DefaultClipBlockInfo() ||
                flatZone->blockInfoPylon.Get() !=
                        scene.DefaultPylonBlockInfo()) {
                return false;
            }
            collection->SetDefaultZone(flatZone.Get());
            installedDefaultZone = true;
        }
    }
    if (!installedDefaultZone) {
        return false;
    }

    challenge.FastInitChallengeData(collection.Get(), decoration.Get());
    if (!InitializeConstructedChallengeZoneGrid(challenge, *collection)) {
        return false;
    }
    challenge.ConfigureReplayConstructionData(
            !scene.ConstructionZones().empty(),
            scene.DefaultPylonBlockInfo() != nullptr);
    challenge.CreateBlocksAndPylons();
    for (u32 pass = 0u; pass < 2u; ++pass) {
        std::size_t index = 0u;
        while (index < challenge.Blocks().size()) {
            CGameCtnBlock *block = challenge.Blocks()[index].get();
            if (block == nullptr ||
                (block->IsSuppressed() &&
                 block->GetType() > EBlockType::Frontier) ||
                ((block->GetType() <= EBlockType::Frontier) !=
                 (pass == 0u))) {
                ++index;
                continue;
            }
            const int attached = challenge.UpdateFieldUnits(block);
            if (pass != 0u && attached == 0) {
                // United removes failed non-terrain entries before their
                // initial mobil can be created or queued.
                removedInitialBlocks->push_back(block);
                if (challenge.RemoveBlock(block) == 0) {
                    return false;
                }
                continue;
            }
            if (pass != 0u &&
                std::find(immediateInitialMobils.begin(),
                          immediateInitialMobils.end(),
                          block) != immediateInitialMobils.end() &&
                CreateInitialBlockMobil(challenge, block)) {
                // United snapshots this mobil before the next authored
                // non-terrain block can alter the field-unit neighbourhood.
                // The deferred add keeps the terrain-first queue order.
                createdInitialMobils->push_back(block);
            }
            ++index;
        }
    }
    return true;
}

CGameCtnBlock *CreateConstructedBlock(
        CGameCtnChallenge &challenge,
        const CGameCtnReplayMapInputBlock &placement,
        const ReplaySceneBlockDefinition &definition) {
    CGameCtnBlockInfo *info = definition.BlockInfo();
    if (info == nullptr) {
        return nullptr;
    }
    auto loadedBlock = std::make_unique<CGameCtnBlock>(
            info,
            placement.Coordinate(),
            placement.Direction(),
            definition.Placement(),
            nullptr);
    CGameCtnBlock *block = loadedBlock.get();
    block->SetInstanceId(
            CGameCtnBlock::InstanceId(placement.Id().Ordinal()));
    block->SetSourceAsset(
            definition.Asset(),
            CGameCtnBlock::PlacementOrigin::Authored);
    block->SetMaterialRemaps(
            definition.UsesReplacementMaterialRemap(),
            definition.UsesDecorationSkinMaterialRemap());
    if (definition.UsesCollectionLandZoneHeight()) {
        block->SetMobilVerticalOffset(-static_cast<float>(
                *definition.CollectionLandZoneHeight()) *
                challenge.CollectionSquareHeight());
    }
    block->SetClipSourceMobils(definition.ClipSourceMobils());
    if (definition.IsClip()) {
        block->SetReplayLoadedClipSourceMobils(
                definition.ClipSourceMobils());
    }
    return challenge.AddLoadedBlock(std::move(loadedBlock));
}

CGameCtnBlock *CreateAutomaticBase(
        CGameCtnChallenge &challenge,
        const ReplaySceneBlockDefinition &definition,
        u32 x,
        u32 y,
        u32 z) {
    CGameCtnBlockInfo *info = definition.BlockInfo();
    if (info == nullptr) {
        return nullptr;
    }
    auto loadedBlock = std::make_unique<CGameCtnBlock>(
            info,
            GmNat3{x, y, z},
            ECardinalDir::North,
            definition.Placement(),
            nullptr);
    CGameCtnBlock *block = loadedBlock.get();
    block->SetSourceAsset(
            definition.Asset(),
            CGameCtnBlock::PlacementOrigin::AutomaticBase);
    block->SetMaterialRemaps(
            definition.UsesReplacementMaterialRemap(),
            definition.UsesDecorationSkinMaterialRemap());
    block->SetClipSourceMobils(definition.ClipSourceMobils());
    if (definition.IsClip()) {
        block->SetReplayLoadedClipSourceMobils(
                definition.ClipSourceMobils());
    }
    return challenge.AddLoadedBlock(std::move(loadedBlock));
}

bool OccupiesColumn(const ChallengeFieldUnits &fieldUnits,
                    u32 x,
                    u32 z) {
    for (const auto &unit : fieldUnits.Units()) {
        if (unit.IsTerrainTopMarker() &&
            unit.position.x == static_cast<int32_t>(x) &&
            unit.position.z == static_cast<int32_t>(z)) {
            return true;
        }
    }
    return false;
}

struct TopMarker {
    int32_t x;
    int32_t y;
    int32_t z;
    CGameCtnBlock *block;
};

void ApplyFieldUnitRemoval(
        CGameCtnChallenge &challenge,
        const ChallengeFieldUnits &fieldUnits,
        const std::vector<CGameCtnBlock *> &authored,
        const std::vector<CGameCtnBlock *> &automaticBase) {
    std::vector<TopMarker> markers;
    markers.reserve(fieldUnits.Count() + automaticBase.size());
    for (const auto &unit : fieldUnits.Units()) {
        const u32 ordinal = unit.placementId.Ordinal();
        if (!unit.IsTerrainTopMarker() || ordinal >= authored.size()) {
            continue;
        }
        markers.push_back(
                {unit.position.x,
                 unit.position.y + 1,
                 unit.position.z,
                 authored[ordinal]});
    }
    for (CGameCtnBlock *block : automaticBase) {
        markers.push_back({static_cast<int32_t>(block->BaseCoord().x),
                           static_cast<int32_t>(block->BaseCoord().y) + 1,
                           static_cast<int32_t>(block->BaseCoord().z),
                           block});
    }
    for (const auto &unit : fieldUnits.Units()) {
        const u32 ordinal = unit.placementId.Ordinal();
        if (!unit.IsPlacedMobil() || ordinal >= authored.size()) {
            continue;
        }
        CGameCtnBlock *remover = authored[ordinal];
        // A later landscape writes every field unit below its top to
        // Underground and writes only its top to Ground.
        const auto terrainState = std::find_if(
                markers.rbegin(),
                markers.rend(),
                [&unit](const TopMarker &marker) {
                    return marker.x == unit.position.x &&
                           marker.z == unit.position.z &&
                           marker.y >= unit.position.y;
                });
        if (terrainState == markers.rend() ||
            terrainState->y != unit.position.y) {
            continue;
        }

        // UpdateFieldUnits attaches every landscape block to the y=0 field
        // unit. GetBlockFromPlayField({x, 0, z}) and AddBlockToRemoveList
        // therefore keep selecting the last landscape attachment in the
        // column when stacked ground cells request its removal.
        const auto occupant = std::find_if(
                markers.rbegin(),
                markers.rend(),
                [&unit](const TopMarker &marker) {
                    return marker.x == unit.position.x &&
                           marker.z == unit.position.z;
                });
        if (occupant != markers.rend()) {
            challenge.RegisterBlockSuppression(
                    occupant->block, remover);
        }
    }
}

void QueueInitialBlockMobils(
        CGameCtnChallenge &challenge,
        const std::vector<CGameCtnBlock *> &authored,
        const std::vector<CGameCtnBlock *> &automaticBase,
        const std::vector<CGameCtnBlock *> &alreadyCreated) {
    const auto queue = [&challenge,
                        &alreadyCreated](CGameCtnBlock *block) {
        const bool mobilAlreadyCreated =
                std::find(alreadyCreated.begin(),
                          alreadyCreated.end(),
                          block) != alreadyCreated.end();
        (void)QueueInitialBlockMobil(
                challenge, block, mobilAlreadyCreated);
    };

    // United queues authored Flat/Frontier terrain, default terrain fill,
    // then every other authored type.
    for (CGameCtnBlock *block : authored) {
        if (block != nullptr && block->GetType() <= EBlockType::Frontier) {
            queue(block);
        }
    }
    for (CGameCtnBlock *block : automaticBase) {
        queue(block);
    }
    for (CGameCtnBlock *block : authored) {
        if (block != nullptr && block->GetType() > EBlockType::Frontier) {
            queue(block);
        }
    }
}

void SynchronizeColumnTerrainModifiers(CGameCtnChallenge &challenge) {
    for (const std::unique_ptr<CGameCtnBlock> &owned : challenge.Blocks()) {
        CGameCtnBlock *block = owned.get();
        CGameCtnBlockInfo *blockInfo = block != nullptr
                ? block->BlockInfoRef()
                : nullptr;
        if (blockInfo == nullptr ||
            (!block->UsesGroundMobilSize() &&
             block->GetType() != EBlockType::Flat &&
             block->GetType() != EBlockType::Frontier)) {
            continue;
        }
        CMwId modifier;
        for (const std::unique_ptr<CGameCtnBlockUnit> &unit :
             block->BlockUnits()) {
            if (unit == nullptr) {
                continue;
            }
            const CMwId candidate =
                    challenge.GetColumnModifierId(unit->GetCoord());
            if (!candidate.IsInvalid()) {
                modifier = candidate;
            }
        }
        const int ground = block->UsesGroundMobilSize() ? 1 : 0;
        if (modifier.IsInvalid() || blockInfo->IsTerrainModifier(ground)) {
            continue;
        }
        const bool replacement = block->UsesReplacementMaterialRemap();
        block->SetTerrainModifiedId(modifier);
        block->SetMaterialRemaps(
                replacement,
                replacement
                        ? block->UsesDecorationSkinMaterialRemap()
                        : !modifier.IsInvalid());
        block->SetMobilAndHelper(nullptr, nullptr, nullptr);
        challenge.CreateMobilForBlock(block);
    }
}

}  // namespace

bool BuildReplayChallenge(
        const CGameCtnReplayMapInput &mapInput,
        const ReplaySceneDefinition &scene,
        CGameCtnChallengeConstruction &construction,
        ReplayChallengeBuildReport &report) {
    report = ReplayChallengeBuildReport{};
    report.mapBlockCount = mapInput.BlockCount();

    const auto &decorationSize = scene.DecorationSize();
    if (!decorationSize || !ReplayDecorationSizeMatchesMap(
                                   mapInput, decorationSize->mapSize)) {
        return false;
    }

    ChallengeFieldUnits fieldUnits;
    if (!fieldUnits.Build(scene, mapInput)) {
        return false;
    }

    auto challenge = std::make_unique<CGameCtnChallenge>();
    challenge->SetMapSize(decorationSize->mapSize);
    if (scene.Collection().has_value()) {
        challenge->SetCollectionGrid(
                scene.Collection()->squareSize,
                scene.Collection()->squareHeight);
    }

    std::vector<CGameCtnBlock *> authored;
    authored.reserve(mapInput.BlockCount());
    std::vector<ReplaySceneLogicalStartPlacement> logicalStarts;
    u32 missingBlocks = 0u;
    for (const auto &placement : mapInput.Blocks()) {
        const ReplaySceneBlockDefinition *definition =
                scene.FindAuthoredBlock(placement.Id());
        if (definition == nullptr) {
            if (!scene.IsAuthoredBlockSkipped(placement.Id())) {
                return false;
            }
            missingBlocks++;
            continue;
        }
        CGameCtnBlock *block = CreateConstructedBlock(
                *challenge, placement, *definition);
        if (block == nullptr) {
            return false;
        }
        authored.push_back(block);
        const CGameCtnBlockInfo *blockInfo = block->BlockInfoRef();
        if (blockInfo != nullptr && ::IsStartLine(blockInfo->RaceRole())) {
            const GmIso4 spawnLocation = block->SpawnLocation(0u, 0u);
            if (IsFiniteLogicalStart(spawnLocation)) {
                logicalStarts.push_back(
                        {placement.Id().Ordinal(), spawnLocation});
            }
        }
    }

    std::vector<CGameCtnBlock *> automaticBase;
    const ReplaySceneBlockDefinition *base = scene.AutomaticBaseBlock();
    if (base != nullptr) {
        for (u32 x = 0u; x < decorationSize->mapSize.x; ++x) {
            for (u32 z = 0u; z < decorationSize->mapSize.z; ++z) {
                if (OccupiesColumn(fieldUnits, x, z)) {
                    continue;
                }
                CGameCtnBlock *block = CreateAutomaticBase(
                        *challenge,
                        *base,
                        x,
                        decorationSize->defaultZoneHeight,
                        z);
                if (block == nullptr) {
                    return false;
                }
                automaticBase.push_back(block);
            }
        }
    }

    ApplyFieldUnitRemoval(
            *challenge, fieldUnits, authored, automaticBase);
    std::vector<CGameCtnBlock *> removedInitialBlocks;
    std::vector<CGameCtnBlock *> immediateInitialMobils;
    for (CGameCtnBlock *block : authored) {
        if (block != nullptr && block->GetType() > EBlockType::Frontier) {
            immediateInitialMobils.push_back(block);
        }
    }
    std::vector<CGameCtnBlock *> createdInitialMobils;
    if (!InstallConstructedChallengeZones(
                *challenge,
                scene,
                immediateInitialMobils,
                &removedInitialBlocks,
                &createdInitialMobils)) {
        return false;
    }
    QueueInitialBlockMobils(*challenge,
                            authored,
                            automaticBase,
                            createdInitialMobils);
    SynchronizeColumnTerrainModifiers(*challenge);

    const auto rejectedCount = [&removedInitialBlocks](
            const std::vector<CGameCtnBlock *> &blocks) {
        return static_cast<u32>(std::count_if(
                blocks.begin(),
                blocks.end(),
                [&removedInitialBlocks](CGameCtnBlock *block) {
                    return std::find(removedInitialBlocks.begin(),
                                     removedInitialBlocks.end(),
                                     block) != removedInitialBlocks.end();
                }));
    };
    const u32 rejectedAuthored = rejectedCount(authored);
    const u32 rejectedAutomaticBase = rejectedCount(automaticBase);
    const u32 rejectedCountTotal =
            static_cast<u32>(removedInitialBlocks.size());
    if (rejectedAuthored + rejectedAutomaticBase != rejectedCountTotal) {
        return false;
    }
    construction.SetCounts(
            static_cast<u32>(authored.size()) - rejectedAuthored,
            static_cast<u32>(automaticBase.size()) - rejectedAutomaticBase,
            0u,
            missingBlocks);
    construction.SetChallenge(std::move(challenge));
    construction.SetRetainedLogicalStartPlacements(
            std::move(logicalStarts));
    report.resolvedBlockCount = static_cast<u32>(scene.BlockCount());
    report.fieldUnitCount = fieldUnits.Count();
    report.constructedBlockCount = construction.BlockCount();
    report.automaticBaseBlockCount = construction.AutomaticBaseBlockCount();
    report.terrainModifierBlockCount =
            construction.TerrainModifierBlockCount();
    report.removedInitialBlockCount = rejectedCountTotal;
    report.missingBlockCount = construction.MissingBlockCount();
    report.complete =
            missingBlocks == scene.SkippedAuthoredBlockCount() &&
            construction.BlockCount() ==
                    construction.AuthoredBlockCount() +
                    construction.AutomaticBaseBlockCount() +
                    construction.TerrainModifierBlockCount();
    return report.complete;
}
