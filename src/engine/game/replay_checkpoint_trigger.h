#ifndef TMNF_REPLAY_CHECKPOINT_TRIGGER_H
#define TMNF_REPLAY_CHECKPOINT_TRIGGER_H

#include "engine/core/engine_types.h"
#include "engine/game/game_ctn_block_info.h"
#include <memory>
#include <stddef.h>

struct CHmsItem;
struct CHmsPhysicalContact;

class ReplayCheckpointContactObserver {
public:
    virtual ~ReplayCheckpointContactObserver() = default;
    virtual void OnCheckpointContact(
            CHmsItem &triggerItem,
            CHmsPhysicalContact &contact) = 0;
};

class CGameCtnReplayCheckpointMobil : public CSceneMobil {
public:
    void SetContactObserver(ReplayCheckpointContactObserver *observer);
    void AbsorbContact(CHmsPhysicalContact &contact) override;

private:
    ReplayCheckpointContactObserver *contactObserver = nullptr;
};

class CGameCtnReplayCheckpointTrigger {
public:
    // Replay-owned checkpoint block and mobil used by the race event flow.
    // Race-only identity belongs to this owner rather than the shared block.
    struct RaceCheckpointIdentity;

    struct RaceCheckpointIdentity {
        BlockRaceRole raceRole = BlockRaceRole::None;
        u32 raceBlockId = 0u;
        bool respawnUsesCurrentTransform = false;

        static RaceCheckpointIdentity FromRaceBlock(
                BlockRaceRole raceRole,
                u32 raceBlockId,
                bool respawnUsesCurrentTransform);
    };

    void Reset();
    void PrepareOwner(const RaceCheckpointIdentity &identity,
                      ReplayCheckpointContactObserver &observer);

    CSceneMobil *MobilOwner();
    const CSceneMobil *MobilOwner() const;
    CGameCtnBlock *BlockOwner();
    const CGameCtnBlock *BlockOwner() const;
    int OwnsBlock(const CGameCtnBlock *block) const;
    const RaceCheckpointIdentity &Identity() const;
private:
    CMwNodRef<CGameCtnReplayCheckpointMobil> mobil;
    CMwNodRef<CGameCtnBlockInfo> blockInfo;
    std::unique_ptr<CGameCtnBlock> block;
    RaceCheckpointIdentity raceIdentity;
};

#endif
