#pragma once

#include "engine/game/game_ctn_block_info.h"
#include <optional>
#include <vector>

#include "engine/core/cmw_nod.h"
#include "engine/game/game_identifier.h"
#include "engine/core/mw_id.h"
class CGameCtnBlockInfo;
class CGameCtnCatalog;
struct CGameCtnZone;
struct CGameCtnZoneFlat;
struct CGameCtnZoneFrontier;
class CGameCtnCollection : public CMwNod {
public:
    CGameCtnCollection(void);
    ~CGameCtnCollection(void) override;
    CGameCtnZone *GetZone(const CMwId &id);
    CGameCtnZone *GetObsoleteZone(const CMwId &id);
    unsigned long GetZoneIndex(const CMwId &id);
    CGameCtnZoneFlat *GetZoneFlat(const CMwId &id);
    CGameCtnZoneFrontier *GetZoneFrontier(const CMwId &parentZoneId,
                                          const CMwId &childZoneId);
    CGameCtnZoneFlat *GetBasicZone(const CMwId &id);
    CGameCtnZone *GetZoneFromLandBlockInfo(
            const CGameCtnBlockInfo *blockInfo) const;
    void SetCurrentZone(const CMwId &id);
    CGameCtnZone *GetUpToDateZone(CGameCtnZone *zone);
    CGameCtnZoneFrontier *GetFrontier(const CMwId &id);
    void AddReplacementZone(void);
    void RemoveReplacementZone(void);
    void GetDefaultDecoration(SGameCtnIdentifier &outIdentifier);
    void SetDefaultDecoration(SGameCtnIdentifier identifier);

    void AddZone(CGameCtnZone *zone);
    void AddObsoleteZone(CGameCtnZone *zone);
    void SetDefaultZone(CGameCtnZoneFlat *zone);
    CGameCtnZoneFlat *DefaultZone(void) const;
    CGameCtnZone *CurrentZone(void) const;
    void AddSurfaceReplacement(const CMwId &sourceSurfaceId,
                               const CMwId &targetSurfaceId);
    unsigned long SurfaceReplacementIndex(
            const CMwId &sourceSurfaceId,
            const CMwId &targetSurfaceId) const;

private:
    struct SSurfaceReplacement {
        CMwId sourceSurfaceId;
        CMwId targetSurfaceId;
    };

    CMwNodRef<CGameCtnZoneFlat> defaultZone_;
    CGameCtnZone *currentZone_ = nullptr;
    std::vector<CMwNodRef<CGameCtnZone>> zones_;
    std::vector<CMwNodRef<CGameCtnZone>> obsoleteZones_;
    std::vector<SSurfaceReplacement> surfaceReplacements_;
    std::optional<SGameCtnIdentifier> defaultDecoration_;
};
