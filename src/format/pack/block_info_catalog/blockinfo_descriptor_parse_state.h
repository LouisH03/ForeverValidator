#pragma once

#include "engine/core/engine_types.h"
#include <array>
#include <optional>
#include <string>
#include <vector>

#include "format/pack/block_info_catalog/blockinfo_descriptor_unit_geometry.h"
#include "format/archive/archive_node_reference.h"
#include "engine/core/gm_types.h"
struct BlockInfoDescriptorMobilParser;

struct BlockInfoParsedHmsState {
    u32 hasItemState = 0u;
    u32 physicsFlags = 0u;
    u32 renderingFlags = 0u;
    u32 visibilityFlags = 0u;
    u32 isRaceTriggerMobil = 0u;
};

struct BlockInfoParsedMobilCounts {
    std::array<u32, 64> airVariantCount{};
    std::array<u32, 64> groundVariantCount{};

    void Init();
    u32 SetVariantCount(u32 family, u32 variant, u32 count);
};

struct BlockInfoParsedMobilSlot {
    u32 selectorGroup = 0u;
    u32 variantIndex = 0u;
    u32 mobilIndex = 0u;
};

struct BlockInfoDescriptorParseResult {
    std::string descriptorPath;
    BlockInfoParsedMobilCounts counts;

private:
    friend class BlockInfoDescriptorMobilParser;
    std::optional<BlockInfoDescriptorUnitGeometry> unitGeometry;
    std::vector<BlockInfoDescriptorUnitDefinition> groundUnitDefinitions;
    std::vector<BlockInfoDescriptorUnitDefinition> airUnitDefinitions;

public:
    u32 descriptorClassId = 0u;
    u32 mobilArg1Count = 0u;
    u32 mobilArg0Count = 0u;
    u32 blockInfoRespawnUsesCurrentTransform = 0u;
    u32 hasBlockInfoIso4A = 0u;
    u32 hasBlockInfoIso4B = 0u;
    float blockInfoIso4A[12]{};
    float blockInfoIso4B[12]{};
    std::string helperGroundDescriptorPath;
    std::string helperAirDescriptorPath;
    std::string helperCommonDescriptorPath;
    std::array<std::optional<BlockInfoParsedMobilSlot>, 3>
            pylonSourceMobils{};
    bool parsedPylonSourceChunk = false;

    GmInt3 SizeWhenGround() const;
    GmInt3 SizeWhenAir() const;
    const std::vector<BlockInfoDescriptorUnitDefinition> &
    UnitDefinitionsForFieldUnits(bool ground) const;
};

struct BlockInfoParsedMobil {
    std::string descriptorPath;
    u32 selectorGroup = 0u;
    u32 variantIndex = 0u;
    u32 mobilIndex = 0u;
    u32 loadable = 0u;
    u32 hasBlockInfoIso4A = 0u;
    u32 hasBlockInfoIso4B = 0u;
    u32 hasHmsItemState = 0u;
    u32 hmsItemPhysicsFlags = 0u;
    u32 hmsItemRenderingFlags = 0u;
    u32 hmsItemVisibilityFlags = 0u;
    u32 isRaceTriggerMobil = 0u;
    u32 motionChangesLocations = 0u;
    u32 collisionUsesInitialTransform = 0u;
    u32 isSceneObjectLinkMobil = 0u;
    u32 hasSceneLinkIso4 = 0u;
    float blockInfoIso4A[12]{};
    float blockInfoIso4B[12]{};
    float sceneLinkIso4[12]{};
    std::string modelName;
    std::string plainPackPath;
    std::string hashedDescriptorPath;
};

struct BlockInfoParsedMobilAlias {
    BlockInfoParsedMobilSlot source;
    BlockInfoParsedMobilSlot target;
};

class BlockInfoParsedMobilCollection {
public:
    int Append(const BlockInfoParsedMobil &mobil);
    int RecordArchiveLocalRef(u32 archiveNodeIndex,
                              const BlockInfoParsedMobilSlot &slot,
                              bool hasInlineSceneMobil);
    u32 Count() const;
    const BlockInfoParsedMobil *ModelAt(u32 index) const;
    const std::vector<BlockInfoParsedMobilAlias> &ArchiveLocalAliases() const;
    std::optional<BlockInfoParsedMobilSlot> ResolveArchiveLocalSource(
            ArchiveNodeReference sourceNode) const;
    void ApplyHmsState(u32 firstMobil,
                       const BlockInfoParsedHmsState &hmsState);
    void ApplyHmsState(const char *descriptorPath,
                       u32 selectorGroup,
                       u32 variantIndex,
                       u32 mobilIndex,
                       const BlockInfoParsedHmsState *hmsState);
    void ApplySceneLinkIso4ToAppendedRaceTriggerModels(
            u32 firstMobil,
            const float iso[12]);
    void MarkAppendedRaceTrigger(u32 firstMobil);
    void MarkAppendedSceneObjectLink(u32 firstMobil);
    void MarkAppendedMotionChangesLocations(u32 firstMobil);
    void MarkAppendedCollisionUsesInitialTransform(u32 firstMobil);

private:
    struct ArchiveLocalNode {
        u32 archiveNodeIndex = 0u;
        BlockInfoParsedMobilSlot source;
        bool hasInlineSceneMobil = false;
    };

    std::vector<BlockInfoParsedMobil> mobils;
    std::vector<ArchiveLocalNode> archiveLocalNodes;
    std::vector<BlockInfoParsedMobilAlias> archiveLocalAliases;
};
