#include "engine/camera/race_camera_internal.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "engine/core/binary32_math.h"

namespace forevervalidator::camera::detail {
namespace {

constexpr GmVec3 WorldUp{0.0f, 1.0f, 0.0f};

enum class CameraState : std::uint32_t {
    Ground = 0u,
    Burning = 1u,
    Airborne = 2u,
};

bool IsActive(float value) { return value > 0.10000000149011612f; }

bool IsSteering(float value) {
    return value > 0.10000000149011612f || value < -0.10000000149011612f;
}

float HorizontalLength(const GmVec3 &value) {
    return CIsqrt(value.x * value.x + value.z * value.z);
}

GmVec3 VehicleForward(const VehicleFrame &vehicle) {
    return vehicle.transform.rotation.Row(GmAxis::Z);
}

GmVec3 VehicleUp(const VehicleFrame &vehicle) {
    return vehicle.transform.rotation.Row(GmAxis::Y);
}

GmMat3 TrackingFrame(const GmVec3 &direction) {
    GmMat3 frame;
    GmVec3 side = Cross(WorldUp, direction);
    const float sideLengthSquared = LengthSquared(side);
    if (sideLengthSquared > VectorNormalizeEpsilonSquared) {
        side = Scale(side, 1.0f / CIsqrt(sideLengthSquared));
    }

    GmVec3 normalizedUp = WorldUp;
    const float upLengthSquared = LengthSquared(normalizedUp);
    if (upLengthSquared > VectorNormalizeEpsilonSquared) {
        normalizedUp = Scale(normalizedUp, 1.0f / CIsqrt(upLengthSquared));
    }

    frame.SetRow(GmAxis::X, side);
    frame.SetRow(GmAxis::Y, normalizedUp);
    frame.SetRow(GmAxis::Z, Cross(side, normalizedUp));
    return frame;
}

void IntegrateSpring(float target, float stiffness, float damping, float dt,
                     float &value, float &velocity) {
    const float acceleration =
            (target - value) * stiffness - damping * velocity;
    velocity = acceleration * dt + velocity;
    value = dt * velocity + value;
}

void SetCameraPose(CameraFrame &camera, const GmVec3 &position,
                   const GmVec3 &direction, const GmVec3 &up) {
    camera.transform.translation = position;
    SetDovAndUp(camera.transform.rotation, direction, up);
}

struct InputHistory {
    bool steering = false;
    std::uint32_t steeringStartMs = 0u;
    std::uint32_t steeringEndMs = 0u;
    std::uint32_t steeringDurationMs = 0u;
    float steeringSign = 0.0f;

    bool airborneCandidate = false;
    std::uint32_t airborneCandidateStartMs = 0u;
    std::uint32_t burningStartMs = 0u;

    void Reset() { *this = {}; }

    void UpdateSteering(const VehicleFrame &vehicle, std::uint32_t dtMs,
                        std::uint32_t inactiveResetMs) {
        if (IsSteering(vehicle.steering)) {
            if (!steering) {
                steering = true;
                steeringStartMs = vehicle.timeMs;
            }
            steeringDurationMs += dtMs;
            steeringSign = vehicle.steering;
            return;
        }
        if (steering) {
            steering = false;
            steeringEndMs = vehicle.timeMs;
        }
        if (vehicle.timeMs >= steeringEndMs &&
            vehicle.timeMs - steeringEndMs > inactiveResetMs) {
            steeringDurationMs = 0u;
        }
    }

    CameraState UpdateState(const VehicleFrame &vehicle, CameraState previous,
                            bool allWheelsUnsupported,
                            std::uint32_t airborneValidationMs) {
        if (vehicle.isVehicleCar && allWheelsUnsupported) {
            if (!airborneCandidate) {
                airborneCandidate = true;
                airborneCandidateStartMs = vehicle.timeMs;
            }
        } else {
            airborneCandidate = false;
        }

        if (airborneCandidate && vehicle.timeMs >= airborneCandidateStartMs &&
            vehicle.timeMs - airborneCandidateStartMs > airborneValidationMs) {
            return CameraState::Airborne;
        }
        if (vehicle.burning) {
            if (previous != CameraState::Burning) {
                burningStartMs = vehicle.timeMs;
            }
            return CameraState::Burning;
        }
        return CameraState::Ground;
    }
};

class RaceController final : public TargetController {
  public:
    explicit RaceController(RaceConfig config)
        : TargetController(config.common.initialFieldOfView),
          config_(std::move(config)) {
        camera_.lens = {config_.common.initialFieldOfView,
                        config_.common.lensParameter1,
                        config_.common.lensParameter2, -1.0f, -1.0f};
    }

    RaceCameraProfile Profile() const noexcept override {
        return RaceCameraProfile::Race;
    }

    void Reset(const VehicleFrame &vehicle) override {
        targetId_ = vehicle.targetId;
        camera_.transform = vehicle.transform;
        camera_.transform.translation =
                Add(vehicle.transform.translation,
                    TransformDirection(vehicle.transform.rotation,
                                       {0.0f, 0.0f, -15.0f}));
    }

  private:
    void UpdateCamera(const VehicleFrame &vehicle,
                      const SegmentCollisionQuery &) override {
        if (vehicle.targetId != targetId_) {
            Reset(vehicle);
        }

        // The separately inserted Race profile loads the inherited
        // ignore-target-rotation flag. The original copies the target
        // transform, replaces only its rotation with identity, and retains
        // the target translation before applying this controller.
        const GmMat3 tracking = TrackingFrame(
                config_.common.ignoreTargetRotation
                        ? GmVec3{0.0f, 0.0f, 1.0f}
                        : VehicleForward(vehicle));
        const GmVec3 reference =
                Add(vehicle.transform.translation,
                    Scale(WorldUp, config_.common.height));
        GmVec3 offset = InverseTransformDirection(
                tracking, Subtract(camera_.transform.translation, reference));
        offset.y = 0.0f;

        float radiusSquared = offset.x * offset.x + offset.z * offset.z;
        if (radiusSquared <= VectorNormalizeEpsilonSquared) {
            offset = {0.0f, 0.0f, -config_.common.minimumDistance};
            radiusSquared = config_.common.minimumDistance *
                            config_.common.minimumDistance;
        }
        const float radius = CIsqrt(radiusSquared);
        if (radius < config_.common.minimumDistance) {
            offset = Scale(offset, config_.common.minimumDistance / radius);
        } else if (radius > config_.common.maximumDistance) {
            offset = Scale(offset, config_.common.maximumDistance / radius);
        }

        float alignment = 1.0f;
        if (vehicle.signedSpeed >= 0.0f) {
            if (vehicle.signedSpeed < config_.coneMinimumSpeed) {
                alignment = 1.0f;
            } else if (vehicle.signedSpeed <= config_.coneMaximumSpeed) {
                const float interval = config_.coneMaximumSpeed -
                                       config_.coneMinimumSpeed;
                alignment = interval == 0.0f
                                    ? config_.coneAperture
                                    : (config_.coneMaximumSpeed -
                                       vehicle.signedSpeed) *
                                                      (1.0f -
                                                       config_.coneAperture) /
                                                      interval +
                                              config_.coneAperture;
            } else {
                alignment = config_.coneAperture;
            }
        }
        if (alignment < 0.9998999834060669f) {
            const float maxAngle = alignment * Pi;
            float angle = CIatan2(-offset.x, -offset.z);
            angle = Clamp(angle, -maxAngle, maxAngle);
            const float length = HorizontalLength(offset);
            offset.x = -CIsin(angle) * length;
            offset.z = -CIcos(angle) * length;
        }

        GmVec3 look = Scale(offset, -1.0f);
        look.y -= (1.0f - config_.common.lookFactor) *
                  config_.common.height;
        const GmVec3 worldOffset = TransformDirection(tracking, offset);
        const GmVec3 worldLook = TransformDirection(tracking, look);
        SetCameraPose(camera_, Add(reference, worldOffset), worldLook,
                      vehicle.cameraSupportUp);
        camera_.lens = {config_.common.baseFieldOfView,
                        config_.common.lensParameter1,
                        config_.common.lensParameter2, -1.0f, -1.0f};
    }

    std::uint32_t targetId_ = ~std::uint32_t{0};
    RaceConfig config_;
};

class Race2Controller final : public TargetController {
  public:
    explicit Race2Controller(Race2Config config)
        : TargetController(config.common.initialFieldOfView),
          config_(std::move(config)),
          lookCurve_(config_.flyingLookCurve),
          rollFromSpeed_(config_.rollFromSpeedCurve),
          yawFromSpeed_(config_.yawFromSpeedCurve),
          gasDistance_(config_.smoothers[0].delta,
                       config_.smoothers[0].riseTimeMs,
                       config_.smoothers[0].fallTimeMs),
          brakeDistance_(config_.smoothers[1].delta,
                         config_.smoothers[1].riseTimeMs,
                         config_.smoothers[1].fallTimeMs),
          steerDistance_(config_.smoothers[2].delta,
                         config_.smoothers[2].riseTimeMs,
                         config_.smoothers[2].fallTimeMs),
          turboFov_(config_.smoothers[3].delta,
                    config_.smoothers[3].riseTimeMs,
                    config_.smoothers[3].fallTimeMs),
          turboDistance_(config_.smoothers[4].delta,
                         config_.smoothers[4].riseTimeMs,
                         config_.smoothers[4].fallTimeMs),
          gearDistance_(config_.smoothers[5].delta,
                        config_.smoothers[5].riseTimeMs,
                        config_.smoothers[5].fallTimeMs),
          burningLook_(config_.smoothers[6].delta,
                       config_.smoothers[6].riseTimeMs,
                       config_.smoothers[6].fallTimeMs),
          burningDistance_(config_.smoothers[7].delta,
                           config_.smoothers[7].riseTimeMs,
                           config_.smoothers[7].fallTimeMs),
          flyingDistance_(config_.smoothers[8].delta,
                          config_.smoothers[8].riseTimeMs,
                          config_.smoothers[8].fallTimeMs),
          flyingHeight_(config_.smoothers[9].delta,
                        config_.smoothers[9].riseTimeMs,
                        config_.smoothers[9].fallTimeMs),
          flyingLook_(config_.smoothers[10].delta,
                      config_.smoothers[10].riseTimeMs,
                      config_.smoothers[10].fallTimeMs),
          leftRoll_(config_.smoothers[11].delta,
                    config_.smoothers[11].riseTimeMs,
                    config_.smoothers[11].fallTimeMs),
          rightRoll_(config_.smoothers[12].delta,
                     config_.smoothers[12].riseTimeMs,
                     config_.smoothers[12].fallTimeMs),
          leftYaw_(config_.smoothers[13].delta,
                   config_.smoothers[13].riseTimeMs,
                   config_.smoothers[13].fallTimeMs),
          rightYaw_(config_.smoothers[14].delta,
                    config_.smoothers[14].riseTimeMs,
                    config_.smoothers[14].fallTimeMs) {
        camera_.lens = {config_.common.initialFieldOfView,
                        config_.common.lensParameter1,
                        config_.common.lensParameter2, -1.0f, -1.0f};
    }

    RaceCameraProfile Profile() const noexcept override {
        return RaceCameraProfile::Race2;
    }

    void Reset(const VehicleFrame &vehicle) override {
        targetId_ = vehicle.targetId;
        lastTimeMs_ = vehicle.timeMs;
        camera_.transform = vehicle.transform;
        camera_.transform.translation =
                Add(vehicle.transform.translation,
                    TransformDirection(vehicle.transform.rotation,
                                       {0.0f, 0.0f, -15.0f}));
        history_.Reset();
        state_ = CameraState::Ground;
        distance_ = distanceVelocity_ = 0.0f;
        look_ = lookVelocity_ = 0.0f;
        fovDelta_ = heightDelta_ = rawFlyingLook_ = 0.0f;
        steeringOrientationLatched_ = false;
        steeringOrientationSign_ = 0.0f;
        steeringOrientationStartDurationMs_ = 0u;
        lastRollAngle_ = 0.0f;
        lastYawAngle_ = 0.0f;
        ResetSmoothers();
    }

  private:
    void ResetSmoothers() {
        gasDistance_.Reset();
        brakeDistance_.Reset();
        steerDistance_.Reset();
        turboFov_.Reset();
        turboDistance_.Reset();
        gearDistance_.Reset();
        burningLook_.Reset();
        burningDistance_.Reset();
        flyingDistance_.Reset();
        flyingHeight_.Reset();
        flyingLook_.Reset();
        leftRoll_.Reset();
        rightRoll_.Reset();
        leftYaw_.Reset();
        rightYaw_.Reset();
    }

    void ApplySteeringOrientation(const VehicleFrame &vehicle,
                                  bool ground) {
        if (!config_.rollFromInput) {
            return;
        }

        const bool active = history_.steering && ground &&
                            vehicle.signedSpeed >=
                                    config_.orientationMinimumSpeed;
        if (history_.steering) {
            const float sign = history_.steeringSign < 0.0f ? -1.0f : 1.0f;
            if (!steeringOrientationLatched_ ||
                sign != steeringOrientationSign_) {
                steeringOrientationLatched_ = true;
                steeringOrientationSign_ = sign;
                steeringOrientationStartDurationMs_ =
                        history_.steeringDurationMs;
                lastRollAngle_ = 0.0f;
                lastYawAngle_ = 0.0f;
            }
        } else if (history_.steeringDurationMs == 0u) {
            steeringOrientationLatched_ = false;
            steeringOrientationSign_ = 0.0f;
        }

        const std::uint32_t elapsed =
                history_.steeringDurationMs >=
                                steeringOrientationStartDurationMs_
                        ? history_.steeringDurationMs -
                                  steeringOrientationStartDurationMs_
                        : 0u;
        const bool positive = steeringOrientationSign_ > 0.0f;
        const bool negative = steeringOrientationSign_ < 0.0f;
        const bool leftRollActive =
                active && positive &&
                (elapsed > config_.steeringRollTriggerMs ||
                 lastRollAngle_ < -0.100000001f);
        const bool rightRollActive =
                active && negative &&
                (elapsed > config_.steeringRollTriggerMs ||
                 lastRollAngle_ > 0.100000001f);
        const bool leftYawActive =
                active && positive &&
                (elapsed > config_.steeringYawTriggerMs ||
                 lastYawAngle_ < -0.100000001f);
        const bool rightYawActive =
                active && negative &&
                (elapsed > config_.steeringYawTriggerMs ||
                 lastYawAngle_ > 0.100000001f);

        const float rollMagnitude =
                rollFromSpeed_.Evaluate(vehicle.signedSpeed) * Pi / 180.0f;
        const float yawMagnitude =
                yawFromSpeed_.Evaluate(vehicle.signedSpeed) * Pi / 180.0f;
        const float roll = Clamp(
                -rollMagnitude *
                                leftRoll_.Update(leftRollActive,
                                                 vehicle.timeMs) +
                        rollMagnitude *
                                rightRoll_.Update(rightRollActive,
                                                  vehicle.timeMs),
                -Pi * 0.5f, Pi * 0.5f);
        const float yaw = Clamp(
                -yawMagnitude *
                                leftYaw_.Update(leftYawActive,
                                                vehicle.timeMs) +
                        yawMagnitude *
                                rightYaw_.Update(rightYawActive,
                                                 vehicle.timeMs),
                -Pi * 0.5f, Pi * 0.5f);
        lastRollAngle_ = roll;
        lastYawAngle_ = yaw;
        camera_.transform.rotation.RotateZ(roll);
        camera_.transform.rotation.RotateY(yaw);
    }

    void UpdateCamera(const VehicleFrame &vehicle,
                      const SegmentCollisionQuery &) override {
        if (vehicle.targetId != targetId_) {
            Reset(vehicle);
        }
        const std::uint32_t dtMs = DeltaTimeMs(vehicle.timeMs, lastTimeMs_);
        const float dt = static_cast<float>(dtMs) * MillisecondsToSeconds;
        lastTimeMs_ = vehicle.timeMs;

        // Stadium Race2 loads isRollFromInput = false, so the original skips
        // steering-duration, roll, yaw, and steering-distance input handling.
        if (vehicle.isVehicleCar) {
            if (config_.rollFromInput) {
                history_.UpdateSteering(
                        vehicle, dtMs, config_.steeringInactiveResetMs);
            }
            const CameraState previousState = state_;
            state_ = history_.UpdateState(
                    vehicle, state_, vehicle.AllWheelsWithoutContact(),
                    config_.airborneValidationMs);
            if (state_ != previousState && state_ == CameraState::Airborne) {
                airborneStateStartMs_ = vehicle.timeMs;
            }
        }

        const bool ground = state_ == CameraState::Ground;
        const bool burning = state_ == CameraState::Burning;
        const bool airborne = state_ == CameraState::Airborne;
        const bool burningSteer = false;

        float flyingCurveValue = 0.0f;
        bool flyingCameraMove = false;
        if (airborne && vehicle.timeMs >= airborneStateStartMs_) {
            const std::uint32_t duration =
                    vehicle.timeMs - airborneStateStartMs_;
            if (duration > config_.flyingCameraMoveDelayMs) {
                const float horizontalSpeed =
                        HorizontalLength(vehicle.linearSpeed);
                flyingCurveValue = lookCurve_.Evaluate(
                        -vehicle.linearSpeed.y / (horizontalSpeed + 1.0f));
                flyingCameraMove = true;
            }
        }

        const bool accelerate =
                vehicle.isVehicleCar && IsActive(vehicle.accelerate);
        const bool brake = vehicle.isVehicleCar && IsActive(vehicle.brake);
        const bool turbo = vehicle.isVehicleCar && vehicle.turbo != 0.0f;
        const bool gasActive = (accelerate && ground) || burning || airborne;
        const bool turboActive = turbo && (ground || airborne);
        const bool steerActive = config_.rollFromInput &&
                                 history_.steeringDurationMs != 0u && ground;
        const bool gearActive =
                vehicle.isVehicleCar && !turbo && vehicle.gearChanged && ground;
        const bool brakeActive = brake && ground;

        float competingDistance =
                gearDistance_.Update(gearActive, vehicle.timeMs);
        competingDistance =
                std::min(competingDistance,
                         steerDistance_.Update(steerActive, vehicle.timeMs));
        competingDistance =
                std::min(competingDistance,
                         brakeDistance_.Update(brakeActive, vehicle.timeMs));

        const float targetDistance =
                gasDistance_.Update(gasActive, vehicle.timeMs) +
                turboDistance_.Update(turboActive, vehicle.timeMs) +
                competingDistance +
                burningDistance_.Update(burningSteer, vehicle.timeMs) +
                flyingDistance_.Update(airborne, vehicle.timeMs);
        IntegrateSpring(targetDistance, config_.distanceSpringStiffness,
                        config_.distanceSpringDamping, dt, distance_,
                        distanceVelocity_);

        const float targetFov = turboFov_.Update(turboActive, vehicle.timeMs);
        fovDelta_ = RateLimit(fovDelta_, targetFov,
                              config_.fieldOfViewRate * dt);

        rawFlyingLook_ =
                RateLimit(rawFlyingLook_, flyingCurveValue,
                          config_.flyingLookRate * dt);
        const float targetLook =
                rawFlyingLook_ +
                burningLook_.Update(burningSteer, vehicle.timeMs) +
                flyingLook_.Update(airborne, vehicle.timeMs);
        IntegrateSpring(targetLook, config_.lookSpringStiffness,
                        config_.lookSpringDamping, dt, look_, lookVelocity_);

        const float targetHeight =
                flyingHeight_.Update(flyingCameraMove, vehicle.timeMs);
        heightDelta_ = RateLimit(heightDelta_, targetHeight,
                                 config_.heightRate * dt);

        const float minimumDistance =
                config_.common.minimumDistance + distance_;
        const float maximumDistance =
                config_.common.maximumDistance + distance_;
        const float height = config_.common.height + heightDelta_;
        const float lookFactor =
                Clamp(config_.common.lookFactor + look_, 0.0f, 1.0f);

        const GmMat3 tracking = TrackingFrame(VehicleForward(vehicle));
        const GmVec3 reference =
                Add(vehicle.transform.translation, Scale(WorldUp, height));
        GmVec3 offset = InverseTransformDirection(
                tracking, Subtract(camera_.transform.translation, reference));
        offset.y = 0.0f;
        float lengthSquared = offset.x * offset.x + offset.z * offset.z;
        if (lengthSquared <= VectorNormalizeEpsilonSquared) {
            offset = {0.0f, 0.0f, -minimumDistance};
            lengthSquared = minimumDistance * minimumDistance;
        }
        if (lengthSquared < minimumDistance * minimumDistance) {
            offset = Scale(offset, minimumDistance / CIsqrt(lengthSquared));
        } else if (lengthSquared > maximumDistance * maximumDistance) {
            offset = Scale(offset, maximumDistance / CIsqrt(lengthSquared));
        }

        GmVec3 localLook = Scale(offset, -1.0f);
        localLook.y -= (1.0f - lookFactor) * height;
        const GmVec3 worldOffset = TransformDirection(tracking, offset);
        const GmVec3 worldLook = TransformDirection(tracking, localLook);
        SetCameraPose(camera_, Add(reference, worldOffset), worldLook,
                      vehicle.cameraSupportUp);
        ApplySteeringOrientation(vehicle, ground);

        camera_.lens = {config_.common.baseFieldOfView + fovDelta_,
                        config_.common.lensParameter1,
                        config_.common.lensParameter2, -1.0f, -1.0f};
    }

    std::uint32_t targetId_ = ~std::uint32_t{0};
    std::uint32_t lastTimeMs_ = 0u;
    std::uint32_t airborneStateStartMs_ = 0u;
    CameraState state_ = CameraState::Ground;
    InputHistory history_{};
    Race2Config config_;
    LinearCurve lookCurve_;
    LinearCurve rollFromSpeed_;
    LinearCurve yawFromSpeed_;
    SmoothReal2 gasDistance_, brakeDistance_, steerDistance_;
    SmoothReal2 turboFov_, turboDistance_, gearDistance_;
    SmoothReal2 burningLook_, burningDistance_;
    SmoothReal2 flyingDistance_, flyingHeight_, flyingLook_;
    SmoothReal2 leftRoll_, rightRoll_, leftYaw_, rightYaw_;
    float distance_ = 0.0f;
    float distanceVelocity_ = 0.0f;
    float look_ = 0.0f;
    float lookVelocity_ = 0.0f;
    float fovDelta_ = 0.0f;
    float heightDelta_ = 0.0f;
    float rawFlyingLook_ = 0.0f;
    bool steeringOrientationLatched_ = false;
    float steeringOrientationSign_ = 0.0f;
    std::uint32_t steeringOrientationStartDurationMs_ = 0u;
    float lastRollAngle_ = 0.0f;
    float lastYawAngle_ = 0.0f;
};

class Race3Controller final : public TargetController {
  public:
    explicit Race3Controller(Race3Config config)
        : TargetController(config.common.initialFieldOfView),
          config_(std::move(config)),
          speedModulation_(config_.speedModulationCurve),
          lookCurve_(config_.flyingLookCurve),
          flyingDownCurve_(config_.flyingDownCurve),
          reverse_(config_.reverse.delta, config_.reverse.riseTimeMs,
                   config_.reverse.fallTimeMs),
          lowSpeedFlightTarget_(config_.lowSpeedFlightTarget.delta,
                                config_.lowSpeedFlightTarget.riseTimeMs,
                                config_.lowSpeedFlightTarget.fallTimeMs),
          flyingDirectionBlend_(config_.flyingDirectionBlend.delta,
                                config_.flyingDirectionBlend.riseTimeMs,
                                config_.flyingDirectionBlend.fallTimeMs),
          flyingUpBlend_(config_.flyingUpBlend.delta,
                         config_.flyingUpBlend.riseTimeMs,
                         config_.flyingUpBlend.fallTimeMs),
          flyingSpeedEffect_(config_.flyingSpeedEffect.delta,
                             config_.flyingSpeedEffect.riseTimeMs,
                             config_.flyingSpeedEffect.fallTimeMs),
          flyingUpSpeedEffect_(config_.flyingUpSpeedEffect.delta,
                               config_.flyingUpSpeedEffect.riseTimeMs,
                               config_.flyingUpSpeedEffect.fallTimeMs),
          flyingLook_(config_.flyingLook.delta,
                      config_.flyingLook.riseTimeMs,
                      config_.flyingLook.fallTimeMs),
          flyingRadius_(config_.flyingRadius.delta,
                        config_.flyingRadius.riseTimeMs,
                        config_.flyingRadius.fallTimeMs),
          gasFar_(config_.gasFar.delta, config_.gasFar.riseTimeMs,
                  config_.gasFar.fallTimeMs),
          brakeFar_(config_.brakeFar.delta, config_.brakeFar.riseTimeMs,
                    config_.brakeFar.fallTimeMs),
          steerFar_(config_.steerFar.delta, config_.steerFar.riseTimeMs,
                    config_.steerFar.fallTimeMs),
          turboFov_(config_.turboFieldOfView.delta,
                    config_.turboFieldOfView.riseTimeMs,
                    config_.turboFieldOfView.fallTimeMs),
          turboFar_(config_.turboFar.delta, config_.turboFar.riseTimeMs,
                    config_.turboFar.fallTimeMs),
          gearFar_(config_.gearFar.delta, config_.gearFar.riseTimeMs,
                   config_.gearFar.fallTimeMs),
          burningLook_(config_.burningLook.delta,
                       config_.burningLook.riseTimeMs,
                       config_.burningLook.fallTimeMs),
          burningRadius_(config_.burningRadius.delta,
                         config_.burningRadius.riseTimeMs,
                         config_.burningRadius.fallTimeMs) {
        initialRadius_ = CIsqrt(config_.up * config_.up +
                                config_.far * config_.far);
        camera_.lens = {config_.common.initialFieldOfView, initialRadius_,
                        config_.common.lensParameter2, -1.0f, -1.0f};
    }

    RaceCameraProfile Profile() const noexcept override {
        return RaceCameraProfile::Race3;
    }

    void Reset(const VehicleFrame &vehicle) override {
        targetId_ = vehicle.targetId;
        lastTimeMs_ = vehicle.timeMs;
        state_ = CameraState::Ground;
        history_.Reset();
        previousWheelSurfaceCount_ = 0u;

        camera_.transform = vehicle.transform;
        previousTarget_ = vehicle.transform.translation;
        if (vehicle.isVehicleCar && vehicle.AllWheelsWithoutSurface() &&
            camera_.transform.rotation.basisY.y < -0.5f) {
            camera_.transform.rotation.RotateZ(Pi);
        }

        const GmVec3 resetOffset = TransformDirection(
                camera_.transform.rotation,
                {0.0f, config_.up, -config_.far});
        camera_.transform.translation =
                Add(camera_.transform.translation, resetOffset);
        currentDirection_ = Normalize(resetOffset);
        idealDirection_ = currentDirection_;
        normalDirection_ = currentDirection_;
        flightTarget_ = currentDirection_;
        resetFlightDirection_ = currentDirection_;

        highFlightDirection_ = TransformDirection(
                camera_.transform.rotation,
                {0.0f, config_.up + config_.up, -config_.far});
        if (highFlightDirection_.y < 0.0f) {
            highFlightDirection_.y = -highFlightDirection_.y;
        }
        highFlightDirection_ = Normalize(highFlightDirection_);

        currentUp_ = camera_.transform.rotation.Row(GmAxis::Y);
        idealUp_ = currentUp_;
        normalUp_ = currentUp_;
        flyingUpTarget_ = WorldUp;
        fallbackUp_ = WorldUp;

        radius_ = initialRadius_;
        radiusVelocity_ = 0.0f;
        flyingDown_ = 0.0f;
        flyingDownVelocity_ = 0.0f;
        rawFlyingLook_ = 0.0f;
        ResetSmoothers();
    }

  private:
    void ResetSmoothers() {
        reverse_.Reset();
        lowSpeedFlightTarget_.Reset();
        flyingDirectionBlend_.Reset();
        flyingUpBlend_.Reset();
        flyingSpeedEffect_.Reset();
        flyingUpSpeedEffect_.Reset();
        flyingLook_.Reset();
        flyingRadius_.Reset();
        gasFar_.Reset();
        brakeFar_.Reset();
        steerFar_.Reset();
        turboFov_.Reset();
        turboFar_.Reset();
        gearFar_.Reset();
        burningLook_.Reset();
        burningRadius_.Reset();
    }

    void UpdateCamera(const VehicleFrame &vehicle,
                      const SegmentCollisionQuery &collision) override {
        if (vehicle.targetId != targetId_) {
            Reset(vehicle);
        }

        fallbackUp_ = camera_.transform.rotation.Row(GmAxis::Y);
        const std::uint32_t dtMs = DeltaTimeMs(vehicle.timeMs, lastTimeMs_);
        const float dt = static_cast<float>(dtMs) * MillisecondsToSeconds;
        lastTimeMs_ = vehicle.timeMs;

        if (vehicle.isVehicleCar) {
            history_.UpdateSteering(
                    vehicle, dtMs, config_.steeringInactiveResetMs);
            state_ = history_.UpdateState(
                    vehicle, state_, vehicle.AllWheelsWithoutSurface(),
                    config_.airborneValidationMs);
        }
        const bool ground = state_ == CameraState::Ground;
        const bool burning = state_ == CameraState::Burning;
        const bool airborne = state_ == CameraState::Airborne;

        bool burningSteer = false;
        if (vehicle.isVehicleCar && burning) {
            const std::uint32_t start =
                    std::max(history_.steeringStartMs, history_.burningStartMs);
            burningSteer = history_.steering && vehicle.timeMs >= start &&
                           vehicle.timeMs - start >
                                   config_.burningSteerDelayMs;
        }

        const GmVec3 linearSpeed =
                vehicle.isVehicleCar ? vehicle.linearSpeed : GmVec3{};
        const float horizontalSpeed = HorizontalLength(linearSpeed);
        float flyingLookTarget = 0.0f;
        if (airborne) {
            flyingLookTarget = lookCurve_.Evaluate(-linearSpeed.y /
                                                   (horizontalSpeed + 1.0f));
        }
        rawFlyingLook_ = RateLimit(rawFlyingLook_, flyingLookTarget,
                                   config_.flyingLookRate * dt);

        const bool accelerate =
                vehicle.isVehicleCar && IsActive(vehicle.accelerate);
        const bool brake = vehicle.isVehicleCar && IsActive(vehicle.brake);
        const bool turbo = vehicle.isVehicleCar && vehicle.turbo != 0.0f;
        const bool gasActive = (((accelerate && vehicle.signedSpeed > 2.0f) ||
                                 (brake && vehicle.signedSpeed < -2.0f)) &&
                                ground) ||
                               burning || airborne;
        const bool turboActive = turbo && (ground || airborne);
        const bool lowSpeed =
                std::fabs(vehicle.signedSpeed) < config_.lowSpeedThreshold;
        const bool veryLowSpeed = std::fabs(vehicle.signedSpeed) <
                                  config_.veryLowSpeedThreshold;
        const bool steerActive =
                history_.steeringDurationMs != 0u && ground && !lowSpeed;
        const bool gearActive =
                vehicle.isVehicleCar && !turbo && vehicle.gearChanged && ground;
        const bool brakeActive = brake && ground;

        const float fovDelta = turboFov_.Update(turboActive, vehicle.timeMs);
        float competingFar = gearFar_.Update(gearActive, vehicle.timeMs);
        competingFar = std::min(competingFar,
                                steerFar_.Update(steerActive, vehicle.timeMs));
        competingFar = std::min(competingFar,
                                brakeFar_.Update(brakeActive, vehicle.timeMs));
        const float farDelta = gasFar_.Update(gasActive, vehicle.timeMs) +
                               turboFar_.Update(turboActive, vehicle.timeMs) +
                               competingFar;
        const float horizontalRadius =
                std::max(config_.far + farDelta, 0.1f);

        const float radiusTarget =
                CIsqrt(config_.up * config_.up +
                        horizontalRadius * horizontalRadius) +
                burningRadius_.Update(burningSteer, vehicle.timeMs) +
                flyingRadius_.Update(airborne, vehicle.timeMs);
        IntegrateSpring(radiusTarget, config_.radiusSpringStiffness,
                        config_.radiusSpringDamping, dt, radius_,
                        radiusVelocity_);

        float downTarget = flyingDownCurve_.Evaluate(-linearSpeed.y /
                                                     (horizontalSpeed + 1.0f));
        downTarget = Clamp(downTarget, 0.0f, 0.949999988079071f);
        IntegrateSpring(downTarget, config_.flyingDownSpringStiffness,
                        config_.flyingDownSpringDamping, dt, flyingDown_,
                        flyingDownVelocity_);

        if (ground || burning) {
            const float reverseAngle =
                    reverse_.Update(vehicle.signedSpeed < -4.0f && ground,
                                    vehicle.timeMs) *
                    Pi;
            const GmVec3 localDirection{
                    CIsin(reverseAngle) * horizontalRadius, config_.up,
                    CIcos(reverseAngle) * -horizontalRadius};
            normalDirection_ = Normalize(TransformDirection(
                    vehicle.transform.rotation, localDirection));
            normalUp_ = VehicleUp(vehicle);
        } else if (airborne) {
            UpdateAirborneTargets(vehicle, horizontalRadius);
        }

        const bool lowSpeedFlight = veryLowSpeed &&
                                    vehicle.cameraFlightTransition != 0.0f &&
                                    airborne;
        flightTarget_ = SlerpVector(
                resetFlightDirection_, highFlightDirection_,
                lowSpeedFlightTarget_.Update(lowSpeedFlight, vehicle.timeMs));
        idealDirection_ = SlerpVector(normalDirection_, flightTarget_,
                                      flyingDirectionBlend_.UpdateUsingFullTime(
                                              airborne, vehicle.timeMs));
        idealUp_ = SlerpVector(
                normalUp_, flyingUpTarget_,
                flyingUpBlend_.UpdateUsingFullTime(airborne, vehicle.timeMs));

        ApplyCollision(vehicle, collision);
        previousTarget_ = vehicle.transform.translation;
        SmoothDirection(vehicle, dt, airborne);

        const float lookModifier =
                rawFlyingLook_ +
                burningLook_.Update(burningSteer, vehicle.timeMs) +
                flyingLook_.Update(airborne, vehicle.timeMs);
        BuildOutput(vehicle, lookModifier, fovDelta);
    }

    void UpdateAirborneTargets(const VehicleFrame &vehicle,
                               float horizontalRadius) {
        if (LengthSquared(Cross(currentDirection_, WorldUp)) >=
            VectorNormalizeEpsilonSquared) {
            GmVec3 horizontal{currentDirection_.x, 0.0f, currentDirection_.z};
            const float horizontalSquared =
                    horizontal.x * horizontal.x + horizontal.z * horizontal.z;
            if (horizontalSquared > VectorNormalizeEpsilonSquared) {
                const float inverse = 1.0f / CIsqrt(horizontalSquared);
                horizontal.x *= inverse;
                horizontal.z *= inverse;
            }
            highFlightDirection_ = Normalize(
                    {horizontal.x * horizontalRadius,
                     config_.up + config_.up,
                     horizontal.z * horizontalRadius});
        }

        GmVec3 movement = Subtract(Add(previousTarget_, currentDirection_),
                                   vehicle.transform.translation);
        const float movementSquared =
                movement.x * movement.x + movement.z * movement.z;
        if (movementSquared > VectorNormalizeEpsilonSquared) {
            const float inverse = 1.0f / CIsqrt(movementSquared);
            movement.x *= inverse;
            movement.z *= inverse;
        }
        const float lookDown =
                Clamp(config_.constantFlyingLookDownFactor + flyingDown_,
                      0.0f, 0.949999988079071f);
        const float horizontalScale = CIsin((1.0f - lookDown) * Pi * 0.5f);
        resetFlightDirection_ = {movement.x * horizontalScale,
                                 CIsin(lookDown * Pi * 0.5f),
                                 movement.z * horizontalScale};
        flyingUpTarget_ = WorldUp;
    }

    void ApplyCollision(const VehicleFrame &vehicle,
                        const SegmentCollisionQuery &collision) {
        if (!vehicle.isVehicleCar) {
            return;
        }
        const std::uint32_t surfaceCount = static_cast<std::uint32_t>(
                std::count(vehicle.wheelHasSurface.begin(),
                           vehicle.wheelHasSurface.end(), true));
        const std::uint32_t wheelCount =
                static_cast<std::uint32_t>(vehicle.wheelHasSurface.size());
        if (collision && surfaceCount * 2u <= wheelCount &&
            previousWheelSurfaceCount_ * 2u <= wheelCount) {
            const GmVec3 start = Add(vehicle.transform.translation,
                                     Scale(currentDirection_, radius_));
            const GmVec3 end = Add(
                    start,
                    {0.0f, -2.0f * config_.collisionRadius, 0.0f});
            const auto hit = collision({ToPublic(start), ToPublic(end)});
            if (hit && hit->fraction < 1.0f) {
                const float hitY = start.y + (end.y - start.y) * hit->fraction;
                float requiredY = (hitY + config_.collisionRadius -
                                   vehicle.transform.translation.y) /
                                  radius_;
                if (idealDirection_.y < requiredY) {
                    requiredY = Clamp(requiredY, -1.0f, 1.0f);
                    const float horizontalSquared =
                            idealDirection_.x * idealDirection_.x +
                            idealDirection_.z * idealDirection_.z;
                    const float scaleSquared =
                            (1.0f - requiredY * requiredY) / horizontalSquared;
                    const float scale =
                            scaleSquared <= VectorNormalizeEpsilonSquared
                                    ? 0.0f
                                    : CIsqrt(scaleSquared);
                    idealDirection_.x *= scale;
                    idealDirection_.y = requiredY;
                    idealDirection_.z *= scale;
                }
            }
        }
        previousWheelSurfaceCount_ = surfaceCount;
    }

    void SmoothDirection(const VehicleFrame &vehicle, float dt, bool airborne) {
        const float directionDot = Dot(currentDirection_, idealDirection_);
        const float directionAngle = CIacos(Clamp(directionDot, -1.0f, 1.0f));
        const float directionDenominator = directionAngle * 2.0f + 1.0f;
        const float directionAirWeight =
                flyingSpeedEffect_.Update(airborne, vehicle.timeMs);
        const float directionFlyingStep =
                Clamp(config_.directionFlyingRate * dt /
                              directionDenominator,
                      0.0f, 1.0f);
        const float directionNormalStep =
                Clamp(config_.directionNormalRate * dt /
                              directionDenominator,
                      0.0f, 1.0f) *
                speedModulation_.Evaluate(vehicle.signedSpeed);
        const float directionBlend =
                directionAirWeight * directionFlyingStep +
                (1.0f - directionAirWeight) * directionNormalStep;
        currentDirection_ =
                SlerpVector(currentDirection_, idealDirection_, directionBlend);

        const float upDot = Dot(currentUp_, idealUp_);
        const float upAngle = CIacos(Clamp(upDot, -1.0f, 1.0f));
        const float upDenominator = upAngle * 2.0f + 1.0f;
        const float upAirWeight =
                flyingUpSpeedEffect_.Update(airborne, vehicle.timeMs);
        const float upFlyingStep =
                Clamp(config_.upFlyingRate * dt / upDenominator,
                      0.0f, 1.0f);
        const float upNormalStep =
                Clamp(config_.upNormalRate * dt / upDenominator,
                      0.0f, 1.0f) *
                speedModulation_.Evaluate(vehicle.signedSpeed);
        const float upBlend = upAirWeight * upFlyingStep +
                              (1.0f - upAirWeight) * upNormalStep;
        currentUp_ = SlerpVector(currentUp_, idealUp_, upBlend);
    }

    void BuildOutput(const VehicleFrame &vehicle, float lookModifier,
                     float fovDelta) {
        const GmVec3 target = vehicle.transform.translation;
        const GmVec3 position = Add(target, Scale(currentDirection_, radius_));
        const float lookFactor =
                Clamp(config_.common.lookFactor + lookModifier, 0.0f, 1.0f);
        const GmVec3 lookTarget =
                Add(target, Scale(currentUp_, lookFactor * config_.up));
        const GmVec3 direction = Subtract(lookTarget, position);
        const GmVec3 up = LengthSquared(Cross(currentUp_, direction)) <
                                          VectorNormalizeEpsilonSquared
                                  ? fallbackUp_
                                  : currentUp_;
        SetCameraPose(camera_, position, direction, up);
        camera_.lens = {config_.common.baseFieldOfView + fovDelta, radius_,
                        config_.common.lensParameter2, -1.0f, -1.0f};
    }

    std::uint32_t targetId_ = ~std::uint32_t{0};
    std::uint32_t lastTimeMs_ = 0u;
    std::uint32_t previousWheelSurfaceCount_ = 0u;
    CameraState state_ = CameraState::Ground;
    InputHistory history_{};
    Race3Config config_;
    LinearCurve speedModulation_;
    LinearCurve lookCurve_;
    LinearCurve flyingDownCurve_;
    SmoothReal2 reverse_;
    SmoothReal2 lowSpeedFlightTarget_;
    SmoothReal2 flyingDirectionBlend_;
    SmoothReal2 flyingUpBlend_;
    SmoothReal2 flyingSpeedEffect_;
    SmoothReal2 flyingUpSpeedEffect_;
    SmoothReal2 flyingLook_;
    SmoothReal2 flyingRadius_;
    SmoothReal2 gasFar_;
    SmoothReal2 brakeFar_;
    SmoothReal2 steerFar_;
    SmoothReal2 turboFov_;
    SmoothReal2 turboFar_;
    SmoothReal2 gearFar_;
    SmoothReal2 burningLook_;
    SmoothReal2 burningRadius_;
    GmVec3 previousTarget_{};
    GmVec3 currentDirection_{};
    GmVec3 idealDirection_{};
    GmVec3 normalDirection_{};
    GmVec3 flightTarget_{};
    GmVec3 resetFlightDirection_{};
    GmVec3 highFlightDirection_{};
    GmVec3 currentUp_{0.0f, 1.0f, 0.0f};
    GmVec3 idealUp_{0.0f, 1.0f, 0.0f};
    GmVec3 normalUp_{0.0f, 1.0f, 0.0f};
    GmVec3 flyingUpTarget_{0.0f, 1.0f, 0.0f};
    GmVec3 fallbackUp_{0.0f, 1.0f, 0.0f};
    float initialRadius_ = 0.0f;
    float radius_ = 0.0f;
    float radiusVelocity_ = 0.0f;
    float flyingDown_ = 0.0f;
    float flyingDownVelocity_ = 0.0f;
    float rawFlyingLook_ = 0.0f;
};
} // namespace

std::unique_ptr<Controller> CreateController(
        const EnvironmentConfig &environment,
        RaceCameraProfile profile,
        std::optional<std::size_t> resourceIndex) {
    switch (profile) {
    case RaceCameraProfile::Race: {
        if (resourceIndex) {
            return *resourceIndex < environment.raceResources.size()
                           ? std::make_unique<RaceController>(
                                     environment.raceResources[*resourceIndex]
                                             .config)
                           : nullptr;
        }
        return environment.race
                       ? std::make_unique<RaceController>(*environment.race)
                       : nullptr;
    }
    case RaceCameraProfile::Race2: {
        if (resourceIndex) {
            return *resourceIndex < environment.race2Resources.size()
                           ? std::make_unique<Race2Controller>(
                                     environment.race2Resources[*resourceIndex]
                                             .config)
                           : nullptr;
        }
        return environment.race2
                       ? std::make_unique<Race2Controller>(*environment.race2)
                       : nullptr;
    }
    case RaceCameraProfile::Race3: {
        if (resourceIndex) {
            return *resourceIndex < environment.race3Resources.size()
                           ? std::make_unique<Race3Controller>(
                                     environment.race3Resources[*resourceIndex]
                                             .config)
                           : nullptr;
        }
        return environment.race3
                       ? std::make_unique<Race3Controller>(*environment.race3)
                       : nullptr;
    }
    }
    return nullptr;
}

} // namespace forevervalidator::camera::detail
