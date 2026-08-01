#pragma once

#include "engine/game/game_ctn_block_info.h"
#include "engine/core/cmw_nod.h"
#include "engine/core/mw_id.h"
class CGameCtnBlockInfo;
class CGameCtnBlockInfoPylon;

struct CGameCtnZone : CMwNod {
    CMwId zoneId;
    CMwId surfaceId;
    unsigned long height = 0u;
    unsigned long depth = 0u;
    bool oldZone = false;
    bool hasWater = false;

    CGameCtnZone(void);
    ~CGameCtnZone(void) override;
    virtual CGameCtnBlockInfo *GetZoneInfo(void);
    static CMwNod *MwNewCGameCtnZone(void);
};

struct CGameCtnZoneFlat : CGameCtnZone {
    CMwNodRef<CGameCtnBlockInfo> blockInfoFlat;
    CMwNodRef<CGameCtnBlockInfo> blockInfoClip;
    CMwNodRef<CGameCtnBlockInfo> blockInfoRoad;
    CMwNodRef<CGameCtnBlockInfoPylon> blockInfoPylon;
    bool groundOnly = false;

    CGameCtnZoneFlat(void);
    ~CGameCtnZoneFlat(void) override;
    CGameCtnBlockInfo *GetZoneInfo(void) override;
    static CMwNod *MwNewCGameCtnZoneFlat(void);
};

struct CGameCtnZoneFrontier : CGameCtnZone {
    CMwNodRef<CGameCtnBlockInfo> blockInfoFrontier;
    CMwId parentZoneId;
    CMwId childZoneId;

    CGameCtnZoneFrontier(void);
    ~CGameCtnZoneFrontier(void) override;
    CGameCtnBlockInfo *GetZoneInfo(void) override;
    static CMwNod *MwNewCGameCtnZoneFrontier(void);
};
