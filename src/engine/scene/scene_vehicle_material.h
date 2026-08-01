#pragma once

#include <memory>
#include <vector>

#include "engine/core/cmw_nod.h"
#include "engine/core/engine_types.h"
class CPlugBitmap;

struct CSceneVehicleMaterial : CMwNod {
    struct SBlendableVals {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 0.0f;
    };
    SBlendableVals blendableVals;
    CPlugBitmap *fakeContactBitmap = nullptr;
    float fakeContactPeriodX = 0.0f;
    float fakeContactPeriodZ = 0.0f;
    float fakeContactSpeedScale = 0.0f;
    float fakeContactDepthMax = 0.0f;
    float feedbackSpeedDivisor = 0.0f;
    float feedbackScale = 0.0f;
    u32 materialNaturalId = 0u;
    CSceneVehicleMaterial(void);
    ~CSceneVehicleMaterial(void) override;
    void Configure(const SBlendableVals &blendableValues,
                             float contactPeriodX,
                             float contactPeriodZ,
                             float contactSpeedScale,
                             float contactDepthMax,
                             float feedbackSpeedDenominator,
                             float feedbackScaleValue,
                             u32 naturalId);
    void AttachFakeContactBitmap(CPlugBitmap *bitmap);
};
struct CSceneVehicleMaterialContainer : CMwNod {
    std::vector<std::unique_ptr<CSceneVehicleMaterial>> materials;
    void Clear(void);
    CSceneVehicleMaterial &AddMaterial(void);
    u32 MaterialCount(void) const;
    CSceneVehicleMaterial *MaterialAt(u32 index) const;
};
