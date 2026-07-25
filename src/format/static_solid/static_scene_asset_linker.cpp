#include "format/static_solid/static_scene_asset_linker.h"
#include <memory>
#include <algorithm>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

#include "engine/game/game_ctn_block.h"
#include "format/static_solid/static_scene_archive_loader.h"
#include "engine/scene/replay_scene_placements.h"
#include "engine/scene/replay_static_scene_corpuses.h"
#include "engine/scene/scene_mobil.h"
#include "engine/resources/static_solid_asset.h"
#include "format/static_solid/static_solid_archive_assembler.h"
#include "format/static_solid/static_solid_archive_reference.h"
#include <new>
namespace {

struct LinkedAsset {
    std::string archivePath;
    StaticSolidArchiveId payload =
            StaticSolidArchiveId::Invalid();
    std::shared_ptr<StaticSolidAsset> asset;
};

void RootDecodedStaticTree(CPlugTree *tree) {
    if (tree == nullptr) {
        return;
    }
    tree->SetIsRooted(1);
    // Serialized roots can contain rooted parents with unrooted source children.
    const u32 childCount = static_cast<u32>(tree->GetChildCount());
    for (u32 childIndex = 0u; childIndex < childCount; childIndex++) {
        RootDecodedStaticTree(tree->GetChild(childIndex));
    }
}

template <typename Prepare>
CMwNodRef<CPlugSolid> BuildPrototypeSolid(
        CPlugTree *sourceRoot,
        const CGameCtnReplayStaticSolidArchiveSolidPhysicsDefinition *physical,
        Prepare &&prepare) {
    if (sourceRoot == nullptr) {
        return {};
    }
    std::unique_ptr<CPlugTree> root(
            sourceRoot->InternalCreateSolidModelInstance());
    if (root == nullptr) {
        return {};
    }
    prepare(root.get());
    CMwNodRef<CPlugSolid> solid = MakeMwNod<CPlugSolid>();
    if (physical != nullptr) {
        physical->ApplyToSolid(solid.Get());
    }
    solid->SetOwnedTree(std::move(root), 0);
    return solid;
}

const LinkedAsset *FindAsset(const std::vector<LinkedAsset> &assets,
                             const std::string &archivePath) {
    for (const LinkedAsset &entry : assets) {
        if (entry.archivePath == archivePath) {
            return &entry;
        }
    }
    return nullptr;
}

bool BuildAssets(
        StaticSolidArchiveLoadSession &payloads,
        StaticSolidArchiveAssembler &archive,
        std::vector<LinkedAsset> &assets) {
    assets.clear();
    bool ok = true;
    payloads.ForEachPayload(
            [&](StaticSolidArchiveId payload,
                const StaticSolidArchivePayload &payloadAsset) {
        auto asset = std::make_shared<StaticSolidAsset>();
        CPlugTree *sourceRoot = archive.CollisionRoot(payload);
        const auto *physical = archive.Physics(payload);
        // Decoded descendants are source geometry, not volatile model children.
        RootDecodedStaticTree(sourceRoot);

        CMwNodRef<CPlugSolid> base =
                (BuildPrototypeSolid(
            (sourceRoot),
            (physical),
            [](CPlugTree *) {}));
        CMwNodRef<CPlugSolid> blockMobil = BuildPrototypeSolid(
                        sourceRoot,
                        physical,
                        [](CPlugTree *root) { root->SetModelInstance(true); });
        CMwNodRef<CPlugSolid> replacement = BuildPrototypeSolid(
                        sourceRoot,
                        physical,
                        [&](CPlugTree *root) {
                            root->SetModelInstance(true);
                            archive.ApplyReplacementMaterials(root, payload);
                        });
        CMwNodRef<CPlugSolid> decorationSkin = BuildPrototypeSolid(
                        sourceRoot,
                        physical,
                        [&](CPlugTree *root) {
                            root->SetModelInstance(true);
                            archive.ApplyDecorationSkinMaterials(root, payload);
                        });
        CMwNodRef<CPlugSolid> warp = BuildPrototypeSolid(
                        sourceRoot,
                        physical,
                        [&](CPlugTree *root) {
                            archive.ApplyWarpDecoration(root);
                        });
        asset->SetPrototype(StaticSolidMaterialVariant::Base, base.Get());
        asset->SetPrototype(
                StaticSolidMaterialVariant::BlockMobil,
                blockMobil.Get());
        asset->SetPrototype(
                StaticSolidMaterialVariant::Replacement,
                replacement.Get());
        asset->SetPrototype(
                StaticSolidMaterialVariant::DecorationSkin,
                decorationSkin.Get());
        asset->SetPrototype(StaticSolidMaterialVariant::Warp, warp.Get());

        try {
            assets.push_back({payloadAsset.SelectedDescriptorPath(),
                              payload,
                              std::move(asset)});
        } catch (const std::bad_alloc &) {
            ok = false;
            return 0;
        }
        return 1;
    });
    return ok;
}

void BindMobil(CSceneMobil *mobil,
               const std::vector<LinkedAsset> &assets,
               const StaticSolidArchiveReferenceCatalog &references) {
    if (mobil == nullptr) {
        return;
    }
    if (mobil->StaticSolidAsset().IsSpecified()) {
        const LinkedAsset *entry = FindAsset(
                assets,
                references.SelectedDescriptorPath(
                        mobil->StaticSolidAsset()));
        if (entry != nullptr) {
            references.Bind(mobil->StaticSolidAsset(), entry->asset);
        }
    }
    for (const CMwNodRef<CSceneObjectLink> &link : mobil->Links()) {
        BindMobil(dynamic_cast<CSceneMobil *>(
                          link.Get() != nullptr ? link->Object() : nullptr),
                  assets,
                  references);
    }
}

void BindBlock(const CGameCtnBlock *block,
               const std::vector<LinkedAsset> &assets,
               const StaticSolidArchiveReferenceCatalog &references) {
    if (block == nullptr) {
        return;
    }
    BindMobil(block->MainMobil(), assets, references);
    BindMobil(block->TriggerMobil(), assets, references);
    BindMobil(block->HelperMobil(), assets, references);
    for (const CMwNodRef<CSceneMobil> &mobil : block->SubMobils()) {
        BindMobil(mobil.Get(), assets, references);
    }
    for (const CMwNodRef<CSceneMobil> &mobil : block->ClipSourceMobils()) {
        BindMobil(mobil.Get(), assets, references);
    }
    CGameCtnBlockInfo *blockInfo = block->BlockInfoRef();
    if (blockInfo != nullptr) {
        BindMobil(blockInfo->FamilyHelperMobilSource(true), assets, references);
        BindMobil(blockInfo->FamilyHelperMobilSource(false), assets, references);
        BindMobil(blockInfo->CommonHelperMobilSource(), assets, references);
    }
}

bool BindArchiveModels(
        CGameCtnReplayArchiveStaticModelCollection &models,
        const std::vector<LinkedAsset> &assets,
        StaticSolidArchiveAssembler &archive) {
    return models.ForEachMutableRecord(
            [&](StaticSceneArchiveModelRecord &record) {
        CGameCtnReplayArchiveStaticModel &model = record.model;
        const LinkedAsset *entry = FindAsset(
                assets, record.source.selectedDescriptorPath);
        if (entry == nullptr) {
            model.BindPrototype({});
            return 1;
        }
        if (!record.source.treeNodeIndex.has_value()) {
            model.BindPrototype(entry->asset->Prototype(
                    record.source.IsWarp()
                            ? StaticSolidMaterialVariant::Warp
                            : StaticSolidMaterialVariant::Base));
            return 1;
        }

        CPlugTree *root = archive.ModelRoot(
                entry->payload, record.source.treeNodeIndex);
        const auto *physical = archive.Physics(entry->payload);
        CMwNodRef<CPlugSolid> prototype =
                (BuildPrototypeSolid(
            (root),
            (physical),
            [](CPlugTree *) {}));
        model.BindPrototype(StaticSolidPrototype(prototype.Get()));
        return 1;
    });
}

bool AddArchiveSceneModels(
        const CGameCtnReplayArchiveStaticModelCollection &archiveModels,
        StaticSceneModelCollection &models) {
    bool ok = models.Reserve(archiveModels.Count());
    archiveModels.ForEachRecord(
            [&](const StaticSceneArchiveModelRecord &record) {
        if (record.dependencyOnly) {
            return 1;
        }
        const CGameCtnReplayArchiveStaticModel &source = record.model;
        if (!source.Prototype().IsValid()) {
            models.MarkIncomplete();
            return 1;
        }
        std::string lowerPath = record.source.selectedDescriptorPath;
        std::transform(
                lowerPath.begin(), lowerPath.end(), lowerPath.begin(),
                [](unsigned char value) {
                    return static_cast<char>(std::tolower(value));
                });
        StaticScenePurpose purpose = StaticScenePurpose::Environment;
        if (!record.source.sceneObjectId.empty()) {
            purpose = StaticScenePurpose::Generated;
        }
        if (lowerPath.find("terrain") != std::string::npos ||
            lowerPath.find("ground") != std::string::npos ||
            lowerPath.find("land") != std::string::npos) {
            purpose = StaticScenePurpose::Terrain;
        } else if (lowerPath.find("decoration") != std::string::npos ||
                   lowerPath.find("sky") != std::string::npos) {
            purpose = StaticScenePurpose::Decoration;
        }
        StaticSceneModel model(source.Prototype(),
                               source.WorldIso(),
                               purpose);
        StaticSceneProvenance provenance;
        provenance.descriptorPath =
                record.source.selectedDescriptorPath;
        provenance.sceneObjectId = record.source.sceneObjectId;
        provenance.componentIndex =
                record.source.treeNodeIndex.value_or(0u);
        provenance.authored = false;
        model.SetProvenance(std::move(provenance));
        if (source.ItemProperties().has_value()) {
            model.SetItemProperties(*source.ItemProperties());
        }
        if (!models.Add(std::move(model))) {
            ok = false;
            return 0;
        }
        return 1;
    });
    return ok;
}

}  // namespace

bool LinkStaticSceneAssets(
        StaticSolidArchiveLoadSession &payloads,
        StaticSolidArchiveAssembler &archive,
        CGameCtnReplayArchiveStaticModelCollection &archiveModels,
        const ReplaySceneBlockPlacements &placements,
        const StaticSolidArchiveReferenceCatalog &references) {
    std::vector<LinkedAsset> assets;
    if (!BuildAssets(payloads, archive, assets)) {
        return false;
    }
    for (const ReplaySceneBlockPlacement &placement :
         placements.CollisionPlacements()) {
        BindBlock(&placement.Block(), assets, references);
    }
    for (const ReplayScenePylonPlacement &placement :
         placements.PylonPlacements()) {
        BindMobil(&placement.Mobil(), assets, references);
    }
    return BindArchiveModels(archiveModels, assets, archive);
}

bool BuildStaticSceneFromArchive(
        StaticSolidArchiveLoadSession &payloads,
        CGameCtnReplayArchiveStaticModelCollection &archiveModels,
        const ReplaySceneBlockPlacements &placements,
        StaticSceneModelCollection &models) {
    StaticSolidArchiveAssembler archive;
    models.Clear();
    StaticSolidArchiveReferenceCatalog *references =
            payloads.SolidReferences();
    if (references == nullptr ||
        !archive.Assemble(payloads.ArchiveGraph(), payloads) ||
        !LinkStaticSceneAssets(
                payloads,
                archive,
                archiveModels,
                placements,
                *references) ||
        !AddArchiveSceneModels(archiveModels, models) ||
        !AppendBlockPlacementModels(placements, models)) {
        models.Clear();
        return false;
    }
    return true;
}
