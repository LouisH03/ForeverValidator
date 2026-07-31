#ifndef FOREVERVALIDATOR_CUDA_STATIC_CONFIGURATION_H
#define FOREVERVALIDATOR_CUDA_STATIC_CONFIGURATION_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "simulation/runtime/replay_simulation_definition.h"

namespace forevervalidator::simulation {

enum class CudaStaticConfigurationBuildResult : std::uint8_t {
    Success,
    InvalidSource,
    CurveOverflow,
    TransmissionOverflow,
    MaterialOverflow,
    CollisionShapeOverflow,
    ForceFieldOverflow,
    WaterCellOverflow,
    WaterPlaneOverflow,
    AllocationFailed,
};

enum class CudaHandlingSpecialization : std::uint8_t {
    Generic,
    Legacy,
    GearedDriveDry,
    GearedDriveWater,
};

struct CudaStaticConfigurationBuildLimits {
    std::uint32_t maximumCurveKeys = 4096u;
    std::uint32_t maximumTransmissionValues = 64u;
    std::uint32_t maximumMaterials = 512u;
    std::uint32_t maximumMaterialRemapEntries = 65536u;
    std::uint32_t maximumCollisionShapes = 64u;
    std::uint32_t maximumForceFields = 1024u;
    std::uint32_t maximumWaterCells = 16777216u;
    std::uint32_t maximumWaterPlanes = 256u;
};

enum class CudaTuningCurveId : std::uint32_t {
    LateralContactSlowDownFromSpeed,
    MaxSideFrictionFromSpeed,
    RolloverLateralFromSpeed,
    RolloverLateralCoefficientFromAngle,
    WheelVisualSteerAngleFromSpeed,
    SteeringDriveTorqueFromSpeed,
    SteerSlowDownFromSpeed,
    SuspensionDamperAbsorbModulation,
    AirControlZScale,
    RadiusSteeringRadiusFromSpeed,
    RadiusSteeringMaxFrictionFromSpeed,
    SlipResponseAccelFromSpeed,
    SlipResponseSlippingAccelFromSpeed,
    ReverseGearAccelFromSpeed,
    BurnoutRolloverLateralFromSpeedRatio,
    BurnoutRadiusFromSpeed,
    BurnoutLateralSpeedFromRadius,
    DonutRolloverFromSpeed,
    BurnoutRolloverFromSpeed,
    SplashVerticalImpulse,
    SplashHorizontalImpulse,
    WaterFrictionFromSpeed,
    SurfaceFeedback,
    VehicleFeedbackRamp1,
    VehicleFeedbackRamp0,
    VehicleDefault30To100,
    Count,
};

inline constexpr std::size_t CudaTuningCurveCount =
        static_cast<std::size_t>(CudaTuningCurveId::Count);
inline constexpr std::uint32_t
        CudaTuningCurvePositionsNondecreasing = 1u;

struct CudaTuningCurve {
    std::uint32_t firstKey = 0u;
    std::uint32_t keyCount = 0u;
    std::uint32_t interpolation = 0u;
    std::uint32_t reserved = 0u;
};

struct CudaTuningCurveKey {
    float position = 0.0f;
    float value = 0.0f;
};

struct CudaTransmissionArray {
    std::uint32_t firstValue = 0u;
    std::uint32_t valueCount = 0u;
};

enum class CudaTransmissionArrayId : std::uint32_t {
    GearSpeedRatio,
    UpshiftThreshold,
    DownshiftThreshold,
    RpmWanted,
    TargetInputBias,
    RpmDelta,
    Count,
};

inline constexpr std::size_t CudaTransmissionArrayCount =
        static_cast<std::size_t>(CudaTransmissionArrayId::Count);

struct CudaVehicleGearedDriveTuning {
    float forwardAccelBase = 0.0f;
    float forwardAccelSpeedCoef = 0.0f;
    float forwardAccelCapWhenSlipping = 0.0f;
    float forwardAccelCap = 0.0f;
    float speedLimitForce = 0.0f;
    float forceZScale = 0.0f;
    float sideForceToDriveTorqueScale = 0.0f;
    float slippingSteerTorqueScale = 0.0f;
    float lateralForceScale = 0.0f;
    float slippingSideFrictionScale = 0.0f;
    float sideFrictionSlipBlend = 0.0f;
    float driveSideFrictionSlipBlend = 0.0f;
    float slipRatioScale = 0.0f;
    float currentForceTorqueMin = 0.0f;
    float currentTorqueXScale = 0.0f;
    float currentTorqueZScale = 0.0f;
    float perSlippingWheelAccelScale = 0.0f;
    float lowSpeedBSlippingGripScale = 0.0f;
    float forwardAccelCapWhenSlippingReverse = 0.0f;
    float forwardAccelCapReverse = 0.0f;
    float dirtSlideSideForceScale = 0.0f;
    float dirtSlideGateScale = 0.0f;
    float dirtSlideForwardForceScale = 0.0f;
    float dirtSlideForwardGateScale = 0.0f;
    float reverseSpeedNorm = 0.0f;
    ReplayVehicleBurnoutTuning burnout{};
    ReplayVehicleEngineInputTuning input{};
    std::array<CudaTransmissionArray, CudaTransmissionArrayCount>
            transmissionArrays{};
};

struct CudaVehicleTuning {
    float engineSpeedNorm = 0.0f;
    float lowSpeedFrictionMagnitude = 0.0f;
    float lowSpeedLinearDamping = 0.0f;
    std::uint32_t wheelForceMode = 0u;
    std::uint32_t handlingModel = 0u;
    ReplayVehicleTuningVisualSettings visual{};
    ReplayVehicleTuningSteering steering{};
    ReplayVehicleTuningSuspension suspension{};
    ReplayVehicleTuningContactResponse contactResponse{};
    ReplayVehicleTuningBodyAirResponse bodyAirResponse{};
    ReplayVehicleRadiusSteeringTuning radiusSteering{};
    ReplayVehicleSlipResponseTuning slipResponse{};
    CudaVehicleGearedDriveTuning gearedDrive{};
    ReplayVehicleTuningWater water{};
    ReplayVehicleTuningTurbo turbo{};
    ReplayVehicleTuningFeedback feedback{};
    std::array<CudaTuningCurve, CudaTuningCurveCount> curves{};
};

struct CudaVehicleCollisionShape {
    std::uint32_t surfaceType = 0u;
    GmIso4 localPose{};
    GmIso4 bodyPose{};
    GmBoxAligned localBounds{};
    GmLocalMaterialIndex localMaterial{};
    std::uint32_t surfaceMaterial = 0u;
    std::uint32_t wheelIndex = UINT32_MAX;
    std::uint32_t parentShapeIndex = UINT32_MAX;
    std::uint32_t archiveOrder = 0u;
    std::uint32_t traversalOrder = 0u;
};

enum class CudaForceFieldType : std::uint32_t {
    Uniform,
    Ball,
};

struct CudaForceField {
    CudaForceFieldType type = CudaForceFieldType::Uniform;
    GmVec3 vector{};
    float radius = 0.0f;
    float strength = 0.0f;
};

struct CudaWaterGrid {
    bool present = false;
    GmVec2 cellSize{};
    GmVec2 origin{};
    GmNat2 dimensions{};
    std::uint32_t outsideOccupancy = 0u;
    std::uint32_t outsidePlaneIndex = 0u;
    float surfaceHeight = 0.0f;
    float secondaryCullHeight = 0.0f;
};

struct CudaHostStaticConfiguration {
    static constexpr std::uint32_t SchemaVersion = 2u;

    std::uint32_t schemaVersion = SchemaVersion;
    std::uint64_t deterministicHash = 0u;
    VehicleInitialParameters initialParameters{};
    ReplayDynaParameters dynaParameters{};
    CudaVehicleTuning tuning{};
    VehicleWheelSetDefinition wheels{};
    float zoneLinearDampingCoefficient = 1.0f;
    float zoneAngularDampingCoefficient = 1.0f;
    CudaWaterGrid water{};
    std::array<CudaTransmissionArray, CudaTransmissionArrayCount>
            transmissionArrays{};
    std::vector<CudaTuningCurveKey> curveKeys;
    std::vector<float> transmissionValues;
    std::vector<CudaVehicleCollisionShape> collisionShapes;
    std::vector<VehicleMaterialDefinition> materials;
    std::vector<std::uint32_t> materialIndexByNaturalId;
    std::vector<std::uint8_t> fakeContactTextureRgb;
    std::vector<CudaForceField> forceFields;
    std::vector<std::uint8_t> waterOccupancy;
    std::vector<std::uint8_t> waterPlaneIndices;
    std::vector<GmVec4> waterPlanes;

    void Clear() noexcept;
    bool Valid(const CudaStaticConfigurationBuildLimits &limits = {})
            const noexcept;
};

CudaStaticConfigurationBuildResult BuildCudaHostStaticConfiguration(
        const ReplaySimulationDefinition &source,
        CudaHostStaticConfiguration *destination,
        const CudaStaticConfigurationBuildLimits &limits = {}) noexcept;

struct CudaStaticConfigurationSection {
    std::uint64_t offset = 0u;
    std::uint32_t count = 0u;
    std::uint32_t stride = 0u;
};

struct CudaPackedStaticConfigurationHeader {
    static constexpr std::uint32_t SchemaVersion = 2u;
    static constexpr std::uint64_t Magic = 0x4656435544414346ull;

    std::uint64_t magic = Magic;
    std::uint32_t schemaVersion = SchemaVersion;
    std::uint32_t headerSize = sizeof(CudaPackedStaticConfigurationHeader);
    std::uint64_t totalSize = 0u;
    std::uint64_t deterministicHash = 0u;
    VehicleInitialParameters initialParameters{};
    ReplayDynaParameters dynaParameters{};
    CudaVehicleTuning tuning{};
    VehicleWheelSetDefinition wheels{};
    float zoneLinearDampingCoefficient = 1.0f;
    float zoneAngularDampingCoefficient = 1.0f;
    CudaWaterGrid water{};
    CudaStaticConfigurationSection curveKeys{};
    CudaStaticConfigurationSection transmissionValues{};
    CudaStaticConfigurationSection collisionShapes{};
    CudaStaticConfigurationSection materials{};
    CudaStaticConfigurationSection materialIndexByNaturalId{};
    CudaStaticConfigurationSection fakeContactTextureRgb{};
    CudaStaticConfigurationSection forceFields{};
    CudaStaticConfigurationSection waterOccupancy{};
    CudaStaticConfigurationSection waterPlaneIndices{};
    CudaStaticConfigurationSection waterPlanes{};
};

bool PackCudaStaticConfiguration(
        const CudaHostStaticConfiguration &source,
        std::vector<std::byte> *destination,
        CudaPackedStaticConfigurationHeader *header = nullptr) noexcept;

#if defined(__CUDACC__)
namespace cuda::research {

#if defined(FOREVERVALIDATOR_CUDA_RESEARCH_SESSION_LTO)
extern "C" __device__ std::uint64_t
ForeverValidatorSessionConfigurationBase();
extern "C" __device__ const unsigned char *
ForeverValidatorSessionConfigurationBytes();
extern "C" __device__ const unsigned char *
ForeverValidatorSessionCollisionShapeBytes();
extern "C" __device__ const unsigned char *
ForeverValidatorSessionCurveKeyBytes();
template<std::uint32_t Index>
__device__ std::uint32_t ForeverValidatorSessionTuningWord();
template<std::uint32_t Index>
__device__ std::uint32_t ForeverValidatorSessionShapeWord(
        std::uint32_t shapeIndex);
extern __device__ __constant__
        CudaPackedStaticConfigurationHeader StaticConfiguration;
extern __device__ __constant__
        std::uint64_t StaticConfigurationBase;

__device__ inline std::uint64_t SessionConfigurationBase() {
    return ForeverValidatorSessionConfigurationBase();
}
#endif
#if defined(FOREVERVALIDATOR_CUDA_RESEARCH_CONSTANT_CONFIGURATION)
__device__ __constant__
        CudaPackedStaticConfigurationHeader StaticConfiguration;
__device__ __constant__
        std::uint64_t StaticConfigurationBase;
#endif
#if defined(FOREVERVALIDATOR_CUDA_RESEARCH_CONSTANT_COLLISION_SHAPES)
__device__ __constant__
        CudaVehicleCollisionShape StaticCollisionShapes[8];
#endif
#if defined(FOREVERVALIDATOR_CUDA_RESEARCH_CONSTANT_CURVE_KEYS)
__device__ __constant__
        CudaTuningCurveKey StaticCurveKeys[4096];
#endif

}  // namespace cuda::research

namespace cuda::facts {

#if defined(FOREVERVALIDATOR_CUDA_RESEARCH_SESSION_LTO)
template<typename T, std::uint32_t Offset>
__device__ inline T ShapeScalar(std::uint32_t shapeIndex) {
    static_assert(sizeof(T) <= sizeof(std::uint32_t));
    constexpr std::uint32_t shift =
            (Offset & 3u) * 8u;
    const std::uint32_t bits =
            research::ForeverValidatorSessionShapeWord<
                    Offset / 4u>(shapeIndex) >> shift;
    if constexpr (sizeof(T) == sizeof(std::uint32_t)) {
        union {
            std::uint32_t bits;
            T value;
        } exact{bits};
        return exact.value;
    } else {
        constexpr std::uint32_t mask =
                (1u << (sizeof(T) * 8u)) - 1u;
        return static_cast<T>(bits & mask);
    }
}

#define FOREVERVALIDATOR_CUDA_SHAPE_FLOAT(path) \
    ShapeScalar< \
            float, \
            __builtin_offsetof(CudaVehicleCollisionShape, path)>( \
            shapeIndex)

__device__ inline GmBoxAligned ShapeLocalBounds(
        std::uint32_t shapeIndex,
        const CudaVehicleCollisionShape &) {
    return {
            {
                    FOREVERVALIDATOR_CUDA_SHAPE_FLOAT(
                            localBounds.center.x),
                    FOREVERVALIDATOR_CUDA_SHAPE_FLOAT(
                            localBounds.center.y),
                    FOREVERVALIDATOR_CUDA_SHAPE_FLOAT(
                            localBounds.center.z),
            },
            {
                    FOREVERVALIDATOR_CUDA_SHAPE_FLOAT(
                            localBounds.halfExtents.x),
                    FOREVERVALIDATOR_CUDA_SHAPE_FLOAT(
                            localBounds.halfExtents.y),
                    FOREVERVALIDATOR_CUDA_SHAPE_FLOAT(
                            localBounds.halfExtents.z),
            },
    };
}

__device__ inline GmIso4 ShapeBodyPose(
        std::uint32_t shapeIndex,
        const CudaVehicleCollisionShape &) {
    return {
            {
                    {
                            FOREVERVALIDATOR_CUDA_SHAPE_FLOAT(
                                    bodyPose.rotation.basisX.x),
                            FOREVERVALIDATOR_CUDA_SHAPE_FLOAT(
                                    bodyPose.rotation.basisX.y),
                            FOREVERVALIDATOR_CUDA_SHAPE_FLOAT(
                                    bodyPose.rotation.basisX.z),
                    },
                    {
                            FOREVERVALIDATOR_CUDA_SHAPE_FLOAT(
                                    bodyPose.rotation.basisY.x),
                            FOREVERVALIDATOR_CUDA_SHAPE_FLOAT(
                                    bodyPose.rotation.basisY.y),
                            FOREVERVALIDATOR_CUDA_SHAPE_FLOAT(
                                    bodyPose.rotation.basisY.z),
                    },
                    {
                            FOREVERVALIDATOR_CUDA_SHAPE_FLOAT(
                                    bodyPose.rotation.basisZ.x),
                            FOREVERVALIDATOR_CUDA_SHAPE_FLOAT(
                                    bodyPose.rotation.basisZ.y),
                            FOREVERVALIDATOR_CUDA_SHAPE_FLOAT(
                                    bodyPose.rotation.basisZ.z),
                    },
            },
            {
                    FOREVERVALIDATOR_CUDA_SHAPE_FLOAT(
                            bodyPose.translation.x),
                    FOREVERVALIDATOR_CUDA_SHAPE_FLOAT(
                            bodyPose.translation.y),
                    FOREVERVALIDATOR_CUDA_SHAPE_FLOAT(
                            bodyPose.translation.z),
            },
    };
}

__device__ inline std::uint32_t ShapeWheelIndex(
        std::uint32_t shapeIndex,
        const CudaVehicleCollisionShape &) {
    return ShapeScalar<
            std::uint32_t,
            __builtin_offsetof(
                    CudaVehicleCollisionShape,
                    wheelIndex)>(shapeIndex);
}

__device__ inline std::uint32_t ShapeSurfaceMaterial(
        std::uint32_t shapeIndex,
        const CudaVehicleCollisionShape &) {
    return ShapeScalar<
            std::uint32_t,
            __builtin_offsetof(
                    CudaVehicleCollisionShape,
                    surfaceMaterial)>(shapeIndex);
}

#undef FOREVERVALIDATOR_CUDA_SHAPE_FLOAT

template<typename T, std::uint32_t Offset>
struct TuningFact {
    static_assert(sizeof(T) <= sizeof(std::uint32_t));

    __device__ inline operator T() const {
        constexpr std::uint32_t shift =
                (Offset & 3u) * 8u;
        const std::uint32_t bits =
                research::ForeverValidatorSessionTuningWord<
                        Offset / 4u>() >> shift;
        if constexpr (sizeof(T) == sizeof(std::uint32_t)) {
            union {
                std::uint32_t bits;
                T value;
            } exact{bits};
            return exact.value;
        } else {
            constexpr std::uint32_t mask =
                    (1u << (sizeof(T) * 8u)) - 1u;
            return static_cast<T>(bits & mask);
        }
    }
};

#define FOREVERVALIDATOR_CUDA_TUNING_FACT(path, name) \
    TuningFact< \
            decltype(((CudaVehicleTuning *)nullptr)->path), \
            __builtin_offsetof(CudaVehicleTuning, path)> name

struct BurnoutTuningFacts {
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.burnout.angleLimit, angleLimit);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.burnout.angleLimitNegative,
            angleLimitNegative);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.burnout.angleLimitPositive,
            angleLimitPositive);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.burnout.angleReturnQuadratic,
            angleReturnQuadratic);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.burnout.angleTorqueScale,
            angleTorqueScale);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.burnout.angularDampingLinear,
            angularDampingLinear);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.burnout.donutSpeedHigh, donutSpeedHigh);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.burnout.donutSpeedLow, donutSpeedLow);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.burnout.driveFadeScale, driveFadeScale);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.burnout.durationTicks, durationTicks);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.burnout.exitAccelFadeScale,
            exitAccelFadeScale);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.burnout.exitBonusAccelScale,
            exitBonusAccelScale);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.burnout.exitDurationTicks,
            exitDurationTicks);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.burnout.lateralCorrectionScale,
            lateralCorrectionScale);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.burnout.radiusCorrectionScale,
            radiusCorrectionScale);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.burnout.radiusCorrectionSpeedScale,
            radiusCorrectionSpeedScale);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.burnout.radiusMin, radiusMin);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.burnout.reverseForceThreshold,
            reverseForceThreshold);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.burnout.sideForceFadeScale,
            sideForceFadeScale);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.burnout.tangentAngularDamping,
            tangentAngularDamping);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.burnout.tangentSpeedMax,
            tangentSpeedMax);
};

struct GearedDriveTuningFacts {
    BurnoutTuningFacts burnout;
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.currentForceTorqueMin,
            currentForceTorqueMin);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.currentTorqueXScale, currentTorqueXScale);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.currentTorqueZScale, currentTorqueZScale);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.dirtSlideForwardForceScale,
            dirtSlideForwardForceScale);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.dirtSlideForwardGateScale,
            dirtSlideForwardGateScale);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.dirtSlideGateScale, dirtSlideGateScale);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.dirtSlideSideForceScale,
            dirtSlideSideForceScale);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.driveSideFrictionSlipBlend,
            driveSideFrictionSlipBlend);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.forceZScale, forceZScale);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.forwardAccelBase, forwardAccelBase);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.forwardAccelCap, forwardAccelCap);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.forwardAccelCapReverse,
            forwardAccelCapReverse);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.forwardAccelCapWhenSlipping,
            forwardAccelCapWhenSlipping);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.forwardAccelCapWhenSlippingReverse,
            forwardAccelCapWhenSlippingReverse);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.forwardAccelSpeedCoef,
            forwardAccelSpeedCoef);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.lateralForceScale, lateralForceScale);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.lowSpeedBSlippingGripScale,
            lowSpeedBSlippingGripScale);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.perSlippingWheelAccelScale,
            perSlippingWheelAccelScale);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.reverseSpeedNorm, reverseSpeedNorm);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.sideForceToDriveTorqueScale,
            sideForceToDriveTorqueScale);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.sideFrictionSlipBlend,
            sideFrictionSlipBlend);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.slipRatioScale, slipRatioScale);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.slippingSideFrictionScale,
            slippingSideFrictionScale);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.slippingSteerTorqueScale,
            slippingSteerTorqueScale);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            gearedDrive.speedLimitForce, speedLimitForce);
};

struct BodyAirResponseTuningFacts {
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            bodyAirResponse.airControlMemoryTickWindow,
            airControlMemoryTickWindow);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            bodyAirResponse.airControlYSwitchThreshold,
            airControlYSwitchThreshold);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            bodyAirResponse.airTorqueLinearCoef,
            airTorqueLinearCoef);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            bodyAirResponse.airTorqueQuadraticCoef,
            airTorqueQuadraticCoef);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            bodyAirResponse.airborneSolidFeedback0,
            airborneSolidFeedback0);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            bodyAirResponse.airborneSolidFeedback1,
            airborneSolidFeedback1);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            bodyAirResponse.groundedSolidFeedback1,
            groundedSolidFeedback1);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            bodyAirResponse.slopeAdherence1Max,
            slopeAdherence1Max);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            bodyAirResponse.slopeAdherence1Min,
            slopeAdherence1Min);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            bodyAirResponse.slopeAdherence2Max,
            slopeAdherence2Max);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            bodyAirResponse.slopeAdherence2Min,
            slopeAdherence2Min);
};

struct ContactResponseTuningFacts {
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            contactResponse.bodyImpactFeedbackHighThreshold,
            bodyImpactFeedbackHighThreshold);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            contactResponse.bodyImpactFeedbackLowThreshold,
            bodyImpactFeedbackLowThreshold);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            contactResponse.specialContactImpulseMagnitude,
            specialContactImpulseMagnitude);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            contactResponse.specialSolidFeedbackValue,
            specialSolidFeedbackValue);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            contactResponse.wheelImpactFeedbackHighThreshold,
            wheelImpactFeedbackHighThreshold);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            contactResponse.wheelImpactFeedbackLowThreshold,
            wheelImpactFeedbackLowThreshold);
};

struct FeedbackTuningFacts {
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            feedback.forceDivisor, forceDivisor);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            feedback.surfaceBaseRate, surfaceBaseRate);
};

struct SlipResponseTuningFacts {
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            slipResponse.lateralSlowDownTickWindow,
            lateralSlowDownTickWindow);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            slipResponse.longitudinalTorqueScale,
            longitudinalTorqueScale);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            slipResponse.slippingAccelScale, slippingAccelScale);
};

struct SteeringTuningFacts {
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            steering.assistFullSpeed, assistFullSpeed);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            steering.slowDownScale, slowDownScale);
};

struct SuspensionTuningFacts {
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            suspension.damperModulationMaxAbsorb,
            damperModulationMaxAbsorb);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            suspension.damperModulationMinAbsorb,
            damperModulationMinAbsorb);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            suspension.wheelDamperCoef, wheelDamperCoef);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            suspension.wheelRestDamperAbsorb,
            wheelRestDamperAbsorb);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            suspension.wheelSpringCoef, wheelSpringCoef);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            suspension.wheelStaticSpringScale,
            wheelStaticSpringScale);
};

struct TurboTuningFacts {
    FOREVERVALIDATOR_CUDA_TUNING_FACT(turbo.durationA, durationA);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(turbo.durationB, durationB);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            turbo.impulseScaleA, impulseScaleA);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            turbo.impulseScaleB, impulseScaleB);
};

struct VisualTuningFacts {
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            visual.wheelSpeedBase, wheelSpeedBase);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            visual.wheelSpeedScale, wheelSpeedScale);
};

struct WaterTuningFacts {
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            water.angularLinearDamping, angularLinearDamping);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            water.angularSpeedDamping, angularSpeedDamping);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            water.buoyancyForce, buoyancyForce);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            water.splashHorizontalSpeedThreshold,
            splashHorizontalSpeedThreshold);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            water.splashTotalSpeedThreshold,
            splashTotalSpeedThreshold);
};

struct SessionTuningFacts {
    BodyAirResponseTuningFacts bodyAirResponse;
    ContactResponseTuningFacts contactResponse;
    FeedbackTuningFacts feedback;
    GearedDriveTuningFacts gearedDrive;
    SlipResponseTuningFacts slipResponse;
    SteeringTuningFacts steering;
    SuspensionTuningFacts suspension;
    TurboTuningFacts turbo;
    VisualTuningFacts visual;
    WaterTuningFacts water;
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            engineSpeedNorm, engineSpeedNorm);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            handlingModel, handlingModel);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            lowSpeedFrictionMagnitude, lowSpeedFrictionMagnitude);
    FOREVERVALIDATOR_CUDA_TUNING_FACT(
            lowSpeedLinearDamping, lowSpeedLinearDamping);
};

#undef FOREVERVALIDATOR_CUDA_TUNING_FACT

__device__ inline SessionTuningFacts Tuning(
        const CudaPackedStaticConfigurationHeader *) {
    return {};
}
#else
__device__ inline const GmBoxAligned &ShapeLocalBounds(
        std::uint32_t,
        const CudaVehicleCollisionShape &shape) {
    return shape.localBounds;
}

__device__ inline const GmIso4 &ShapeBodyPose(
        std::uint32_t,
        const CudaVehicleCollisionShape &shape) {
    return shape.bodyPose;
}

__device__ inline std::uint32_t ShapeWheelIndex(
        std::uint32_t,
        const CudaVehicleCollisionShape &shape) {
    return shape.wheelIndex;
}

__device__ inline std::uint32_t ShapeSurfaceMaterial(
        std::uint32_t,
        const CudaVehicleCollisionShape &shape) {
    return shape.surfaceMaterial;
}

__device__ inline const CudaVehicleTuning &Tuning(
        const CudaPackedStaticConfigurationHeader *configuration) {
#if defined(FOREVERVALIDATOR_CUDA_RESEARCH_CONSTANT_CONFIGURATION)
    return research::StaticConfiguration.tuning;
#else
    return configuration->tuning;
#endif
}
#endif

__device__ inline const VehicleWheelDefinition &Wheel(
        const CudaPackedStaticConfigurationHeader *configuration,
        std::uint32_t index) {
#if defined(FOREVERVALIDATOR_CUDA_RESEARCH_CONSTANT_CONFIGURATION)
    return reinterpret_cast<const VehicleWheelDefinition *>(
            &research::StaticConfiguration.wheels.wheels)[index];
#else
    return reinterpret_cast<const VehicleWheelDefinition *>(
            &configuration->wheels.wheels)[index];
#endif
}

__device__ inline VehicleWheelAxle WheelAxle(
        const CudaPackedStaticConfigurationHeader *configuration,
        std::uint32_t index) {
#if defined(FOREVERVALIDATOR_CUDA_RESEARCH_CANONICAL_WHEEL_FACTS)
    return index < 2u
            ? VehicleWheelAxle::Front
            : VehicleWheelAxle::Rear;
#else
    return Wheel(configuration, index).axle;
#endif
}

__device__ inline bool WheelKillsLateralSpeed(
        const CudaPackedStaticConfigurationHeader *configuration,
        std::uint32_t index) {
#if defined(FOREVERVALIDATOR_CUDA_RESEARCH_CANONICAL_WHEEL_FACTS)
    return true;
#else
    return Wheel(
            configuration,
            index).killsLateralSpeedOnContact;
#endif
}

__device__ inline float WheelRollingRadius(
        const CudaPackedStaticConfigurationHeader *configuration,
        std::uint32_t index) {
#if defined(FOREVERVALIDATOR_CUDA_RESEARCH_CANONICAL_WHEEL_FACTS)
    return Wheel(configuration, 0u).rollingRadius;
#else
    return Wheel(configuration, index).rollingRadius;
#endif
}

__device__ inline std::uint32_t WheelForceMode(
        const CudaPackedStaticConfigurationHeader *configuration) {
#if defined(FOREVERVALIDATOR_CUDA_RESEARCH_WHEEL_FORCE_MODE)
    return FOREVERVALIDATOR_CUDA_RESEARCH_WHEEL_FORCE_MODE;
#else
    return configuration->tuning.wheelForceMode;
#endif
}

}  // namespace cuda::facts
#endif

}  // namespace forevervalidator::simulation

#endif
