#ifndef TMNF_BLOCKINFO_ARCHIVE_STREAM_H
#define TMNF_BLOCKINFO_ARCHIVE_STREAM_H

#include "engine/core/engine_types.h"
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

#include "format/pack/block_info_catalog/blockinfo_descriptor_unit_geometry.h"
#include "format/archive/archive_node_reference.h"
#include "format/archive/mw_id_archive_codec.h"
class BlockInfoDescriptorExternalRefs;

struct ArchiveLocalCMwIdEntry {
    std::string text;
};

struct ArchiveLocalCMwIdTable {
    std::vector<ArchiveLocalCMwIdEntry> entries;
    size_t maxStoredEntries{};

    int Append(const char *name);
    const ArchiveLocalCMwIdEntry *FindOneBased(u32 index) const;
};

struct BlockInfoSizeParseStream {
    const unsigned char *bytes;
    u32 byteCount;
    u32 offset;
    u32 cmwIdMode;
    u32 hasCmwIdMode;
    u32 unresolvedUnitRefCount;
    ArchiveLocalCMwIdTable cmwIdTable;
    const BlockInfoDescriptorExternalRefs *blockInfoSourceRefs;

    void SetBlockInfoSourceRefs(const BlockInfoDescriptorExternalRefs *refs);
    int ReadU32(u32 *valueOut);
    int ReadU8(u32 *valueOut);
    int ReadF32(float *valueOut);
    int ReadIso4(float iso[12]);
    int ReadU16(u32 *valueOut);
    int Skip(u32 byteCount);
    int SkipString();
    int ReadStringOwned(std::string *out);
    int ReadStringText(char *out, size_t outSize);
    int SkipCMwIdPayload(u32 encodedValue);
    int SkipCMwId();
    int ReadCMwIdText(char *out, size_t outSize);
    int ReadCMwIdEncodedText(u32 *encodedOut, char *out, size_t outSize);
    int ReadCMwIdValue(TmnfFormat::ArchiveIdentifierValue *valueOut);
    int ReadArchiveNodeRef(ArchiveNodeReference *refOut);
    int SkipNodRefPayload(ArchiveNodeReference sourceNode);
    int SkipCountedNodRefs();
    static int WordIsSceneObjectLinkChunk(u32 word);
    int ParseInlineMotionArchive(ArchiveNodeReference rootNode,
                                 u32 rootClassId,
                                 int *changesLocationsOut = nullptr,
                                 int *collisionUsesInitialTransformOut = nullptr);
    int ParseInlineCMotionsArchive(ArchiveNodeReference rootNode,
                                   int *changesLocationsOut = nullptr,
                                   int *collisionUsesInitialTransformOut = nullptr);
    int ReadBlockInfoSourceRef(char *descriptorOut, size_t descriptorOutSize);
    int SkipMobilRefArray();
    int ParseUnitNode(u32 outOffset[3],
                      BlockInfoDescriptorUnitDefinition *unitOut);
    int SkipNodRef();
    int ReadUnitRef(u32 outOffset[3],
                    u32 *hasOffsetOut,
                    BlockInfoDescriptorUnitDefinition *unitOut);
    int ReadUnitLayout(
            BlockInfoDescriptorUnitLayout *layoutOut,
            std::vector<BlockInfoDescriptorUnitDefinition> *definitionsOut);
    int SkipCommonPrefix();
    int ReadStringFixed(char *out, size_t outSize);
};

#endif
