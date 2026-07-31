#pragma once

#include <forevervalidator/camera.h>

#include <array>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <vector>

#include "engine/core/gm_types.h"
#include "engine/camera/race_camera_config.h"

namespace forevervalidator::camera::detail {

inline constexpr float Pi = 3.1415927410125732421875f;
inline constexpr float MillisecondsToSeconds = 0.0010000000474974513f;
inline constexpr float VectorNormalizeEpsilonSquared = 0.000009999999747378752f;

class LinearCurve {
  public:
    LinearCurve() = default;
    LinearCurve(std::initializer_list<CurveKey> keys);
    explicit LinearCurve(std::vector<CurveKey> keys);

    float Evaluate(float position) const;
    const std::vector<CurveKey> &Keys() const noexcept { return keys_; }

  private:
    std::vector<CurveKey> keys_;
};

enum class SmoothDirection : std::uint32_t {
    Falling = 0u,
    Rising = 1u,
};

class SmoothReal2 {
  public:
    SmoothReal2() = default;
    SmoothReal2(float delta, std::uint32_t riseTimeMs,
                std::uint32_t fallTimeMs);

    void Reset();
    float Update(bool active, std::uint32_t timeMs);
    float UpdateUsingFullTime(bool active, std::uint32_t timeMs);

    float Value() const noexcept { return value_; }
    float Delta() const noexcept { return delta_; }

  private:
    static float SmoothedValue(SmoothDirection direction, float position);
    static float SmoothedPosition(SmoothDirection direction, float value);
    void SetDirection(SmoothDirection direction, std::uint32_t timeMs);

    float delta_ = 0.0f;
    std::uint32_t riseTimeMs_ = 1000u;
    std::uint32_t fallTimeMs_ = 1000u;
    float transitionScale_ = 1.0f;
    float value_ = 0.0f;
    std::uint32_t transitionStartTimeMs_ = 0u;
    float transitionStartValue_ = 0.0f;
    SmoothDirection direction_ = SmoothDirection::Falling;
};

struct CameraFrame {
    GmIso4 transform{};
    std::array<float, 5> lens{{90.0f, 60.0f, 3.0f, -1.0f, -1.0f}};
};

struct VehicleFrame {
    std::uint32_t targetId = 0u;
    std::uint32_t timeMs = 0u;
    GmIso4 transform{};
    GmVec3 linearSpeed{};
    float signedSpeed = 0.0f;
    float steering = 0.0f;
    float accelerate = 0.0f;
    float brake = 0.0f;
    float turbo = 0.0f;
    float cameraFlightTransition = 0.0f;
    bool burning = false;
    bool gearChanged = false;
    bool isVehicleCar = true;
    std::array<bool, 4> wheelContact{{true, true, true, true}};
    std::array<bool, 4> wheelHasSurface{{true, true, true, true}};
    GmVec3 cameraSupportUp{};

    bool AllWheelsWithoutContact() const noexcept;
    bool AllWheelsWithoutSurface() const noexcept;
};

class Controller {
  public:
    virtual ~Controller() = default;
    virtual RaceCameraProfile Profile() const noexcept = 0;
    virtual void Reset(const VehicleFrame &vehicle) = 0;
    virtual CameraFrame
    Evaluate(const VehicleFrame &vehicle,
             const SegmentCollisionQuery &segmentCollision) = 0;
};

class TargetController : public Controller {
  public:
    CameraFrame Evaluate(const VehicleFrame &vehicle,
                         const SegmentCollisionQuery &segmentCollision) final;

  protected:
    explicit TargetController(float fieldOfViewDegrees);

    virtual void
    UpdateCamera(const VehicleFrame &vehicle,
                 const SegmentCollisionQuery &segmentCollision) = 0;

    CameraFrame camera_{};

  private:
};

std::unique_ptr<Controller> CreateController(
        const EnvironmentConfig &environment,
        RaceCameraProfile profile,
        std::optional<std::size_t> resourceIndex = std::nullopt);

struct RaceCameraEnvironmentFactory {
    static RaceCameraEnvironment Create(EnvironmentConfig config);
    static const EnvironmentConfig &Config(
            const RaceCameraEnvironment &environment);
};

GmVec3 ToInternal(const Vector3 &value);
Vector3 ToPublic(const GmVec3 &value);
GmQuat ToInternal(const Quaternion &value);
Quaternion ToPublic(const GmQuat &value);
GmIso4 ToInternal(const Transform &value);
Transform ToPublic(const GmIso4 &value);
VehicleFrame ToInternal(const RaceCameraVehicleState &value);
RaceCameraOutput ToPublic(const CameraFrame &value);

float Clamp(float value, float minimum, float maximum);
GmVec3 Add(const GmVec3 &left, const GmVec3 &right);
GmVec3 Subtract(const GmVec3 &left, const GmVec3 &right);
GmVec3 Scale(const GmVec3 &value, float scale);
float LengthSquared(const GmVec3 &value);
GmVec3 Normalize(const GmVec3 &value);
GmVec3 Cross(const GmVec3 &left, const GmVec3 &right);
float Dot(const GmVec3 &left, const GmVec3 &right);
GmVec3 TransformDirection(const GmMat3 &rotation, const GmVec3 &value);
GmVec3 InverseTransformDirection(const GmMat3 &rotation, const GmVec3 &value);
void SetDovAndUp(GmMat3 &rotation, const GmVec3 &directionOfView,
                 const GmVec3 &up);
GmVec3 SlerpVector(const GmVec3 &from, const GmVec3 &to, float blend);
std::uint32_t DeltaTimeMs(std::uint32_t now, std::uint32_t before);
float RateLimit(float current, float target, float maximumStep);

} // namespace forevervalidator::camera::detail
