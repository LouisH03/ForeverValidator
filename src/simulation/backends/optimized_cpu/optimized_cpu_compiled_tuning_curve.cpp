#include "simulation/backends/optimized_cpu/optimized_cpu_compiled_tuning_curve.h"

#include <cfenv>
#include <cmath>
#include <cstring>
#include <new>
#include <utility>

#include "engine/core/binary32_math.h"
#include "engine/core/func_keys_real.h"
#include "simulation/backends/optimized_cpu/optimized_cpu_binary32_math.h"

#if (defined(__i386__) || defined(__x86_64__)) && \
        (defined(__GNUC__) || defined(__clang__))
#define FV_COMPILED_CURVE_X86_SSE2 1
#include <emmintrin.h>
#else
#define FV_COMPILED_CURVE_X86_SSE2 0
#endif

namespace forevervalidator::simulation {
namespace {

constexpr float CurveKeyEpsilon = 1.0e-5f;

class FloatingEnvironmentRestore final {
public:
    FloatingEnvironmentRestore() noexcept
        : captured_(std::fegetenv(&environment_) == 0) {}

    ~FloatingEnvironmentRestore() {
        if (captured_) {
            std::fesetenv(&environment_);
        }
    }

    FloatingEnvironmentRestore(const FloatingEnvironmentRestore &) = delete;
    FloatingEnvironmentRestore &operator=(
            const FloatingEnvironmentRestore &) = delete;

    bool Established(void) const noexcept { return captured_; }

private:
    std::fenv_t environment_{};
    bool captured_ = false;
};

float ExactNativeFromDouble(double value) noexcept {
    return OptimizedCpuBinary32FromDoubleX86Sse2(value);
}

std::uint32_t FloatBits(float value) noexcept {
    std::uint32_t bits = 0u;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

struct CompiledBound {
    float value = 0.0f;
    bool raisesInexact = false;
};

CompiledBound BuildBound(float position, bool upper) noexcept {
    std::feclearexcept(FE_ALL_EXCEPT);
    volatile double widePosition = static_cast<double>(position);
    volatile double wideEpsilon = static_cast<double>(CurveKeyEpsilon);
    volatile double source = upper
            ? widePosition + wideEpsilon
            : widePosition - wideEpsilon;
    const float value = Binary32::FromDouble(source);
    return {
            value,
            std::fetestexcept(FE_INEXACT) != 0,
    };
}

float ExactSpeedInput(float speed) noexcept {
    const std::uint32_t magnitude = FloatBits(speed) & 0x7fffffffu;
    const double converted =
            static_cast<double>(speed) * static_cast<double>(3.6f);
    if (magnitude != 0u && magnitude < 0x00800000u) {
        return Binary32::FromDouble(converted);
    }
    return ExactNativeFromDouble(converted);
}

bool IsWithinBounds(float input, float lower, float upper) noexcept {
    return !std::isnan(input) && !std::isnan(lower) &&
           !std::isnan(upper) && input >= lower && input <= upper;
}

#if FV_COMPILED_CURVE_X86_SSE2
__attribute__((target("sse2")))
void RaiseInexactStatus(void) noexcept {
    constexpr unsigned int InexactStatus = 0x20u;
    const unsigned int current = _mm_getcsr();
    if ((current & InexactStatus) == 0u) {
        _mm_setcsr(current | InexactStatus);
    }
}
#else
void RaiseInexactStatus(void) noexcept {
    std::feraiseexcept(FE_INEXACT);
}
#endif

}  // namespace

bool OptimizedCpuCompiledTuningCurve::TryBuild(
        const CFuncKeysReal &curve) noexcept {
    const FloatingEnvironmentRestore floatingEnvironmentRestore;
    if (!floatingEnvironmentRestore.Established()) {
        Clear();
        return false;
    }
    try {
        OptimizedCpuCompiledTuningCurve rebuilt;
        rebuilt.source_ = &curve;
        rebuilt.sourceStorageRevision_ = curve.StorageRevision();
        rebuilt.interpolation_ =
                static_cast<unsigned>(curve.Interpolation());
        const std::size_t count = curve.KeyCount();
        rebuilt.positions_.resize(count);
        rebuilt.values_.resize(count);
        rebuilt.lowerBounds_.resize(count);
        rebuilt.upperBounds_.resize(count);
        rebuilt.lowerRaisesInexact_.resize(count);
        rebuilt.upperRaisesInexact_.resize(count);
        for (std::size_t index = 0u; index < count; ++index) {
            const float position = curve.XAt(index);
            if (!std::isfinite(position)) {
                Clear();
                return false;
            }
            rebuilt.positions_[index] = position;
            rebuilt.values_[index] = curve.ValueAt(index);
            const CompiledBound lower = BuildBound(position, false);
            const CompiledBound upper = BuildBound(position, true);
            rebuilt.lowerBounds_[index] = lower.value;
            rebuilt.upperBounds_[index] = upper.value;
            rebuilt.lowerRaisesInexact_[index] = lower.raisesInexact;
            rebuilt.upperRaisesInexact_[index] = upper.raisesInexact;
        }
        *this = std::move(rebuilt);
        return true;
    } catch (const std::bad_alloc &) {
        Clear();
        return false;
    }
}

void OptimizedCpuCompiledTuningCurve::Clear(void) noexcept {
    source_ = nullptr;
    positions_.clear();
    values_.clear();
    lowerBounds_.clear();
    upperBounds_.clear();
    lowerRaisesInexact_.clear();
    upperRaisesInexact_.clear();
    sourceStorageRevision_ = 0u;
    interpolation_ = 0u;
}

bool OptimizedCpuCompiledTuningCurve::IsFor(
        const CFuncKeysReal &curve) const noexcept {
    return IsStorageFor(curve) &&
           interpolation_ == static_cast<unsigned>(curve.Interpolation());
}

bool OptimizedCpuCompiledTuningCurve::IsStorageFor(
        const CFuncKeysReal &curve) const noexcept {
    return source_ == &curve &&
           sourceStorageRevision_ == curve.StorageRevision() &&
           positions_.size() == curve.KeyCount() &&
           values_.size() == positions_.size() &&
           lowerBounds_.size() == positions_.size() &&
           upperBounds_.size() == positions_.size() &&
           lowerRaisesInexact_.size() == positions_.size() &&
           upperRaisesInexact_.size() == positions_.size();
}

OptimizedCpuCompiledTuningCurve::Lookup
OptimizedCpuCompiledTuningCurve::LookupFor(float input) const
        noexcept {
    const std::size_t count = positions_.size();
    if (count == 0u) {
        return {};
    }

    Lookup result;
    if (count == 1u) {
        return result;
    }
    result.raisesInexact = lowerRaisesInexact_.front() != 0u;
    if (input < lowerBounds_.front()) {
        return result;
    }
    result.raisesInexact = result.raisesInexact ||
            upperRaisesInexact_.back() != 0u;
    if (input > upperBounds_.back()) {
        result.index = count - 1u;
        return result;
    }

    std::size_t current = 0u;
    for (std::size_t scanned = 0u; scanned <= count; ++scanned) {
        const std::size_t next = current + 1u < count
                ? current + 1u
                : 0u;
        result.raisesInexact = result.raisesInexact ||
                lowerRaisesInexact_[current] != 0u ||
                upperRaisesInexact_[next] != 0u;
        if (IsWithinBounds(
                    input,
                    lowerBounds_[current],
                    upperBounds_[next])) {
            result.index = current;
            return result;
        }
        current = next;
    }
    result.index = current;
    return result;
}

void OptimizedCpuCompiledTuningCurve::PreserveStatus(
        const Lookup &lookup) noexcept {
    if (lookup.raisesInexact) {
        RaiseInexactStatus();
    }
}

float OptimizedCpuCompiledTuningCurve::Evaluate(float input) const noexcept {
    const std::size_t count = positions_.size();
    if (count == 0u) {
        return 0.0f;
    }

    const Lookup lookup = LookupFor(input);
    PreserveStatus(lookup);
    const std::size_t keyIndex = lookup.index;
    const std::size_t nextKeyIndex = keyIndex + 1u < count
            ? keyIndex + 1u
            : 0u;
    const bool clampedLow = count == 1u || input < lowerBounds_.front();
    const bool clampedHigh = input > upperBounds_.back();
    const std::size_t effectiveNext = clampedLow || clampedHigh
            ? keyIndex
            : nextKeyIndex;

    float blend = 0.0f;
    if (keyIndex != effectiveNext) {
        const float x0 = positions_[keyIndex];
        const float span = positions_[effectiveNext] - x0;
        blend = std::fabs(span) >= CurveKeyEpsilon
                ? (input - x0) / span
                : 0.0f;
    }
    if (keyIndex == effectiveNext ||
        interpolation_ == static_cast<unsigned>(CFuncKeysReal::Constant)) {
        return values_[keyIndex];
    }

    const float value0 = values_[keyIndex];
    const float value1 = values_[effectiveNext];
    return (1.0f - blend) * value0 + blend * value1;
}

float OptimizedCpuCompiledTuningCurve::EvaluateConstant(float input) const
        noexcept {
    if (positions_.empty()) {
        return 0.0f;
    }
    const Lookup lookup = LookupFor(input);
    PreserveStatus(lookup);
    const std::size_t count = positions_.size();
    const std::size_t keyIndex = lookup.index;
    const std::size_t nextKeyIndex = keyIndex + 1u < count
            ? keyIndex + 1u
            : 0u;
    const bool clampedLow = count == 1u || input < lowerBounds_.front();
    const bool clampedHigh = input > upperBounds_.back();
    const std::size_t effectiveNext = clampedLow || clampedHigh
            ? keyIndex
            : nextKeyIndex;
    if (keyIndex != effectiveNext) {
        const float x0 = positions_[keyIndex];
        const float span = positions_[effectiveNext] - x0;
        volatile float ignoredBlend = std::fabs(span) >= CurveKeyEpsilon
                ? (input - x0) / span
                : 0.0f;
        static_cast<void>(ignoredBlend);
    }
    return values_[keyIndex];
}

float OptimizedCpuCompiledTuningCurve::ConvertSpeedToKmh(float speed)
        noexcept {
    return ExactSpeedInput(speed);
}

float OptimizedCpuCompiledTuningCurve::EvaluateSpeed(float speed) const
        noexcept {
    const float kilometersPerHour = ConvertSpeedToKmh(speed);
    return Evaluate(kilometersPerHour);
}

float OptimizedCpuCompiledTuningCurve::EvaluateSpeedConstant(float speed) const
        noexcept {
    const float kilometersPerHour = ConvertSpeedToKmh(speed);
    return EvaluateConstant(kilometersPerHour);
}

}  // namespace forevervalidator::simulation

#undef FV_COMPILED_CURVE_X86_SSE2
