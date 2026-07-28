#include "engine/game/game_ctn_block_info.h"
#include "engine/resources/block_info_catalog.h"
#include "simulation/replay/replay_challenge_factory.h"
#include "simulation/replay/replay_scene_definition_factory.h"

#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

bool Check(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

class TestBlockInfoAssetRegistry : public BlockInfoAssetRegistry {
public:
    static BlockInfoAssetHandle Handle(u32 index) {
        return HandleForStorageIndex(index);
    }
};

class TestCatalogRepository final : public CatalogAssetRepository {
public:
    TestCatalogRepository()
            : base_(MakeMwNod<CGameCtnBlockInfo>()),
              clip_(MakeMwNod<CGameCtnBlockInfoClip>()),
              known_(MakeMwNod<CGameCtnBlockInfo>()),
              foreign_(MakeMwNod<CGameCtnBlockInfo>()) {
        base_->AddBlock({0u, 0u, 0u}, 0, 0u, 1);
        clip_->SetSourceAsset(ClipAsset());
        AddEntry("Base", "Test", EBlockType::Flat, BaseAsset());
        AddEntry("Clip",
                 "Test",
                 EBlockType::Clip,
                 ClipAsset(),
                 "Test\\ConstructionZone\\Clip.BlockInfo.Gbx");
        AddEntry("Known", "Test", EBlockType::Classic, KnownAsset());
        AddEntry("Foreign", "Other", EBlockType::Classic, ForeignAsset());
    }

    const BlockInfoCatalog *Catalog() override {
        return &catalog_;
    }

    CGameCtnBlockInfo *BlockInfo(BlockInfoAssetHandle asset) override {
        if (asset == BaseAsset()) {
            return base_.Get();
        }
        if (asset == ClipAsset()) {
            return clip_.Get();
        }
        if (asset == KnownAsset()) {
            return known_.Get();
        }
        if (asset == ForeignAsset()) {
            ++foreignAssetRequests_;
            return foreign_.Get();
        }
        return nullptr;
    }

    CSceneMobil *Mobil(BlockInfoAssetHandle, bool, u32) override {
        return nullptr;
    }

    std::optional<std::string> FirstGroundSurface(
            BlockInfoAssetHandle) override {
        return std::nullopt;
    }

    std::optional<CatalogCollectionDefinition> Collection(
            std::string_view name) override {
        if (name != "Test") {
            return std::nullopt;
        }
        CatalogCollectionDefinition collection;
        collection.name = "Test";
        collection.defaultBaseIdentifier = "Base";
        collection.defaultZoneKind = CatalogConstructionZoneKind::Flat;
        CatalogCollectionWaterDefinition water;
        CatalogWaterZoneDefinition zone;
        zone.identifier = "Default";
        zone.blockInfoIdentifier = "Base";
        zone.clipIdentifier = "Clip";
        zone.selectedClipDescriptorPath =
                "Test\\ConstructionZone\\Clip.BlockInfo.Gbx";
        zone.kind = CatalogConstructionZoneKind::Flat;
        water.zones.push_back(std::move(zone));
        collection.water = std::move(water);
        return collection;
    }

    std::optional<CatalogDecorationSizeDefinition> DecorationSize(
            const CGameCtnReplayMapInput &) override {
        CatalogDecorationSizeDefinition decoration;
        decoration.mapSize = {3u, 1u, 3u};
        return decoration;
    }

    bool HasSurfaceReplacement(
            std::string_view,
            std::string_view,
            std::string_view) override {
        return false;
    }

    u32 ForeignAssetRequests() const {
        return foreignAssetRequests_;
    }

private:
    static BlockInfoAssetHandle BaseAsset() {
        return TestBlockInfoAssetRegistry::Handle(0u);
    }

    static BlockInfoAssetHandle ClipAsset() {
        return TestBlockInfoAssetRegistry::Handle(1u);
    }

    static BlockInfoAssetHandle KnownAsset() {
        return TestBlockInfoAssetRegistry::Handle(2u);
    }

    static BlockInfoAssetHandle ForeignAsset() {
        return TestBlockInfoAssetRegistry::Handle(3u);
    }

    void AddEntry(
            const char *identifier,
            const char *collection,
            EBlockType type,
            BlockInfoAssetHandle asset,
            const char *selectedPath = "") {
        BlockInfoCatalogEntry entry;
        entry.identifier = identifier;
        entry.collection = collection;
        entry.selectedDescriptorPath = selectedPath;
        entry.blockType = type;
        entry.asset = asset;
        catalog_.Add(std::move(entry));
    }

    BlockInfoCatalog catalog_;
    CMwNodRef<CGameCtnBlockInfo> base_;
    CMwNodRef<CGameCtnBlockInfoClip> clip_;
    CMwNodRef<CGameCtnBlockInfo> known_;
    CMwNodRef<CGameCtnBlockInfo> foreign_;
    u32 foreignAssetRequests_ = 0u;
};

CGameCtnReplayMapInputBlock Placement(
        u32 ordinal,
        const char *identifier,
        const char *collection,
        GmNat3 coordinate) {
    return CGameCtnReplayMapInputBlock(
            CGameCtnReplayBlockPlacementId(ordinal),
            CGameCtnReplayMapIdentifier(identifier),
            CGameCtnReplayMapIdentifier(collection),
            ECardinalDir::North,
            coordinate,
            BlockPlacementState());
}

bool TestUnresolvedAuthoredBlocksAreSkipped() {
    std::vector<CGameCtnReplayMapInputBlock> blocks;
    blocks.push_back(Placement(0u, "Known", "Test", {0u, 0u, 0u}));
    blocks.push_back(Placement(1u, "Foreign", "Test", {1u, 0u, 1u}));
    blocks.push_back(Placement(2u, "Unknown", "Test", {2u, 0u, 2u}));

    CGameCtnReplayMapInput mapInput;
    if (!CGameCtnReplayMapInput::Create(
                {3u, 1u, 3u},
                CGameCtnReplayMapIdentifier("Test"),
                CGameCtnReplayMapIdentifier(),
                CGameCtnReplayMapIdentifier("Test"),
                CGameCtnReplayMapIdentifier(),
                std::move(blocks),
                &mapInput)) {
        return Check(false, "could not create test map input");
    }

    TestCatalogRepository assets;
    ReplaySceneDefinition scene;
    const bool built = BuildReplaySceneDefinition(mapInput, assets, scene);
    bool okay = Check(
            built, "collection-scoped miss aborted scene construction");
    if (!built) {
        return false;
    }
    okay &= Check(scene.BlockCount() == 1u,
                  "unresolved authored placements were not omitted");
    okay &= Check(scene.FindAuthoredBlock(
                          CGameCtnReplayBlockPlacementId(0u)) != nullptr,
                  "known authored placement was not constructed");
    okay &= Check(scene.FindAuthoredBlock(
                          CGameCtnReplayBlockPlacementId(1u)) == nullptr,
                  "foreign authored placement leaked into the scene");
    okay &= Check(scene.FindAuthoredBlock(
                          CGameCtnReplayBlockPlacementId(2u)) == nullptr,
                  "unknown authored placement leaked into the scene");
    okay &= Check(scene.IsAuthoredBlockSkipped(
                          CGameCtnReplayBlockPlacementId(1u)) &&
                          scene.IsAuthoredBlockSkipped(
                                  CGameCtnReplayBlockPlacementId(2u)) &&
                          scene.SkippedAuthoredBlockCount() == 2u,
                  "skipped placement identities were not retained");
    okay &= Check(assets.ForeignAssetRequests() == 0u,
                  "foreign block asset was loaded after collection mismatch");

    const bool baseAppended = scene.AppendAutomaticBase(mapInput, assets);
    okay &= Check(baseAppended,
                  "automatic base could not be appended to the test scene");
    if (!baseAppended) {
        return false;
    }

    CGameCtnChallengeConstruction construction;
    ReplayChallengeBuildReport report;
    const bool challengeBuilt = BuildReplayChallenge(
            mapInput, scene, construction, report);
    okay &= Check(challengeBuilt,
                  "skipped placement made challenge construction incomplete");
    okay &= Check(report.complete && report.missingBlockCount == 2u,
                  "challenge report did not retain the skipped block count");
    okay &= Check(construction.MissingBlockCount() == 2u,
                  "construction did not store the skipped block count");
    okay &= Check(report.resolvedBlockCount == scene.BlockCount() &&
                          construction.AuthoredBlockCount() == 1u,
                  "challenge did not contain exactly the resolved block");
    return okay;
}

}  // namespace

int main() {
    return TestUnresolvedAuthoredBlocksAreSkipped() ? 0 : 1;
}
