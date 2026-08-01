#ifndef TMNF_STATIC_SOLID_ARCHIVE_CMWID_STATE_H
#define TMNF_STATIC_SOLID_ARCHIVE_CMWID_STATE_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "engine/core/mw_id.h"
class CGameCtnReplayStaticSolidArchiveByteStream;

struct CGameCtnReplayStaticSolidArchiveCMwIdState {
    void ResetSharedNameCache(std::size_t capacity);

    int ReadSkip(CGameCtnReplayStaticSolidArchiveByteStream &stream);
    int ReadText(
            CGameCtnReplayStaticSolidArchiveByteStream &stream,
            char *out,
            std::size_t outSize);
    int ReadId(
            CGameCtnReplayStaticSolidArchiveByteStream &stream,
            CMwId *idOut);

    const char *LastTextOrNull() const;

private:
    enum class EncodingMode : std::uint8_t {
        Unknown = 0u,
        TextTagged = 1u,
        InlineNames = 2u,
        SharedNames = 3u,
    };

    EncodingMode mode_ = EncodingMode::Unknown;
    std::size_t sharedNameCacheCapacity_ = 0u;
    std::vector<CMwId> sharedNameCache_;
    char lastText_[64]{};
    bool hasLastText_ = false;

    int ReadResolved(
            CGameCtnReplayStaticSolidArchiveByteStream &stream,
            CMwId *idOut,
            char *textOut,
            std::size_t textOutSize);
    static void CaptureText(
            char *textOut,
            std::size_t textOutSize,
            const char *name);
    void ClearLastText();
    void CaptureLastText(const char *name);
};

#endif
