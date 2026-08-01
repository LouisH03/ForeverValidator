#pragma once

#include "engine/game/game_ctn_pylon_column.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "engine/core/gm_types.h"
class CGameCtnPylonColumn;

class CGameCtnPylonColumnGrid {
public:
    CGameCtnPylonColumnGrid(void);
    ~CGameCtnPylonColumnGrid(void);

    CGameCtnPylonColumnGrid(const CGameCtnPylonColumnGrid &) = delete;
    CGameCtnPylonColumnGrid &operator=(
            const CGameCtnPylonColumnGrid &) = delete;

    void Reset(std::uint32_t width, std::uint32_t depth);
    void Clear(void);

    CGameCtnPylonColumn *North(GmNat3 coord) const;
    CGameCtnPylonColumn *South(GmNat3 coord) const;
    CGameCtnPylonColumn *East(GmNat3 coord) const;
    CGameCtnPylonColumn *West(GmNat3 coord) const;

    std::size_t NorthCount(void) const;
    std::size_t SouthCount(void) const;
    std::size_t EastCount(void) const;
    std::size_t WestCount(void) const;

private:
    using Columns = std::vector<std::unique_ptr<CGameCtnPylonColumn>>;

    static void CreateColumns(Columns &columns, std::size_t count);

    std::uint32_t width_ = 0u;
    std::uint32_t depth_ = 0u;
    Columns north_;
    Columns south_;
    Columns east_;
    Columns west_;
};
