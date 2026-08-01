#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/resources/catalog_asset_handle.h"
#include "engine/core/cmw_nod.h"
#include "engine/game/game_ctn_block_info.h"
struct CPlugFilePack;
class CSceneMobil;
class StaticSolidArchiveReferenceCatalog;

class BlockInfoAssetStore : private BlockInfoAssetRegistry {
public:
    BlockInfoAssetStore(CPlugFilePack &pack,
                        StaticSolidArchiveReferenceCatalog &solidReferences);
    BlockInfoAssetStore(const BlockInfoAssetStore &) = delete;
    BlockInfoAssetStore &operator=(const BlockInfoAssetStore &) = delete;

    BlockInfoAssetHandle Register(std::string_view archivePath);
    CGameCtnBlockInfo *Load(BlockInfoAssetHandle asset,
                            bool includeArchiveModels = false);
    CSceneMobil *Mobil(BlockInfoAssetHandle asset,
                       bool ground,
                       u32 variant);
    std::optional<std::string> FirstGroundSurface(
            BlockInfoAssetHandle asset);
    const char *LastLoadFailureReason() const;

private:
    struct Asset {
        std::string archivePath;
        CMwNodRef<CGameCtnBlockInfo> blockInfo;
        CMwNodRef<CGameCtnBlockInfo> archiveBlockInfo;
    };

    CPlugFilePack &pack;
    StaticSolidArchiveReferenceCatalog &solidReferences;
    std::vector<Asset> assets;
    const char *lastLoadFailureReason = "none";
};
