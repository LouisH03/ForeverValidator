#pragma once

#include "engine/core/engine_types.h"
#include "engine/game/game_ctn_block_unit.h"
#include "engine/game/game_ctn_block_info.h"
#include <memory>
#include <optional>
#include <string>
#include <array>
#include <vector>

#include "engine/game/block_placement_state.h"
#include "engine/resources/catalog_asset_handle.h"
#include "engine/core/cfast_buffer.h"
#include "engine/core/cmw_nod.h"
#include "engine/core/fast_string.h"
#include "engine/game/game_identifier.h"
#include "engine/core/gm_types.h"
#include "engine/core/mw_id.h"
class CGameCtnBlockInfo;
class CGameCtnBlockUnit;
class CSceneMobil;

struct CGameCtnBlockSkin : CMwNod {
    CFastStringInt label;
    const CFastStringInt &InterfaceLabel(void) const;
};

class CGameCtnBlock : public CMwNod {
public:
    class InstanceId {
    public:
        explicit InstanceId(u32 value) : value_(value) {}
        u32 Value(void) const { return value_; }
        bool operator==(const InstanceId &other) const {
            return value_ == other.value_;
        }

    private:
        u32 value_;
    };

    enum class PlacementOrigin {
        Authored,
        AutomaticBase,
        TerrainModifier,
        AutomaticBaseTerrainModifier
    };

    static constexpr float SquareSize = 32.0f;
    static constexpr float SquareHeight = 8.0f;

    CGameCtnBlock(void);
    CGameCtnBlock(
            CGameCtnBlockInfo *blockInfo,
            GmNat3 coord,
            ECardinalDir direction,
            unsigned long variantOrSource,
            int isGround,
            int createMobilFlag,
            CGameCtnBlockSkin *skin);
    CGameCtnBlock(
            CGameCtnBlockInfo *blockInfo,
            GmNat3 coord,
            ECardinalDir direction,
            BlockPlacementState placement,
            CGameCtnBlockSkin *skin);
    CGameCtnBlock(CGameCtnBlock *source);
    CGameCtnBlock(const CGameCtnBlock &) = delete;
    CGameCtnBlock &operator=(const CGameCtnBlock &) = delete;
    CGameCtnBlock(CGameCtnBlock &&) = delete;
    CGameCtnBlock &operator=(CGameCtnBlock &&) = delete;
    ~CGameCtnBlock(void) override;

    CMwId GetTerrainModifiedId(void);
    EBlockType GetType(void) const;
    void SetSkin(CGameCtnBlockSkin *skin);
    void SetBlockInfo(CGameCtnBlockInfo *blockInfo);
    void SetOldBlockInfo(CGameCtnBlockInfo *blockInfo);
    void ReleaseMobils(void);
    void SetMobilAndHelper(
            CSceneMobil *mobil,
            CSceneMobil *helperMobil,
            const CFastBuffer<CSceneMobil *> *subMobils);
    static void GetMobilLoc(GmIso4 &out,
                            const GmNat3 &coord,
                            ECardinalDir direction,
                            const GmNat3 &size);
    void GetMobilLoc(GmIso4 &out);
    void GetSpawnLoc(GmIso4 &out,
                     unsigned long spawnCount,
                     unsigned long spawnIndex);
    void CreateBlockMobil(int helperFlags, int reserved);
    int IsLandscape(void) const;
    void AllMobilsSetIsVisible(int isVisible);
    void AddBlockUnit(CGameCtnBlockUnit *blockUnit);
    void ApplySkin(void);

    CGameCtnBlockInfo *BlockInfoRef(void) const;
    const SGameCtnIdentifier &Identifier(void) const;
    CGameCtnBlockSkin *Skin(void) const;
    CSceneMobil *MainMobil(void) const;
    CSceneMobil *TriggerMobil(void) const;
    CSceneMobil *HelperMobil(void) const;
    const std::vector<CMwNodRef<CSceneMobil>> &SubMobils(void) const;
    const std::array<CMwNodRef<CSceneMobil>, 4> &ClipSourceMobils(void) const;
    const std::array<CMwNodRef<CSceneMobil>, 4> &
            ClipHelperSourceMobils(void) const;
    const std::array<CMwNodRef<CSceneMobil>, 4> &
            ReplayLoadedClipSourceMobils(void) const;
    const std::vector<std::unique_ptr<CGameCtnBlockUnit>> &BlockUnits(void) const;
    CGameCtnBlockUnit *BlockUnitAt(u32 index) const;
    bool HasBlockUnits(void) const;
    const GmNat3 &BaseCoord(void) const;
    ECardinalDir Direction(void) const;
    u32 VariantIndex(void) const;
    const BlockPlacementState &Placement(void) const;
    int IsType(EBlockType type) const;
    int HasNoMainMobil(void) const;
    GmIso4 MobilLocation(void) const;
    GmIso4 SpawnLocation(unsigned long spawnCount,
                         unsigned long spawnIndex) const;

    void SetSourceAsset(BlockInfoAssetHandle asset,
                        PlacementOrigin origin);
    BlockInfoAssetHandle SourceAsset(void) const;
    PlacementOrigin Origin(void) const;
    void SetMaterialRemaps(bool replacement, bool decorationSkin);
    bool UsesReplacementMaterialRemap(void) const;
    bool UsesDecorationSkinMaterialRemap(void) const;
    bool UsesGroundMobilSize(void) const;
    void SuppressBy(const CGameCtnBlock &block);
    void ClearSuppression(void);
    bool IsSuppressed(void) const;
    const CGameCtnBlock *SuppressingBlock(void) const;
    void SetInstanceId(InstanceId id);
    const std::optional<InstanceId> &BlockInstanceId(void) const;
    void SetMobilVerticalOffset(float offset);
    void SetTriggerMobil(CSceneMobil *mobil);
    void SetClipSourceMobils(
            const std::array<CMwNodRef<CSceneMobil>, 4> &mobils);
    void SetClipHelperSourceMobils(
            const std::array<CMwNodRef<CSceneMobil>, 4> &mobils);
    void SetReplayLoadedClipSourceMobils(
            const std::array<CMwNodRef<CSceneMobil>, 4> &mobils);
    void SetTerrainModifiedId(const CMwId &id);
    void SetCollectionGrid(float squareSize, float squareHeight);
    float CollectionSquareSize(void) const;
    float CollectionSquareHeight(void) const;

private:
    friend class CGameCtnChallenge;

    void ClearMobilOwners(void);

    SGameCtnIdentifier identifier;
    CMwNodRef<CGameCtnBlockSkin> skin;
    CMwNodRef<CGameCtnBlockInfo> blockInfo;
    std::vector<std::unique_ptr<CGameCtnBlockUnit>> blockUnits;
    CMwNodRef<CSceneMobil> mobil;
    CMwNodRef<CSceneMobil> triggerMobil;
    std::vector<CMwNodRef<CSceneMobil>> subMobils;
    CMwNodRef<CSceneMobil> helperMobil;
    std::array<CMwNodRef<CSceneMobil>, 4> clipSourceMobils;
    std::array<CMwNodRef<CSceneMobil>, 4> clipHelperSourceMobils;
    std::array<CMwNodRef<CSceneMobil>, 4> replayLoadedClipSourceMobils;
    GmNat3 coord{};
    ECardinalDir direction = ECardinalDir::North;
    CMwId terrainModifiedId;
    BlockInfoAssetHandle sourceAsset_;
    PlacementOrigin origin_ = PlacementOrigin::Authored;
    bool replacementMaterialRemap_ = false;
    bool decorationSkinMaterialRemap_ = false;
    const CGameCtnBlock *suppressingBlock_ = nullptr;
    std::optional<InstanceId> instanceId_;
    float mobilVerticalOffset_ = 0.0f;
    float collectionSquareSize_ = SquareSize;
    float collectionSquareHeight_ = SquareHeight;
    BlockPlacementState placement_;
};
