#pragma once

#include <array>
#include <memory>
#include <variant>
#include <vector>

#include "engine/core/gx_color.h"
#include "engine/rendering/plug_bitmap_render.h"
#include "engine/resources/plug_file_img.h"
class CPlugShader;

enum EGxTexAddress : unsigned long {
    EGxTexAddress_Wrap = 0ul,
    EGxTexAddress_Clamp = 1ul,
    EGxTexAddress_Mirror = 2ul,
    EGxTexAddress_Border = 3ul,
};

class CPlugBitmapShader : public CPlug {
public:
    CPlugBitmapShader(void);
    ~CPlugBitmapShader(void) override;
    void SetBitmapOwner(CPlugBitmap *bitmap);
    void SetShader(CPlugShader *shader);
    int SetBitmapToSwap(CPlugBitmap *bitmap);

private:
    CPlugBitmap *bitmapOwner_ = nullptr;
    CMwNodRef<CPlugShader> shader_;
    CMwNodRef<CPlugBitmap> bitmapToSwap_;
};

class CPlugBitmap : public CPlug {
public:
    enum EColorDepth : unsigned long {
        EColorDepth_Default = 0ul,
        EColorDepth_High = 1ul,
        EColorDepth_Medium = 2ul,
        EColorDepth_Low = 3ul,
    };

    enum EUsage : unsigned long {
        EUsage_Color = 0ul,
        EUsage_Opacity = 1ul,
        EUsage_Luminance = 2ul,
        EUsage_RenderTarget = 3ul,
        EUsage_Vector2 = 4ul,
        EUsage_NormalMap = 5ul,
        EUsage_NormalMapAlternate = 6ul,
        EUsage_Generated = 7ul,
        EUsage_Mask = 8ul,
        EUsage_MaskWithMip = 9ul,
        EUsage_NormalMapRgb = 10ul,
        EUsage_NormalMapPacked = 11ul,
        EUsage_LuminanceAlpha = 14ul,
        EUsage_GeneratedAlpha = 15ul,
        EUsage_MaskLinear = 16ul,
        EUsage_MaskSrgb = 17ul,
        EUsage_Vector2Alternate = 18ul,
        EUsage_NormalMapSigned = 19ul,
        EUsage_NormalMapBgra = 20ul,
        EUsage_Vector2Signed = 21ul,
        EUsage_GeneratedAuxiliary = 22ul,
        EUsage_AnyFormat = 23ul,
        EUsage_NormalMapHeight = 24ul,
        EUsage_NormalMapHeightAlternate = 25ul,
        EUsage_NormalMapAlpha = 26ul,
        EUsage_NormalMapRgba = 27ul,
        EUsage_NormalMapRgbOnly = 28ul,
    };

    enum EPixelUpdate : unsigned long {
        PixelUpdate_None = 0ul,
        PixelUpdate_Copy = 1ul,
        PixelUpdate_Blend = 2ul,
        PixelUpdate_Dirty = 3ul,
        PixelUpdate_Render = 4ul,
        PixelUpdate_Shader = 5ul,
        PixelUpdate_Specular = 7ul,
        PixelUpdate_Unused = 8ul,
    };

    struct SPixelBlend;
    struct SPixelCopy;
    struct SPixelDirty;
    struct SSpecularSubMapCat;
    struct CDynaSpecular;
    struct PixelUpdateState;

    CPlugBitmap(void);
    ~CPlugBitmap(void) override;

    void SetDirty(int dirty);
    int CheckUsage(EUsage usage);
    void SetWantedColorDepth(EColorDepth colorDepth);
    void SetRenderTexelsMustPersist(int enabled);
    void SetIsNonPow2Conditional(int enabled);
    void SetRenderRequireBlending(int enabled);
    void SetRenderMultiSampleCount(unsigned long sampleCount);
    void SetRenderCanUseDetachedKeeper(int enabled);
    void SetDefaultVideoTimer(EPlugVideoTimer timerMode);
    void DeletePixels(void);
    void SetDefaultTexAddress(
            EGxTexAddress uAddress,
            EGxTexAddress vAddress,
            EGxTexAddress wAddress);
    void SetBorderAlpha(int enabled, float alpha);
    int ReGenerate(void);
    void SetPixelUpdate(
            EPixelUpdate updateKind,
            CPlugBitmapRender *updateParam);
    int SetUsage(EUsage usage);
    void SetImage(CPlugFileImg *image);
    void GenerateChecker(unsigned long checkerIndex);

    CPlugFileImg *Image(void) const;
    EUsage Usage(void) const;
    bool IsDirty(void) const;
    EPlugVideoTimer DefaultVideoTimer(void) const;
    EPixelUpdate PixelUpdateKind(void) const;
    CPlugBitmapRender *RenderPayload(void) const;

protected:
    void ForceImageBorders(void);
    void UpdateFromImage(void);

private:
    friend struct CPlugFileVideo;

    bool SupportsUsage(EUsage usage) const;
    EUsage FallbackUsageForCurrentFormat(void) const;
    void ApplyBorderPolicyIfReady(void);
    void InstallPixelUpdatePayload(
            EPixelUpdate updateKind,
            CPlugBitmapRender *updateParam);
    SPixelDirty *PixelDirtyPayload(void);
    void InstallVideoPixelUpdate(CPlugFileVideo &video);
    void ResolveUnsupportedUsageFromImage(void);
    void UpdateNonPowerOfTwoStateFromImage(void);
    CMwNodRef<CPlugFileImg> CreateGeneratedCheckerFile(
            unsigned long checkerIndex);
    EUsage GeneratedCheckerUsage(const CPlugFileImg &image) const {
        return image.Format() == CPlugFileImg::EFormat_Alpha8
                ? EUsage_Opacity
                : EUsage_Color;
    }

    CMwNodRef<CPlugFileImg> image_;
    EUsage usage_ = EUsage_Color;
    EColorDepth wantedColorDepth_ = EColorDepth_Default;
    EPlugVideoTimer videoTimer_ = EPlugVideoTimer_Game;
    EGxTexAddress uAddress_ = EGxTexAddress_Mirror;
    EGxTexAddress vAddress_ = EGxTexAddress_Mirror;
    EGxTexAddress wAddress_ = EGxTexAddress_Mirror;
    unsigned long multisampleCount_ = 0ul;
    bool dirty_ = false;
    bool texelsMustPersist_ = false;
    bool nonPowerOfTwoConditional_ = false;
    bool requireBlending_ = true;
    bool canUseDetachedKeeper_ = false;
    bool deleteImagePixels_ = true;
    bool generatedMipData_ = true;
    bool nonPowerOfTwoImage_ = false;
    bool cubeRenderTarget_ = false;
    bool automaticMipSuppressed_ = false;
    bool automaticMipDisabled_ = true;
    bool renderTexelReuse_ = false;
    bool specialMaskMipPolicy_ = false;
    bool borderUsesSourceColor_ = false;
    bool borderAddsAlpha_ = false;
    unsigned long borderWidth_ = 1ul;
    GxBGRAColor borderColor_{};
    float mipMapLodBiasDefault_ = 0.0f;
    float mipMapLowerAlpha_ = 0.0f;
    float bumpScaleFactor_ = 1.0f;
    float bumpScaleMipLevel_ = 0.5f;
    GmVec2 defaultTexCoordScale_{1.0f, 1.0f};
    GmVec2 defaultTexCoordTranslation_{0.0f, 0.0f};
    float defaultTexCoordRotation_ = 0.0f;
    std::unique_ptr<PixelUpdateState> pixelUpdate_;
};

struct CPlugBitmap::SPixelBlend {
    CMwNodRef<CPlugBitmap> source;
    float blendFactor = -1.0f;
    SPixelBlend(void);
};

struct CPlugBitmap::SPixelCopy {
    CMwNodRef<CPlugBitmap> source;
    SGxPixRect sourceRegion;
    int destinationX = 0;
    int destinationY = 0;
    SPixelCopy(void);
};

struct CPlugBitmap::SPixelDirty {
    std::vector<SGxPixRect> pendingRegions;
    CPlugFileVideo *videoSource = nullptr;

    SPixelDirty(void);
    ~SPixelDirty(void);
    void InstallVideoCallback(CPlugFileVideo &video);
};

struct CPlugBitmap::SSpecularSubMapCat {
    unsigned long rectX0 = 0ul;
    unsigned long rectY0 = 0ul;
    unsigned long rectX1 = 0ul;
    unsigned long rectY1 = 0ul;
    float normalizedWidth = 0.0f;
    float normalizedHeight = 0.0f;
    float normalizedX0Center = 0.0f;
    float normalizedY0Center = 0.0f;
    float specularScaleR = 1.0f;
    float specularScaleG = 1.0f;
    float specularScaleB = 1.0f;
    float shininess = 25.0f;
};

struct CPlugBitmap::CDynaSpecular {
    void SetRect(
            unsigned long index,
            unsigned long x0,
            unsigned long y0,
            unsigned long x1,
            unsigned long y1);
    void ComputeFromRects(const CPlugBitmap *bitmap);

    void InitializeDefaultSpecular(const CPlugBitmap &bitmap);
    std::vector<SSpecularSubMapCat> subMaps;
};

struct CPlugBitmap::PixelUpdateState {
    using Payload = std::variant<
            std::monostate,
            SPixelCopy,
            SPixelBlend,
            SPixelDirty,
            CMwNodRef<CPlugBitmapRender>,
            CMwNodRef<CPlugBitmapShader>,
            CDynaSpecular>;

    EPixelUpdate kind = PixelUpdate_None;
    Payload payload = std::monostate{};
};
