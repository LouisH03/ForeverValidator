#pragma once

#include "engine/core/engine_types.h"
#include "engine/game/game_ctn_collector.h"
#include "engine/game/game_ctn_decoration.h"
#include "engine/game/game_ctn_pylon_column.h"
#include "engine/game/game_ctn_field_unit.h"
#include "engine/game/game_ctn_block_unit.h"
#include "engine/game/game_ctn_block_info.h"
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "engine/core/cmw_nod.h"
#include "engine/game/ctn_forced_mods.h"
#include "engine/game/game_ctn_block.h"
#include "engine/game/game_ctn_pylon_column_grid.h"
#include "engine/game/game_ctn_zone.h"
#include "engine/game/game_identifier.h"
#include "engine/core/gm_types.h"
#include "engine/scene/scene_mobil.h"
class CGameCtnBlockInfo;
class CGameCtnBlockInfoClip;
class CGameCtnBlockInfoPylon;
class CGameCtnBlockUnit;
class CGameCtnBlockUnitInfo;
class CGameCtnChapter;
class CGameCtnCatalog;
class CGameCtnCollection;
class CGameCtnCollectorList;
class CGameCtnDecoration;
class CGameCtnFieldUnit;
class CGameCtnPylonColumn;
struct CSystemPackDesc;

using FieldUnitColumn = std::vector<std::unique_ptr<CGameCtnFieldUnit>>;

struct SZoneGenealogyEntry {
    CMwId zoneId;
    uint8_t zoneHeight = 0u;
};

struct SZoneGenealogy {
    std::vector<SZoneGenealogyEntry> ancestors;
    int operator==(const SZoneGenealogy &other) const;
};

struct ChallengeZoneCell {
    CMwId zoneId;
    uint8_t height = 0u;
    std::optional<SZoneGenealogy> genealogy;
};

struct SPylonMobil {
    struct SSingleMobil {
        explicit SSingleMobil(CSceneMobil &mobil) : mobil_(&mobil) {}
        CSceneMobil &Mobil(void) const { return *mobil_.Get(); }

    private:
        CMwNodRef<CSceneMobil> mobil_;
    };

    struct STripleMobils {
        STripleMobils(CSceneMobil &lower,
                      CSceneMobil &middle,
                      CSceneMobil &upper)
            : lower_(&lower), middle_(&middle), upper_(&upper) {}
        CSceneMobil &Lower(void) const { return *lower_.Get(); }
        CSceneMobil &Middle(void) const { return *middle_.Get(); }
        CSceneMobil &Upper(void) const { return *upper_.Get(); }

    private:
        CMwNodRef<CSceneMobil> lower_;
        CMwNodRef<CSceneMobil> middle_;
        CMwNodRef<CSceneMobil> upper_;
    };

    SPylonMobil(CSceneMobil &mobil,
                GmNat3 coord,
                unsigned long pylonIndex,
                unsigned long pylonHeight);
    SPylonMobil(CSceneMobil &lower,
                CSceneMobil &middle,
                CSceneMobil &upper,
                GmNat3 coord,
                unsigned long pylonIndex,
                unsigned long pylonHeight);
    bool IsSingleMobil(void) const;
    CSceneMobil &SingleMobil(void) const;
    const STripleMobils &TripleMobils(void) const;

    GmNat3 coord;
    unsigned long pylonIndex;
    unsigned long pylonHeight;
    std::variant<SSingleMobil, STripleMobils> mobils;
};

class CGameCtnChallenge : public CMwNod {
private:
    std::vector<FieldUnitColumn> fieldUnitColumns;
    CGameCtnPylonColumnGrid pylonColumns;
    std::vector<ChallengeZoneCell> zoneCells;
    std::deque<std::reference_wrapper<CGameCtnBlock>> blocksToAdd;
    std::deque<SPylonMobil> pylonsToAdd;
    std::deque<CMwNodRef<CSceneMobil>> mobilsToRemove;
    CMwNodRef<CGameCtnCollection> collection;
    CMwNodRef<CGameCtnCatalog> catalog;
    CMwNodRef<CSystemPackDesc> modPackDesc;
    CMwId collectionId;
    u32 mapWidth = 0u;
    u32 mapHeight = 0u;
    u32 mapDepth = 0u;
    float collectionSquareSize_ = 32.0f;
    float collectionSquareHeight_ = 8.0f;
    u32 blockMobilUpdateSerial_ = 0u;
    bool hasTerrainData = false;
    bool hasPylonData = false;
    SGameCtnIdentifier decorationId;
    std::unique_ptr<CGameCtnCollectorList> terrainBlocksCollectorList;
    CMwNodRef<CGameCtnDecoration> decoration;
    std::unique_ptr<CGameCtnCollectorList> terrainPylonsCollectorList;
    u32 defaultZoneHeight = 0u;

    std::vector<CMwNodRef<CGameCtnBlockInfo>> ownedBlockInfos_;
    std::vector<std::unique_ptr<CGameCtnBlock>> blocks_;
    std::vector<std::unique_ptr<CGameCtnBlock>> retiredBlocks_;

    struct BlockSuppressionCandidate {
        CGameCtnBlock *target = nullptr;
        const CGameCtnBlock *suppressor = nullptr;
    };
    std::vector<BlockSuppressionCandidate> blockSuppressionCandidates_;

    bool IsActiveBlock(const CGameCtnBlock *block) const;
    void RebindSuppressedBlocksAfterRemoval(CGameCtnBlock *block);

public:
    CGameCtnChallenge(void);
    ~CGameCtnChallenge(void) override;

    void SetMapSize(const GmNat3 &size);
    void SetCollectionGrid(float squareSize, float squareHeight);
    GmNat3 MapSize(void) const;
    u32 MapHeight(void) const;
    float CollectionSquareSize(void) const;
    float CollectionSquareHeight(void) const;
    void ConfigureReplayConstructionData(bool terrainData, bool pylonData);
    bool HasReplayPylonData(void) const;
    void CreateBlocksAndPylons(void);

    virtual void InitChallengeData(const SCtnForcedMods *forcedMods);
    void FastInitChallengeData(CGameCtnCollection *collection,
                               CGameCtnDecoration *decoration);
    CGameCtnBlock *CreateBlock(CGameCtnBlockInfo *blockInfo,
                               GmNat3 coord,
                               ECardinalDir direction,
                               unsigned long variantOrSource,
                               int isGround,
                               int createMobilFlag,
                               CGameCtnBlockSkin *skin);
    void CreateMobilForBlock(CGameCtnBlock *block);
    void CreateMobilForClip(CGameCtnBlock *block);
    int UpdateClip(GmNat3 coord);

    unsigned char GetNeighbourPylonIndex(unsigned char index);
    CGameCtnPylonColumn *GetColumnNorth(GmNat3 coord);
    CGameCtnPylonColumn *GetColumnSouth(GmNat3 coord);
    CGameCtnPylonColumn *GetColumnEast(GmNat3 coord);
    CGameCtnPylonColumn *GetColumnWest(GmNat3 coord);
    CGameCtnPylonColumn *GetColumn(GmNat3 coord,
                                  ECardinalDir direction,
                                  int isEvenSide);
    CGameCtnPylonColumn *GetColumn(GmNat3 coord,
                                  unsigned long pylonIndex);
    CSceneMobil *CreateNewPylonMobil(CGameCtnBlockInfoPylon *pylonBlockInfo,
                                     unsigned long variantIndex);
    void UpdateColumnMobils(GmNat3 coord);
    void UpdatePylons(GmNat3 coord);
    int IsEditableCoord(GmNat3 coord);
    CGameCtnFieldUnit *GetFieldUnit(GmNat3 coord);
    void RemoveFieldUnit(GmNat3 coord);
    CGameCtnBlock *GetBlockFromPlayField(GmNat3 coord);
    CGameCtnBlockUnit *GetBlockUnitFromPlayField(GmNat3 coord);
    CGameCtnBlockUnitInfo *GetBlockUnitInfoFromPlayField(GmNat3 coord);
    const CMwId &GetZoneId(GmNat3 coord);
    unsigned char GetZoneHeight(GmNat3 coord);
    void SetZoneHeight(GmNat3 coord, unsigned long height);
    void InitializeZoneGrid(const CMwId &zoneId,
                            unsigned long height,
                            bool genealogyEnabled);
    void ResetZoneGrid(void);
    void InitializeFieldUnitGrid(void);
    void ResetFieldUnitGrid(void);
    int ContainsCoord(GmNat3 coord) const;
    u32 GridIndex(u32 x, u32 z) const;
    u32 ColumnModifierFirstScanY(GmNat3 coord);
    CGameCtnBlockUnitInfo *BlockUnitInfoAtCoord(GmNat3 coord);
    CGameCtnBlockUnitInfo *FindColumnTerrainModifierUnit(GmNat3 coord);
    CGameCtnZone *ZoneForId(const CMwId &id) const;
    CGameCtnZone *RealZoneForBlock(CGameCtnBlock *block) const;
    int IsFrontierZoneAt(GmNat3 coord);
    int IsGroundZoneAt(GmNat3 coord);
    void SetFieldTerrain(CGameCtnFieldUnit *fieldUnit, ETerrain terrain);
    CGameCtnFieldUnit *EnsureFieldUnit(GmNat3 coord, ETerrain terrain);
    void RemoveGroundBlockAt(u32 x, u32 z);
    GmNat3 AddBlockCoordOffset(const CGameCtnBlock &block,
                               const GmNat3 &offset) const;
    GmNat3 RotatedBlockUnitOffset(CGameCtnBlockInfo &blockInfo,
                                  u32 index,
                                  ECardinalDir direction,
                                  int isGround);
    GmNat3 AbsoluteBlockUnitCoord(const CGameCtnBlock &block,
                                  const GmNat3 &offset) const;
    int IsEditableBlockUnitCoord(const CGameCtnBlock &block,
                                 const GmNat3 &offset);
    void UpdatePylonsForBlockUnit(CGameCtnBlockUnit *unit);
    u32 AdjustRoadJunctionMask(const CGameCtnBlock &block, u32 junctionMask);
    CGameCtnBlockUnit *CreatePlacedBlockUnit(CGameCtnBlock *block,
                                             CGameCtnBlockUnitInfo *unitInfo,
                                             CGameCtnFieldUnit *fieldUnit,
                                             GmNat3 localOffset,
                                             u32 junctionMask,
                                             u32 helperMask);
    CGameCtnBlockUnit *AttachPlacedBlockUnit(CGameCtnBlock *block,
                                             CGameCtnBlockUnitInfo *unitInfo,
                                             CGameCtnFieldUnit *fieldUnit,
                                             GmNat3 localOffset,
                                             u32 junctionMask,
                                             u32 helperMask);
    CGameCtnZone *GetZone(GmNat3 coord);
    SZoneGenealogy *GetZoneGenealogy(GmNat3 coord);
    CGameCtnZone *GetRealZone(GmNat3 coord);
    const CMwId &GetRealZoneId(GmNat3 coord);
    ETerrain GetTerrainFromPlayField(GmNat3 coord);
    CGameCtnBlock *GetBlockFromAddList(void);
    void RemoveBlockFromAddList(void);
    SPylonMobil *GetPylonFromAddList(void);
    void RemovePylonFromAddList(void);
    CSceneMobil *GetMobilFromRemoveList(void);
    void RemoveMobilFromRemoveList(void);
    CGameCtnFieldUnit *CreateFieldUnit(GmNat3 coord, ETerrain terrain);
    void SetZoneId(GmNat3 coord, const CMwId &id);
    CMwId GetColumnModifierId(GmNat3 coord);
    void AddBlockToAddList(CGameCtnBlock *block);
    void AddPylonToAddList(CSceneMobil *mobil,
                           GmNat3 coord,
                           unsigned long pylonIndex,
                           unsigned long pylonHeight);
    void AddPylonToAddList(CSceneMobil *lower,
                           CSceneMobil *middle,
                           CSceneMobil *upper,
                           GmNat3 coord,
                           unsigned long pylonIndex,
                           unsigned long pylonHeight);
    void AddMobilToRemoveList(CSceneMobil *mobil);
    void AddBlockToRemoveList(CGameCtnBlock *block);
    void ReplaceBlock(CGameCtnBlock *block);
    int RemoveBlock(CGameCtnBlock *block);
    void UpdateBlockMobils(void);
    int IsBlockOnGround(CGameCtnBlockInfo *blockInfo,
                        GmNat3 coord,
                        ECardinalDir direction);
    unsigned long GetReplacementIndex(CMwId firstId, CMwId secondId);
    CGameCtnChapter *GetChapter(void) const;
    CSystemPackDesc *GetModPackDesc(const SCtnForcedMods *forcedMods);
    void LoadDecorationAndCollection(const SCtnForcedMods *forcedMods);
    int CheckTerrainBlock(CGameCtnBlock *block);
    CGameCtnBlockInfo *GetDescFromBlock(CGameCtnBlock *block);
    int UpdateFieldUnits(CGameCtnBlock *block);
    CGameCtnBlockInfoClip *GetRequiredBlockUnitJunction(
            GmNat3 coord,
            ECardinalDir direction);
    CGameCtnBlockInfoClip *GetRequiredNeighbourBlockUnitJunction(
            GmNat3 coord,
            ECardinalDir direction);
    CGameCtnBlockInfoClip *GetNeighbourBlockUnitJunction(
            GmNat3 coord,
            ECardinalDir direction);
    int CanJoinRoadBlock(CGameCtnBlock *block, ECardinalDir direction);
    int IsFullUnderground(GmNat3 coord);
    int IsOccluding(const GmNat3 &coord);
    int IsTunnelOccludedCardinal(GmNat3 coord, ECardinalDir direction);
    int IsTunnelOccludedAltitude(GmNat3 coord, int upward);
    CGameCtnBlock *GetGroundBlock(GmNat3 coord);

    CGameCtnBlockInfo &OwnBlockInfo(std::unique_ptr<CGameCtnBlockInfo> blockInfo);
    CGameCtnBlock *AddLoadedBlock(std::unique_ptr<CGameCtnBlock> block);
    const std::vector<std::unique_ptr<CGameCtnBlock>> &Blocks(void) const;
    void RegisterBlockSuppression(CGameCtnBlock *target,
                                  const CGameCtnBlock *suppressor);
    void CopyBlockSuppressionCandidates(CGameCtnBlock *target,
                                        const CGameCtnBlock *source);
    u32 BlockMobilUpdateSerial(void) const;
    void ReleaseRetiredBlockOwnersAfterMobilRemoval(void);
    void ReleaseRetiredBlockOwnersAfterMobilRemovalIf(
            const std::function<bool(const CGameCtnBlock &)> &canRelease);
    void ClearConstructedBlocks(void);

};
