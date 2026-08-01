#pragma once

#include <vector>

#include "engine/core/engine_types.h"
#include "engine/core/gm_types.h"
#include "engine/core/gx_color.h"
#include "engine/rendering/plug.h"
class CPlugBitmap;
class CPlugBitmapRender;
class CPlugBitmapRenderScene3d;
struct CPlugMaterialCustom;
class CPlugShader;
struct CSystemFid;
struct CSystemPackDesc;

class CPlugBitmapAddress : public CMwNod {
public:
    CPlugBitmapAddress(void);
    ~CPlugBitmapAddress(void) override;

    void CreateBitmapAddress(
            CPlugShader *shader,
            CPlugBitmap *bitmap,
            unsigned long textureCoordinateSet);
    CPlugShader *Shader(void) const;
    CPlugBitmap *Bitmap(void) const;
    CSystemFid *ImageFid(void) const;

    void SetBumpEnvironmentScale(float scale);
    float BumpEnvironmentScale(void) const;
    void SetTexCoordIndex(unsigned long textureCoordinateSet);
    unsigned long TextureCoordinateSet(void) const;

private:
    CPlugShader *shader_ = nullptr;
    CMwNodRef<CPlugBitmap> bitmap_;
    float bumpEnvironmentScale_ = 0.0f;
    unsigned long textureCoordinateSet_ = 0ul;
};

class CPlugShaderPass : public CPlug {
public:
    CPlugShaderPass(void);
    ~CPlugShaderPass(void) override;

    void AttachShader(CPlugShader *shader, unsigned long passIndex);
    void DetachShader(void);
    void AddBitmapAddress(CPlugBitmapAddress *address);
    void RemoveBitmapAddress(CPlugBitmapAddress *address);
    const CPlugBitmapAddress *FirstBitmapAddress(void) const;
    CPlugBitmapAddress *FirstMutableBitmapAddress(void);

private:
    CPlugShader *shader_ = nullptr;
    unsigned long passIndex_ = 0ul;
    std::vector<CMwNodRef<CPlugBitmapAddress>> bitmapAddresses_;
};

class CPlugShader : public CPlug {
public:
    struct SRequirement {
        unsigned long TextureCoordinateCount(void) const;
        void SetTextureCoordinateCount(unsigned long count);

        bool RequiresVertexNormal(void) const;
        void SetRequiresVertexNormal(bool required);
        bool RequiresVertexColor(void) const;
        void SetRequiresVertexColor(bool required);
        bool RequiresFirstTangent(void) const;
        void SetRequiresFirstTangent(bool required);
        bool RequiresSecondTangent(void) const;
        void SetRequiresSecondTangent(bool required);

        bool operator==(const SRequirement &other) const;
        bool operator!=(const SRequirement &other) const;

    private:
        unsigned long textureCoordinateCount_ = 0ul;
        bool vertexNormal_ = true;
        bool vertexColor_ = false;
        bool firstTangent_ = false;
        bool secondTangent_ = false;
    };

    CPlugShader(void);
    ~CPlugShader(void) override;

    SRequirement &Requirement(void);
    const SRequirement &Requirement(void) const;
    void AddPass(CPlugShaderPass *pass);
    unsigned long GetPassToRenderCount(void) const;
    const CPlugShaderPass *GetPassToRender(unsigned long index) const;
    virtual void CheckGeometryRequirement(void);
    void RemovePasses(void);
    u32 RequiredTexCoordCount(void) const;
    bool RequiresVertexNormal(void) const;
    bool RequiresVertexColor(void) const;
    bool RequiresFirstTangent(void) const;
    bool RequiresSecondTangent(void) const;
    bool RequiresShadowTexCoords(void) const;
    bool RequiresLightTexCoords(void) const;
    bool RequiresGeneratedTexCoords(void) const;
    u32 GetRenderBeforeCount(void) const;
    CPlugBitmapAddress *GetRenderBeforeAt(u32 index);
    int RenderBeforeEntryMatchesPackDesc(
            u32 index,
            const CSystemPackDesc &packDesc) const;
    void SetDirty(int dirty, int visionDirty);
    bool IsDirty(void) const;
    bool IsVisionDirty(void) const;
    void SetArchiveFlags(u32 flags);
    u32 ArchiveFlags(void) const;
    CPlugBitmapRender *FindBitmapRenderByClassId(
            unsigned long classId,
            CPlugBitmap **outBitmap,
            CPlugBitmapAddress **outAddress);
private:
    const CPlugBitmapAddress *RenderBeforeAddress(u32 index) const;

    SRequirement requirement_;
    std::vector<CMwNodRef<CPlugShaderPass>> passes_;
    bool dirty_ = false;
    bool visionDirty_ = false;
    u32 archiveFlags_ = 0u;
};

class CPlugShaderGeneric : public CPlugShader {
public:
    struct SMaterial {
        GmVec3 classicLighting{1.0f, 1.0f, 1.0f};
        GmVec4 diffuseColor{1.0f, 1.0f, 1.0f, 1.0f};
        GmVec3 ambientColor{1.0f, 1.0f, 1.0f};
        float lightPower = 20.0f;
        GmVec3 emissiveColor{1.0f, 1.0f, 1.0f};
        GmVec3 extraColor{0.0f, 0.0f, 0.0f};
        GmVec4 finalColor{1.0f, 1.0f, 1.0f, 1.0f};
    };

    CPlugShaderGeneric(void);
    ~CPlugShaderGeneric(void) override;
    void SetMaterial(const SMaterial &material);
    const SMaterial &Material(void) const;

private:
    SMaterial material_;
};

class CPlugShaderApply : public CPlugShaderGeneric {
public:
    CPlugShaderApply(void);
    ~CPlugShaderApply(void) override;
    static CMwNod *MwNewCPlugShaderApply(void);
    virtual void RemoveLayers(void);

    float DefaultApplyValue(void) const;
    unsigned char PrimaryAlphaReference(void) const;
    unsigned char SecondaryAlphaReference(void) const;
    GxBGRAColor TextureOperationConstant(void) const;

private:
    float defaultApplyValue_ = 1.0f;
    bool enabled_ = true;
    unsigned char primaryAlphaReference_ = 0x7fu;
    unsigned char secondaryAlphaReference_ = 0x7fu;
    GxBGRAColor textureOperationConstant_{0x01010101u};
};
