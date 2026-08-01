#ifndef TMNF_BLOCKINFO_DESCRIPTOR_UNIT_GEOMETRY_H
#define TMNF_BLOCKINFO_DESCRIPTOR_UNIT_GEOMETRY_H

#include "engine/core/engine_types.h"
#include <array>
#include <stdint.h>
#include <string>
#include <vector>

#include "engine/core/gm_types.h"
#include "format/archive/mw_id_archive_codec.h"
class CGameCtnReplayMapInputBlock;
struct BlockInfoSizeParseStream;

struct BlockInfoDescriptorUnitOffset {
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;
};

struct BlockInfoDescriptorUnitPlacement {
    BlockInfoDescriptorUnitOffset offset;
    bool underground = false;
};

struct BlockInfoDescriptorUnitDefinition {
    BlockInfoDescriptorUnitOffset offset;
    u32 helperMask = 0u;
    u32 junctionMask = 0u;
    bool underground = false;
    bool hasHelperMask = false;
    bool hasJunctionMask = false;
    bool hasUnderground = false;
    TmnfFormat::ArchiveIdentifierValue terrainModifierId;
    std::string surfaceIdName;
    std::array<std::string, 4> sourceDescriptorPaths;
};

struct BlockInfoDescriptorUnitDimensions {
    u32 x = 1u;
    u32 y = 1u;
    u32 z = 1u;

    GmInt3 Size() const;
};

struct BlockInfoDescriptorUnitLayout {
    void Clear();
    int IsDefined() const;
    int HasUnits() const;
    GmInt3 Size() const;
    const std::vector<BlockInfoDescriptorUnitPlacement> &Units() const;

private:
    friend class BlockInfoDescriptorUnitGeometry;
    friend struct BlockInfoSizeParseStream;

    bool defined = false;
    BlockInfoDescriptorUnitDimensions dimensions;
    std::vector<BlockInfoDescriptorUnitPlacement> units;
};

class BlockInfoDescriptorUnitGeometry {
public:
    void Clear();
    int ParseGbxDescriptorBytes(const unsigned char *bytes, u32 byteCount);
    GmInt3 SizeWhenGround() const;
    GmInt3 SizeWhenAir() const;
    const BlockInfoDescriptorUnitLayout &GroundLayout() const;
    const BlockInfoDescriptorUnitLayout &AirLayout() const;

private:
    BlockInfoDescriptorUnitLayout groundLayout;
    BlockInfoDescriptorUnitLayout airLayout;
    u32 unresolvedUnitRefCount = 0u;
};

#endif
