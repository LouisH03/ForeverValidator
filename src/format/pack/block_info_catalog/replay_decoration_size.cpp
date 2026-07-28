#include "format/pack/block_info_catalog/replay_decoration_size.h"

#include <new>
#include <string>
#include <string_view>
#include <utility>

#include "engine/game/replay_challenge_map_input.h"
#include "format/archive/archive_binary.h"
#include "format/archive/archive_class_ids.h"
#include "format/archive/classic_archive_reader.h"
#include "format/archive/tmnf_archive_ids.h"
#include "format/archive/tmnf_gbx_body_reader.h"
#include "format/pack/installed/byte_buffer.h"
#include "format/pack/installed/plug_file_pack.h"

namespace {

using TmnfFormat::ArchiveBinary::ReadU32LE;

constexpr u32 DecorationSizeChunkPayloadByteCount = 20u;
constexpr u32 CollectorChunk0301A006PayloadByteCount = 4u;
constexpr u32 CollectorChunk0301A007PayloadByteCount = 24u;
constexpr u32 CollectorChunk0301A009PayloadByteCount = 20u;
constexpr u32 DecorationIdentityHeaderChunk = 0x0301a003u;
constexpr u32 HeaderChunkByteCountMask = 0x7fffffffu;
constexpr u32 HeaderChunkCountLimit = 50u;
constexpr std::string_view DecorationSizeSuffix =
        ".TMDecorationSize.Gbx";

struct DecorationIdentity {
    std::string identifier;
    std::string collection;
    std::string author;
    u32 archiveIdentifierMode = 0u;
};

struct DecodedDecorationSizeBody {
    CatalogDecorationSizeDefinition definition;
    u32 baseSceneNodeIndex = UINT32_MAX;
    u32 endOffset = 0u;
};

struct DecodedDecorationSizeArchive {
    CatalogDecorationSizeDefinition definition;
    GbxBodyReferenceTable references;
    u32 baseSceneNodeIndex = UINT32_MAX;
};

bool EndsWith(std::string_view text, std::string_view suffix) {
    return suffix.size() <= text.size() &&
           text.substr(text.size() - suffix.size()) == suffix;
}

bool SkipBytes(u32 archiveByteCount, u32 *offset, u32 byteCount) {
    if (offset == nullptr || *offset > archiveByteCount ||
        byteCount > archiveByteCount - *offset) {
        return false;
    }
    *offset += byteCount;
    return true;
}

bool IdentityMatches(
        const DecorationIdentity &identity,
        const CGameCtnReplayMapInput &mapInput) {
    return identity.identifier == mapInput.DecorationMood().Name() &&
           identity.collection == mapInput.DecorationCollection().Name() &&
           identity.author == mapInput.DecorationAuthor().Name();
}

bool ReadDecorationIdentity(
        const unsigned char *archiveBytes,
        u32 archiveByteCount,
        DecorationIdentity *out,
        TmnfFormat::ClassicArchiveReader *readerOut = nullptr) {
    if (archiveBytes == nullptr || out == nullptr || archiveByteCount < 21u) {
        return false;
    }
    const u32 headerByteCount = ReadU32LE(archiveBytes + 13u);
    if (headerByteCount < 4u || headerByteCount > archiveByteCount - 17u) {
        return false;
    }
    const u32 headerEnd = 17u + headerByteCount;
    const u32 chunkCount = ReadU32LE(archiveBytes + 17u);
    if (chunkCount > HeaderChunkCountLimit ||
        chunkCount > (headerByteCount - 4u) / 8u) {
        return false;
    }
    const u32 directoryEnd = 21u + chunkCount * 8u;
    u32 payloadOffset = directoryEnd;
    u32 identityOffset = 0u;
    u32 identityByteCount = 0u;
    for (u32 index = 0u; index < chunkCount; ++index) {
        const u32 recordOffset = 21u + index * 8u;
        const u32 chunkId = ReadU32LE(archiveBytes + recordOffset);
        const u32 payloadByteCount =
                ReadU32LE(archiveBytes + recordOffset + 4u) &
                HeaderChunkByteCountMask;
        if (payloadOffset > headerEnd ||
            payloadByteCount > headerEnd - payloadOffset) {
            return false;
        }
        if (chunkId == DecorationIdentityHeaderChunk) {
            if (identityByteCount != 0u || payloadByteCount < 4u) {
                return false;
            }
            identityOffset = payloadOffset;
            identityByteCount = payloadByteCount;
        }
        payloadOffset += payloadByteCount;
    }
    if (payloadOffset != headerEnd || identityByteCount == 0u) {
        return false;
    }

    const u32 archiveIdentifierMode =
            ReadU32LE(archiveBytes + identityOffset);
    if (archiveIdentifierMode < 1u || archiveIdentifierMode > 3u) {
        return false;
    }

    TmnfFormat::ClassicArchiveReader reader;
    TmnfFormat::ClassicArchiveIdentifier parts[3];
    if (!reader.Open(archiveBytes, archiveByteCount, 512u) ||
        !reader.Seek(identityOffset) || !reader.ReadIdentifier(parts[0]) ||
        !reader.ReadIdentifier(parts[1]) ||
        !reader.ReadIdentifier(parts[2]) ||
        reader.Offset() > identityOffset + identityByteCount) {
        return false;
    }
    DecorationIdentity identity;
    identity.identifier = std::move(parts[0].text);
    identity.collection = std::move(parts[1].text);
    identity.author = std::move(parts[2].text);
    identity.archiveIdentifierMode = archiveIdentifierMode;
    if (identity.identifier.empty() || identity.collection.empty()) {
        return false;
    }
    *out = std::move(identity);
    if (readerOut != nullptr) {
        *readerOut = std::move(reader);
    }
    return true;
}

const GbxBodyExternalReference *ExternalReferenceForNode(
        const GbxBodyReferenceTable &references,
        u32 nodeIndex) {
    for (const GbxBodyExternalReference &reference :
         references.externalReferences) {
        if (reference.nodeIndex == nodeIndex) {
            return &reference;
        }
    }
    return nullptr;
}

std::optional<DecodedDecorationSizeBody> DecodeDecorationSizeBody(
        const unsigned char *archiveBytes,
        u32 archiveByteCount,
        u32 bodyOffset,
        const GbxBodyReferenceTable &references) {
    if (archiveBytes == nullptr || bodyOffset > archiveByteCount) {
        return std::nullopt;
    }

    bool sawBaseChunk = false;
    std::optional<DecodedDecorationSizeBody> result;
    u32 offset = bodyOffset;
    while (offset <= archiveByteCount && archiveByteCount - offset >= 4u) {
        const u32 chunkId = ReadU32LE(archiveBytes + offset);
        offset += 4u;
        if (chunkId == CMwNodArchiveFacadeSentinel) {
            if (!sawBaseChunk || !result) {
                return std::nullopt;
            }
            result->endOffset = offset;
            return result;
        }
        if (chunkId == TMNF_CGameCtnDecorationSizeChunk_0303B000) {
            if (sawBaseChunk ||
                archiveByteCount - offset <
                        DecorationSizeChunkPayloadByteCount) {
                return std::nullopt;
            }
            offset += DecorationSizeChunkPayloadByteCount;
            sawBaseChunk = true;
            continue;
        }
        if (chunkId != TMNF_CGameCtnDecorationSizeChunk_0303B001 ||
            !sawBaseChunk || result ||
            archiveByteCount - offset < DecorationSizeChunkPayloadByteCount) {
            return std::nullopt;
        }

        DecodedDecorationSizeBody decoded;
        decoded.definition.defaultZoneHeight =
                ReadU32LE(archiveBytes + offset);
        decoded.definition.mapSize.x = ReadU32LE(archiveBytes + offset + 4u);
        decoded.definition.mapSize.y = ReadU32LE(archiveBytes + offset + 8u);
        decoded.definition.mapSize.z = ReadU32LE(archiveBytes + offset + 12u);
        decoded.baseSceneNodeIndex = ReadU32LE(archiveBytes + offset + 16u);
        if (decoded.definition.mapSize.x == 0u ||
            decoded.definition.mapSize.y == 0u ||
            decoded.definition.mapSize.z == 0u ||
            decoded.definition.defaultZoneHeight >=
                    decoded.definition.mapSize.y ||
            ExternalReferenceForNode(
                    references, decoded.baseSceneNodeIndex) == nullptr) {
            return std::nullopt;
        }
        result = std::move(decoded);
        offset += DecorationSizeChunkPayloadByteCount;
    }
    return std::nullopt;
}

std::optional<DecodedDecorationSizeArchive>
DecodeReplayDecorationSizeArchiveWithReferences(
        const unsigned char *archiveBytes,
        u32 archiveByteCount) {
    u32 classId = 0u;
    GbxBodyReferenceTable references;
    if (!GbxBodyOffsetReader::TryParseWithReferences(
                archiveBytes,
                archiveByteCount,
                &classId,
                &references) ||
        classId != TMNF_CLASS_CGameCtnDecorationSize) {
        return std::nullopt;
    }
    auto body = DecodeDecorationSizeBody(
            archiveBytes,
            archiveByteCount,
            references.bodyOffset,
            references);
    if (!body || body->endOffset != archiveByteCount) {
        return std::nullopt;
    }
    DecodedDecorationSizeArchive decoded;
    decoded.definition = std::move(body->definition);
    decoded.baseSceneNodeIndex = body->baseSceneNodeIndex;
    decoded.references = std::move(references);
    return decoded;
}

bool ReadChunkAndSkip(
        const unsigned char *archiveBytes,
        u32 archiveByteCount,
        u32 expectedChunkId,
        u32 payloadByteCount,
        u32 *offset) {
    if (archiveBytes == nullptr || offset == nullptr ||
        *offset > archiveByteCount || archiveByteCount - *offset < 4u ||
        ReadU32LE(archiveBytes + *offset) != expectedChunkId) {
        return false;
    }
    *offset += 4u;
    return SkipBytes(archiveByteCount, offset, payloadByteCount);
}

std::optional<DecodedDecorationSizeBody> EmbeddedDecorationSize(
        const unsigned char *archiveBytes,
        u32 archiveByteCount,
        const GbxBodyReferenceTable &references,
        TmnfFormat::ClassicArchiveReader *identifierReader) {
    // CGameCtnDecoration::Chunk archives Size through chunk 0x03038011 as a
    // node reference, which may serialize the CGameCtnDecorationSize inline.
    u32 offset = references.bodyOffset;
    if (identifierReader == nullptr ||
        !ReadChunkAndSkip(
                archiveBytes,
                archiveByteCount,
                TMNF_CGameCtnCollectorChunk_0301A006,
                CollectorChunk0301A006PayloadByteCount,
                &offset) ||
        !ReadChunkAndSkip(
                archiveBytes,
                archiveByteCount,
                TMNF_CGameCtnCollectorChunk_0301A007,
                CollectorChunk0301A007PayloadByteCount,
                &offset) ||
        !ReadChunkAndSkip(
                archiveBytes,
                archiveByteCount,
                TMNF_CGameCtnCollectorChunk_LoadableNodRefAndId,
                CollectorChunk0301A009PayloadByteCount,
                &offset) ||
        offset > archiveByteCount || archiveByteCount - offset < 4u ||
        ReadU32LE(archiveBytes + offset) !=
                TMNF_CGameCtnCollectorChunk_CMwId0301A00A) {
        return std::nullopt;
    }
    offset += 4u;
    TmnfFormat::ClassicArchiveIdentifier identifier;
    if (!identifierReader->Seek(offset) ||
        !identifierReader->ReadIdentifier(identifier) ||
        identifierReader->Offset() > UINT32_MAX) {
        return std::nullopt;
    }
    offset = static_cast<u32>(identifierReader->Offset());
    if (offset > archiveByteCount || archiveByteCount - offset < 4u ||
        ReadU32LE(archiveBytes + offset) !=
                TMNF_CGameCtnCollectorChunk_SGameCtnIdentifier) {
        return std::nullopt;
    }
    offset += 4u;
    for (u32 identifierIndex = 0u; identifierIndex < 3u;
         ++identifierIndex) {
        if (!identifierReader->Seek(offset) ||
            !identifierReader->ReadIdentifier(identifier) ||
            identifierReader->Offset() > UINT32_MAX) {
            return std::nullopt;
        }
        offset = static_cast<u32>(identifierReader->Offset());
    }
    if (offset > archiveByteCount || archiveByteCount - offset < 12u ||
        ReadU32LE(archiveBytes + offset) !=
                TMNF_CGameCtnDecorationChunk_03038011) {
        return std::nullopt;
    }
    offset += 4u;
    const u32 sizeNodeIndex = ReadU32LE(archiveBytes + offset);
    offset += 4u;
    if (sizeNodeIndex == 0u || sizeNodeIndex == UINT32_MAX ||
        sizeNodeIndex > references.nodeCount ||
        references.IsExternalNode(sizeNodeIndex) ||
        ReadU32LE(archiveBytes + offset) !=
                TMNF_CLASS_CGameCtnDecorationSize) {
        return std::nullopt;
    }
    offset += 4u;
    return DecodeDecorationSizeBody(
            archiveBytes, archiveByteCount, offset, references);
}

bool CompleteDecorationSizeDefinition(
        const CPlugFilePack &pack,
        std::string_view decorationPath,
        std::string_view sizeSourcePath,
        std::string_view selectedSizePath,
        bool sizeEmbedded,
        const GbxBodyReferenceTable &references,
        u32 baseSceneNodeIndex,
        CatalogDecorationSizeDefinition *definition) {
    if (definition == nullptr) {
        return false;
    }
    const GbxBodyExternalReference *baseSceneReference =
            ExternalReferenceForNode(references, baseSceneNodeIndex);
    std::string baseScenePlainPath;
    char baseSceneSelectedPath[512]{};
    if (baseSceneReference == nullptr || baseSceneReference->IsResource() ||
        !baseSceneReference->useFile ||
        !references.ResolvePlainPathForReference(
                sizeSourcePath,
                *baseSceneReference,
                &baseScenePlainPath) ||
        !pack.SelectedPathForPlainRef(
                baseScenePlainPath.c_str(),
                baseSceneSelectedPath,
                sizeof(baseSceneSelectedPath))) {
        return false;
    }
    const CPlugFileFidContainer_SFileDesc *baseSceneFile =
            pack.FindFileDescByPath(baseSceneSelectedPath);
    if (baseSceneFile == nullptr ||
        baseSceneFile->classId != TMNF_CLASS_CScene3d) {
        return false;
    }
    try {
        definition->decorationSizeEmbedded = sizeEmbedded;
        definition->selectedDecorationPath = decorationPath;
        definition->selectedDecorationSizePath = selectedSizePath;
        definition->selectedBaseScenePath = baseSceneSelectedPath;
    } catch (const std::bad_alloc &) {
        return false;
    }
    return true;
}

std::optional<CatalogDecorationSizeDefinition> DecorationSizeFromArchive(
        const CPlugFilePack &pack,
        const char *decorationPath,
        const ByteBuffer &decorationBytes,
        const DecorationIdentity &expectedIdentity) {
    if (decorationPath == nullptr || decorationPath[0] == '\0' ||
        decorationBytes.Empty() || decorationBytes.Size() > UINT32_MAX) {
        return std::nullopt;
    }
    u32 classId = 0u;
    GbxBodyReferenceTable references;
    if (!GbxBodyOffsetReader::TryParseWithReferences(
                decorationBytes.Data(),
                static_cast<u32>(decorationBytes.Size()),
                &classId,
                &references) ||
        classId != TMNF_CLASS_CGameCtnDecoration) {
        return std::nullopt;
    }

    std::optional<CatalogDecorationSizeDefinition> result;
    for (const GbxBodyExternalReference &reference :
         references.externalReferences) {
        std::string plainPath;
        if (!references.ResolvePlainPathForReference(
                    decorationPath, reference, &plainPath) ||
            !EndsWith(plainPath, DecorationSizeSuffix)) {
            continue;
        }
        char selectedPath[512]{};
        ByteBuffer sizeBytes;
        const CPlugFileFidContainer_SFileDesc *sizeFile = nullptr;
        if (!pack.SelectedPathForPlainRef(
                    plainPath.c_str(), selectedPath, sizeof(selectedPath)) ||
            (sizeFile = pack.FindFileDescByPath(selectedPath)) == nullptr ||
            sizeFile->classId != TMNF_CLASS_CGameCtnDecorationSize ||
            !pack.ExtractPath(selectedPath, &sizeBytes) ||
            sizeBytes.Empty() || sizeBytes.Size() > UINT32_MAX) {
            return std::nullopt;
        }
        auto decoded = DecodeReplayDecorationSizeArchiveWithReferences(
                sizeBytes.Data(), static_cast<u32>(sizeBytes.Size()));
        if (!decoded || result) {
            return std::nullopt;
        }

        if (!CompleteDecorationSizeDefinition(
                    pack,
                    decorationPath,
                    plainPath,
                    selectedPath,
                    false,
                    decoded->references,
                    decoded->baseSceneNodeIndex,
                    &decoded->definition)) {
            return std::nullopt;
        }
        result = std::move(decoded->definition);
    }
    if (result) {
        return result;
    }

    ByteBuffer fullDecorationBytes;
    GbxBodyReferenceTable fullReferences;
    const ByteBuffer *bodyBytes = &decorationBytes;
    const GbxBodyReferenceTable *bodyReferences = &references;
    if (decorationBytes.Size() <= references.bodyOffset) {
        u32 fullClassId = 0u;
        if (!pack.ExtractPath(decorationPath, &fullDecorationBytes) ||
            fullDecorationBytes.Empty() ||
            fullDecorationBytes.Size() > UINT32_MAX ||
            !GbxBodyOffsetReader::TryParseWithReferences(
                    fullDecorationBytes.Data(),
                    static_cast<u32>(fullDecorationBytes.Size()),
                    &fullClassId,
                    &fullReferences) ||
            fullClassId != TMNF_CLASS_CGameCtnDecoration) {
            return std::nullopt;
        }
        bodyBytes = &fullDecorationBytes;
        bodyReferences = &fullReferences;
    }
    DecorationIdentity bodyIdentity;
    TmnfFormat::ClassicArchiveReader identifierReader;
    if (!ReadDecorationIdentity(
                bodyBytes->Data(),
                static_cast<u32>(bodyBytes->Size()),
                &bodyIdentity,
                &identifierReader) ||
        bodyIdentity.identifier != expectedIdentity.identifier ||
        bodyIdentity.collection != expectedIdentity.collection ||
        bodyIdentity.author != expectedIdentity.author ||
        bodyIdentity.archiveIdentifierMode !=
                expectedIdentity.archiveIdentifierMode) {
        return std::nullopt;
    }
    auto embedded = EmbeddedDecorationSize(
            bodyBytes->Data(),
            static_cast<u32>(bodyBytes->Size()),
            *bodyReferences,
            &identifierReader);
    if (!embedded ||
        !CompleteDecorationSizeDefinition(
                pack,
                decorationPath,
                decorationPath,
                {},
                true,
                *bodyReferences,
                embedded->baseSceneNodeIndex,
                &embedded->definition)) {
        return std::nullopt;
    }
    return std::move(embedded->definition);
}

}  // namespace

std::optional<CatalogDecorationSizeDefinition>
SelectReplayDecorationSizeCandidate(
        const std::optional<CatalogDecorationSizeDefinition> &candidate,
        u32 candidateCount,
        const GmNat3 &serializedMapSize) {
    if (candidateCount != 1u || !candidate || serializedMapSize.x == 0u ||
        serializedMapSize.y == 0u || serializedMapSize.z == 0u) {
        return std::nullopt;
    }
    CatalogDecorationSizeDefinition result = *candidate;
    result.mapSize = serializedMapSize;
    return result;
}

std::optional<CatalogDecorationSizeDefinition>
DecodeReplayDecorationSizeArchive(
        const unsigned char *archiveBytes,
        u32 archiveByteCount) {
    auto decoded = DecodeReplayDecorationSizeArchiveWithReferences(
            archiveBytes, archiveByteCount);
    return decoded
            ? std::optional<CatalogDecorationSizeDefinition>(
                      std::move(decoded->definition))
            : std::nullopt;
}

std::optional<CatalogDecorationSizeDefinition>
ResolveReplayDecorationSize(
        const CPlugFilePack &pack,
        const CGameCtnReplayMapInput &mapInput) {
    if (!mapInput.DecorationMood().HasName() ||
        !mapInput.DecorationCollection().HasName()) {
        return std::nullopt;
    }

    std::optional<CatalogDecorationSizeDefinition> candidate;
    u32 candidateCount = 0u;
    for (const CPlugFileFidContainer_SFileDesc &file : pack.files) {
        if (file.classId != TMNF_CLASS_CGameCtnDecoration) {
            continue;
        }
        char decorationPath[512]{};
        ByteBuffer decorationPrefix;
        DecorationIdentity identity;
        if (!pack.FileDescPlainPath(
                    &file, decorationPath, sizeof(decorationPath)) ||
            !pack.ExtractReferenceTablePrefix(
                    decorationPath, &decorationPrefix) ||
            decorationPrefix.Empty() ||
            decorationPrefix.Size() > UINT32_MAX ||
            !ReadDecorationIdentity(
                    decorationPrefix.Data(),
                    static_cast<u32>(decorationPrefix.Size()),
                    &identity) ||
            !IdentityMatches(identity, mapInput)) {
            continue;
        }
        auto decoded = DecorationSizeFromArchive(
                pack,
                decorationPath,
                decorationPrefix,
                identity);
        if (!decoded) {
            continue;
        }
        if (!candidate) {
            candidate = std::move(*decoded);
        }
        ++candidateCount;
    }
    return SelectReplayDecorationSizeCandidate(
            candidate, candidateCount, mapInput.Size());
}
