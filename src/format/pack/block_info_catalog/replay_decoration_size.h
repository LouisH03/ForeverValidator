#pragma once

#include <optional>

#include "engine/resources/catalog_asset_repository.h"
struct CGameCtnReplayMapInput;
struct CPlugFilePack;

// Finalize identity-compatible decoration candidates against the replay's
// serialized grid dimensions. A unique candidate is accepted even when its
// archive carries a smaller nominal footprint; multiple candidates are
// ambiguous and rejected.
std::optional<CatalogDecorationSizeDefinition>
SelectReplayDecorationSizeCandidate(
        const std::optional<CatalogDecorationSizeDefinition> &candidate,
        u32 candidateCount,
        const GmNat3 &serializedMapSize);

std::optional<CatalogDecorationSizeDefinition>
DecodeReplayDecorationSizeArchive(
        const unsigned char *archiveBytes,
        u32 archiveByteCount);

std::optional<CatalogDecorationSizeDefinition>
ResolveReplayDecorationSize(
        const CPlugFilePack &pack,
        const CGameCtnReplayMapInput &mapInput);
