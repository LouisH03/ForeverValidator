#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "engine/core/gx_color.h"
#include "engine/resources/plug_file.h"
#include "engine/rendering/plug_image_pixels.h"
class CPlugBitmap;

enum EPlugVideoTimer : unsigned long {
    EPlugVideoTimer_Default = 0ul,
    EPlugVideoTimer_Game = 1ul,
};

struct SGxPixRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct SPlugPixelDirtyFormat {
    unsigned long bytesPerPixel = 0ul;
    std::array<bool, 4> channelPresent{};
};

struct CPlugFileImg : CPlugFile {
    enum EFormat : unsigned long {
        EFormat_Alpha8,
        EFormat_RGB8,
        EFormat_BGRA8,
        EFormat_PackedColor16,
    };

    enum ENonPowOfTwo : unsigned long {
        ENonPowOfTwo_Unsupported = 0ul,
        ENonPowOfTwo_Conditional = 1ul,
        ENonPowOfTwo_Supported = 2ul,
    };

    struct SDesc {
        unsigned long width = 0ul;
        unsigned long height = 1ul;
        unsigned long depth = 1ul;
        EFormat format = EFormat_RGB8;
    };

    static ENonPowOfTwo s_NonPowOfTwoSupport;
    static unsigned long s_EnableLoadTexels;

    CPlugFileImg(void);
    ~CPlugFileImg(void) override;
    CPlugFileImg(const CPlugFileImg &) = delete;
    CPlugFileImg &operator=(const CPlugFileImg &) = delete;

    virtual int ReGenerate(void);
    unsigned long ComputeNbMipMapLevel(void) const;
    int ReGenerateForceTexelLoading(void);
    int IsInSystemMemory(void) const;
    unsigned char *GetPixel(unsigned long x, unsigned long y) const;
    unsigned long GetDepth(void) const;
    unsigned long GetByteSizePerComp(void) const;
    void DeletePixels(void);
    void AddAlpha_BGRA(unsigned char alpha);
    void ForceBorders_BGRA(
            GxBGRAColor &color,
            int hasSourceColor,
            int addAlpha,
            unsigned long borderWidth);

    unsigned long Width(void) const;
    unsigned long Height(void) const;
    unsigned long Depth(void) const;
    unsigned long BytesPerPixel(void) const;
    unsigned long PixelCount(void) const;
    unsigned long RowByteCount(void) const;
    EFormat Format(void) const;
    bool IsGeneratedImage(void) const;
    bool HasGeneratedMipData(void) const;
    bool HasAutomaticMipSource(void) const;
    bool HasEmbeddedMipSource(void) const;
    bool IsCubeMap(void) const;
    bool IsTwoBytePackedColor(void) const;
    const std::vector<unsigned char> &PixelBytes(void) const;
    void *AllocatePixels(void);
    void SetPixels(
            const SDesc &description,
            unsigned char *pixels,
            unsigned long byteCount);
    void AssignPixels(
            unsigned long width,
            unsigned long height,
            unsigned long depth,
            EFormat format,
            std::vector<unsigned char> pixels);

protected:
    void FinishGeneratedPixels(void);

private:
    struct MipMapState {
        std::optional<unsigned long> storedLevelCount = 1ul;
        bool generatedData = false;
        bool automaticSource = false;
        bool embeddedSource = false;
    };

    static unsigned long FormatByteCount(EFormat format);

    EFormat format_ = EFormat_RGB8;
    mutable PlugImagePixels pixels_;
    MipMapState mipMaps_;
    bool cubeMap_ = false;
};

struct CPlugFileVideo : CPlugFileImg {
    CPlugFileVideo(void);
    ~CPlugFileVideo(void) override;
    int ReGenerate(void) override;
    virtual void Play(
            EPlugVideoTimer timerMode,
            int restart,
            unsigned long frameIndex);
    virtual void UpdateVideoTexture(
            CPlugBitmap *bitmap,
            SGxPixRect *dirtyRect,
            const SPlugPixelDirtyFormat &dirtyFormat,
            unsigned char *pixels,
            unsigned long byteCount);
    void AutoInitBitmap(CPlugBitmap *bitmap);
};

struct CPlugFileBink : CPlugFileVideo {
};

struct CPlugFileGen : CPlugFileImg {
    CPlugFileGen(void);
    ~CPlugFileGen(void) override;
    int ReGenerate(void) override;
    void GenChecker(unsigned long checkerIndex);

private:
    std::optional<unsigned long> checkerId_;
};
