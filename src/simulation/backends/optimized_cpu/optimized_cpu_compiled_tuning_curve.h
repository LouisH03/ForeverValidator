#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class CFuncKeysReal;

namespace forevervalidator::simulation {

// Immutable exact sidecar for the hot vehicle-tuning curve representation.
// Construction is allowed to allocate and precompute key tolerances; Evaluate
// performs no allocation and retains the authoritative interpolation order.
class OptimizedCpuCompiledTuningCurve final {
public:
    bool TryBuild(const CFuncKeysReal &curve) noexcept;
    void Clear(void) noexcept;

    bool IsFor(const CFuncKeysReal &curve) const noexcept;
    bool IsStorageFor(const CFuncKeysReal &curve) const noexcept;
    static float ConvertSpeedToKmh(float speed) noexcept;
    float Evaluate(float input) const noexcept;
    float EvaluateSpeed(float speed) const noexcept;
    float EvaluateConstant(float input) const noexcept;
    float EvaluateSpeedConstant(float speed) const noexcept;

private:
    struct Lookup {
        std::size_t index = 0u;
        bool raisesInexact = false;
        bool clamped = false;
    };

    Lookup LookupFor(float input) const noexcept;
    static void PreserveStatus(const Lookup &lookup) noexcept;

    const CFuncKeysReal *source_ = nullptr;
    std::vector<float> positions_;
    std::vector<float> values_;
    std::vector<float> lowerBounds_;
    std::vector<float> upperBounds_;
    std::vector<std::uint8_t> lowerRaisesInexact_;
    std::vector<std::uint8_t> upperRaisesInexact_;
    std::vector<std::uint8_t> orderedLookupRaisesInexact_;
    std::uint64_t sourceStorageRevision_ = 0u;
    unsigned interpolation_ = 0u;
    bool positionsAreNondecreasing_ = false;
    bool lowerClampRaisesInexact_ = false;
    bool upperClampRaisesInexact_ = false;
};

}  // namespace forevervalidator::simulation
