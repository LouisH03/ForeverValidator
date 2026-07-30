#include "engine/camera/race_camera_internal.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "engine/core/binary32_math.h"

namespace forevervalidator::camera::detail {

LinearCurve::LinearCurve(std::initializer_list<CurveKey> keys) : keys_(keys) {}

LinearCurve::LinearCurve(std::vector<CurveKey> keys)
    : keys_(std::move(keys)) {}

float LinearCurve::Evaluate(float position) const {
    if (keys_.empty()) {
        return 0.0f;
    }
    if (position <= keys_.front().position) {
        return keys_.front().value;
    }
    for (std::size_t index = 1u; index < keys_.size(); ++index) {
        if (position <= keys_[index].position) {
            const CurveKey &left = keys_[index - 1u];
            const CurveKey &right = keys_[index];
            const float interval = right.position - left.position;
            if (interval <= 0.0f) {
                return right.value;
            }
            const float blend = (position - left.position) / interval;
            return (right.value - left.value) * blend + left.value;
        }
    }
    return keys_.back().value;
}

SmoothReal2::SmoothReal2(float delta, std::uint32_t riseTimeMs,
                         std::uint32_t fallTimeMs)
    : delta_(delta), riseTimeMs_(riseTimeMs), fallTimeMs_(fallTimeMs) {
    Reset();
}

void SmoothReal2::Reset() {
    transitionScale_ = 1.0f;
    value_ = 0.0f;
    transitionStartTimeMs_ = 0u;
    transitionStartValue_ = 0.0f;
    direction_ = SmoothDirection::Falling;
}

float SmoothReal2::SmoothedValue(SmoothDirection direction, float position) {
    const float clamped = Clamp(position, 0.0f, 1.0f);
    const float cosine = CIcos(clamped * Pi);
    if (direction == SmoothDirection::Rising) {
        return (1.0f - cosine) * 0.5f;
    }
    return (cosine + 1.0f) * 0.5f;
}

float SmoothReal2::SmoothedPosition(SmoothDirection direction, float value) {
    float clamped = 0.000009999999747378752f;
    if (value > clamped) {
        clamped = value;
        if (value >= 0.999989986419677734375f) {
            clamped = 0.999989986419677734375f;
        }
    }
    if (direction == SmoothDirection::Rising) {
        return CIacos(1.0f - (clamped + clamped)) / Pi;
    }
    return CIacos(clamped + clamped - 1.0f) / Pi;
}

void SmoothReal2::SetDirection(SmoothDirection direction,
                               std::uint32_t timeMs) {
    transitionScale_ = SmoothedPosition(direction_, std::fabs(value_ / delta_));
    direction_ = direction;
    transitionStartTimeMs_ = timeMs;
    transitionStartValue_ = value_;
}

float SmoothReal2::Update(bool active, std::uint32_t timeMs) {
    if (active) {
        if (direction_ != SmoothDirection::Rising) {
            SetDirection(SmoothDirection::Rising, timeMs);
        }
        if (std::fabs(delta_) > std::fabs(value_) &&
            timeMs >= transitionStartTimeMs_ &&
            std::fabs(transitionScale_) > 0.000009999999747378752f &&
            riseTimeMs_ != 0u) {
            const float position =
                    static_cast<float>(timeMs - transitionStartTimeMs_) /
                    (static_cast<float>(riseTimeMs_) * transitionScale_);
            value_ = SmoothedValue(direction_, position) *
                             (delta_ - transitionStartValue_) +
                     transitionStartValue_;
        }
    } else {
        if (direction_ == SmoothDirection::Rising) {
            SetDirection(SmoothDirection::Falling, timeMs);
        }
        if (std::fabs(value_) > 0.0f && timeMs >= transitionStartTimeMs_ &&
            std::fabs(transitionScale_) > 0.000009999999747378752f &&
            fallTimeMs_ != 0u) {
            const float position =
                    static_cast<float>(timeMs - transitionStartTimeMs_) /
                    (static_cast<float>(fallTimeMs_) * transitionScale_);
            value_ =
                    SmoothedValue(direction_, position) * transitionStartValue_;
        }
    }
    return value_;
}

float SmoothReal2::UpdateUsingFullTime(bool active, std::uint32_t timeMs) {
    if (active) {
        if (direction_ != SmoothDirection::Rising) {
            direction_ = SmoothDirection::Rising;
            transitionStartTimeMs_ = timeMs;
            transitionStartValue_ = value_;
        }
        if (std::fabs(delta_) > std::fabs(value_) &&
            timeMs >= transitionStartTimeMs_ && transitionScale_ != 0.0f) {
            const float position =
                    static_cast<float>(timeMs - transitionStartTimeMs_) /
                    static_cast<float>(riseTimeMs_);
            value_ = SmoothedValue(direction_, position) *
                             (delta_ - transitionStartValue_) +
                     transitionStartValue_;
        }
    } else {
        if (direction_ == SmoothDirection::Rising) {
            direction_ = SmoothDirection::Falling;
            transitionStartTimeMs_ = timeMs;
            transitionStartValue_ = value_;
        }
        if (std::fabs(value_) > 0.0f && timeMs >= transitionStartTimeMs_ &&
            transitionScale_ != 0.0f) {
            const float position =
                    static_cast<float>(timeMs - transitionStartTimeMs_) /
                    static_cast<float>(fallTimeMs_);
            value_ =
                    SmoothedValue(direction_, position) * transitionStartValue_;
        }
    }
    return value_;
}

bool VehicleFrame::AllWheelsWithoutContact() const noexcept {
    return std::none_of(wheelContact.begin(), wheelContact.end(),
                        [](bool contact) { return contact; });
}

bool VehicleFrame::AllWheelsWithoutSurface() const noexcept {
    return std::none_of(wheelHasSurface.begin(), wheelHasSurface.end(),
                        [](bool hasSurface) { return hasSurface; });
}

TargetController::TargetController(float fieldOfViewDegrees) {
    camera_.transform.SetIdentity();
    camera_.lens = {fieldOfViewDegrees, fieldOfViewDegrees, 3.0f, -1.0f, -1.0f};
}

CameraFrame
TargetController::Evaluate(const VehicleFrame &vehicle,
                           const SegmentCollisionQuery &segmentCollision) {
    UpdateCamera(vehicle, segmentCollision);
    return camera_;
}

GmVec3 ToInternal(const Vector3 &value) { return {value.x, value.y, value.z}; }

Vector3 ToPublic(const GmVec3 &value) { return {value.x, value.y, value.z}; }

GmQuat ToInternal(const Quaternion &value) {
    GmQuat result{value.w, value.x, value.y, value.z};
    result.Normalize();
    return result;
}

Quaternion ToPublic(const GmQuat &value) {
    return {value.w, value.x, value.y, value.z};
}

GmIso4 ToInternal(const Transform &value) {
    GmIso4 result;
    GmMat3 rotation;
    rotation.Set(ToInternal(value.rotation));
    result.Set(rotation, ToInternal(value.position));
    return result;
}

Transform ToPublic(const GmIso4 &value) {
    GmQuat rotation;
    rotation.Set(value.rotation);
    rotation.Normalize();
    return {ToPublic(rotation), ToPublic(value.translation)};
}

VehicleFrame ToInternal(const RaceCameraVehicleState &value) {
    VehicleFrame result;
    result.targetId = value.targetId;
    result.timeMs = value.timeMs;
    result.transform = ToInternal(value.transform);
    result.linearSpeed = ToInternal(value.linearSpeed);
    result.signedSpeed = value.signedSpeed;
    result.steering = value.steering;
    result.accelerate = value.accelerate;
    result.brake = value.brake;
    result.turbo = value.turbo;
    result.cameraFlightTransition = value.cameraFlightTransition;
    result.burning = value.burning;
    result.gearChanged = value.gearChanged;
    result.isVehicleCar = value.isVehicleCar;
    result.wheelContact = value.wheelContact;
    result.wheelHasSurface = value.wheelHasSurface;
    result.cameraSupportUp = ToInternal(value.cameraSupportUp);
    return result;
}

RaceCameraOutput ToPublic(const CameraFrame &value) {
    RaceCameraOutput result;
    result.transform = ToPublic(value.transform);
    result.lens.fieldOfViewDegrees = value.lens[0];
    result.lens.parameter1 = value.lens[1];
    result.lens.parameter2 = value.lens[2];
    result.lens.nearClipDistance = value.lens[3];
    result.lens.farClipDistance = value.lens[4];
    return result;
}

float Clamp(float value, float minimum, float maximum) {
    return std::max(minimum, std::min(maximum, value));
}

GmVec3 Add(const GmVec3 &left, const GmVec3 &right) {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

GmVec3 Subtract(const GmVec3 &left, const GmVec3 &right) {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

GmVec3 Scale(const GmVec3 &value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

float LengthSquared(const GmVec3 &value) {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

GmVec3 Normalize(const GmVec3 &value) {
    const float lengthSquared = LengthSquared(value);
    if (lengthSquared <= VectorNormalizeEpsilonSquared) {
        return value;
    }
    return Scale(value, 1.0f / CIsqrt(lengthSquared));
}

GmVec3 Cross(const GmVec3 &left, const GmVec3 &right) {
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

float Dot(const GmVec3 &left, const GmVec3 &right) {
    return (left.x * right.x + left.y * right.y) + left.z * right.z;
}

GmVec3 TransformDirection(const GmMat3 &rotation, const GmVec3 &value) {
    return {rotation.basisX.x * value.x + rotation.basisX.y * value.y +
                    rotation.basisX.z * value.z,
            rotation.basisY.x * value.x + rotation.basisY.y * value.y +
                    rotation.basisY.z * value.z,
            rotation.basisZ.x * value.x + rotation.basisZ.y * value.y +
                    rotation.basisZ.z * value.z};
}

GmVec3 InverseTransformDirection(const GmMat3 &rotation, const GmVec3 &value) {
    return {rotation.basisX.x * value.x + rotation.basisY.x * value.y +
                    rotation.basisZ.x * value.z,
            rotation.basisX.y * value.x + rotation.basisY.y * value.y +
                    rotation.basisZ.y * value.z,
            rotation.basisX.z * value.x + rotation.basisY.z * value.y +
                    rotation.basisZ.z * value.z};
}

void SetDovAndUp(GmMat3 &rotation, const GmVec3 &directionOfView,
                 const GmVec3 &up) {
    GmVec3 side = Cross(up, directionOfView);
    const float sideLengthSquared = LengthSquared(side);
    if (sideLengthSquared > VectorNormalizeEpsilonSquared) {
        side = Scale(side, 1.0f / CIsqrt(sideLengthSquared));
    }

    GmVec3 normalizedDirection = directionOfView;
    const float directionLengthSquared = LengthSquared(normalizedDirection);
    if (directionLengthSquared > VectorNormalizeEpsilonSquared) {
        normalizedDirection = Scale(normalizedDirection,
                                    1.0f / CIsqrt(directionLengthSquared));
    }

    rotation.SetRow(GmAxis::X, side);
    rotation.SetRow(GmAxis::Y, Cross(normalizedDirection, side));
    rotation.SetRow(GmAxis::Z, normalizedDirection);
}

GmVec3 SlerpVector(const GmVec3 &from, const GmVec3 &to, float blend) {
    if (blend == 0.0f) {
        return from;
    }
    if (blend == 1.0f) {
        return to;
    }
    const GmVec3 cross = Cross(from, to);
    const float sine = CIsqrt(LengthSquared(cross));
    const float dot = Dot(from, to);
    float fromWeight = 1.0f - blend;
    float toWeight = blend;
    if (1.0f - dot > VectorNormalizeEpsilonSquared) {
        const float angle = CIasin(Clamp(sine, -1.0f, 1.0f));
        fromWeight = CIsin((1.0f - blend) * angle) / sine;
        toWeight = CIsin(blend * angle) / sine;
    }
    return Add(Scale(from, fromWeight), Scale(to, toWeight));
}

std::uint32_t DeltaTimeMs(std::uint32_t now, std::uint32_t before) {
    if (now < before) {
        return 0u;
    }
    return std::min<std::uint32_t>(now - before, 200u);
}

float RateLimit(float current, float target, float maximumStep) {
    const float delta = target - current;
    if (std::fabs(delta) <= maximumStep) {
        return target;
    }
    return current + (delta < 0.0f ? -maximumStep : maximumStep);
}

} // namespace forevervalidator::camera::detail
