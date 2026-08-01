#ifndef TMNF_COLLECTION_SURFACE_REPLACEMENT_PAIRS_H
#define TMNF_COLLECTION_SURFACE_REPLACEMENT_PAIRS_H

#include <stddef.h>
#include <vector>

#include "engine/core/engine_types.h"
class BlockInfoDescriptorExternalRefs;
struct CPlugFilePack;

struct CollectionReplacementPair {
    char sourceSurfaceIdName[64];
    char targetSurfaceIdName[64];
};

class CollectionReplacementPairs {
public:
    static constexpr size_t MaxStoredPairs = 16u;

    int Contains(const char *sourceSurfaceIdName,
                 const char *targetSurfaceIdName) const;
    const std::vector<CollectionReplacementPair> &Pairs() const;
    u32 OverflowCount() const;
    int LoadCatalogCollection(const CPlugFilePack *installedPack,
                              const char *collectionName);

private:
    std::vector<CollectionReplacementPair> pairs;
    u32 overflowCount{};

    void Clear();
    int Append(const char *sourceSurfaceIdName,
               const char *targetSurfaceIdName);
    int ParseSurfaceReplacementPairsChunk(
            const unsigned char *bytes,
            u32 byteCount,
            const BlockInfoDescriptorExternalRefs &refs);
};

#endif
