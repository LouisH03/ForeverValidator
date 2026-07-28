#ifndef FOREVERVALIDATOR_OPTIMIZED_CPU_STATIC_BOUNDS_OVERLAP_H
#define FOREVERVALIDATOR_OPTIMIZED_CPU_STATIC_BOUNDS_OVERLAP_H

#include <cstddef>
#include <cstdint>
#include <cmath>

#include "engine/core/gm_types.h"

#if defined(__SSE2__)
#include <emmintrin.h>
#endif

#if defined(_MSC_VER)
#define FV_E026_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define FV_E026_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define FV_E026_ALWAYS_INLINE inline
#endif

namespace forevervalidator::simulation {

struct alignas(16) OptimizedCpuStaticBoundsPacket8 {
    float centerX[8];
    float centerY[8];
    float centerZ[8];
    float extentX[8];
    float extentY[8];
    float extentZ[8];

    FV_E026_ALWAYS_INLINE void SetLane(
            std::size_t lane,
            const GmBoxAligned &bounds) noexcept {
        centerX[lane] = bounds.center.x;
        centerY[lane] = bounds.center.y;
        centerZ[lane] = bounds.center.z;
        extentX[lane] = bounds.halfExtents.x;
        extentY[lane] = bounds.halfExtents.y;
        extentZ[lane] = bounds.halfExtents.z;
    }
};

namespace optimized_cpu_static_bounds_detail {

FV_E026_ALWAYS_INLINE bool AxisIntervalsOverlap(
        float centerA,
        float extentA,
        float centerB,
        float extentB) noexcept {
    const float distance = std::fabs(centerB - centerA);
    const float extentSum = extentB + extentA;
    return !(extentSum < distance);
}

#if defined(__SSE2__)
FV_E026_ALWAYS_INLINE __m128 AxisIntervalsOverlap4(
        const float *centers,
        const float *extents,
        float staticCenter,
        float staticExtent) noexcept {
    const __m128 distance = _mm_and_ps(
            _mm_sub_ps(
                    _mm_set1_ps(staticCenter),
                    _mm_load_ps(centers)),
            _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff)));
    const __m128 extentSum = _mm_add_ps(
            _mm_set1_ps(staticExtent),
            _mm_load_ps(extents));
    // Match !(extentSum < distance). The bounded runtime certificate excludes
    // unordered inputs; the unordered-true predicate retains defensive parity.
    return _mm_cmpnlt_ps(extentSum, distance);
}
#endif

}  // namespace optimized_cpu_static_bounds_detail

FV_E026_ALWAYS_INLINE int OptimizedCpuStaticBoundsOverlap(
        const GmBoxAligned &movingBox,
        const GmBoxAligned &staticBounds) noexcept {
    using optimized_cpu_static_bounds_detail::AxisIntervalsOverlap;
    if (!AxisIntervalsOverlap(
                movingBox.center.z,
                movingBox.halfExtents.z,
                staticBounds.center.z,
                staticBounds.halfExtents.z)) {
        return 0;
    }
    if (!AxisIntervalsOverlap(
                movingBox.center.y,
                movingBox.halfExtents.y,
                staticBounds.center.y,
                staticBounds.halfExtents.y)) {
        return 0;
    }
    return AxisIntervalsOverlap(
                   movingBox.center.x,
                   movingBox.halfExtents.x,
                   staticBounds.center.x,
                   staticBounds.halfExtents.x)
            ? 1
            : 0;
}

FV_E026_ALWAYS_INLINE std::uint32_t
OptimizedCpuStaticBoundsOverlapPacket8(
        const OptimizedCpuStaticBoundsPacket8 &moving,
        const GmBoxAligned &staticBounds) noexcept {
#if defined(__SSE2__)
    using optimized_cpu_static_bounds_detail::AxisIntervalsOverlap4;
    __m128 low = AxisIntervalsOverlap4(
            moving.centerZ,
            moving.extentZ,
            staticBounds.center.z,
            staticBounds.halfExtents.z);
    __m128 high = AxisIntervalsOverlap4(
            moving.centerZ + 4u,
            moving.extentZ + 4u,
            staticBounds.center.z,
            staticBounds.halfExtents.z);
    low = _mm_and_ps(
            low,
            AxisIntervalsOverlap4(
                    moving.centerY,
                    moving.extentY,
                    staticBounds.center.y,
                    staticBounds.halfExtents.y));
    high = _mm_and_ps(
            high,
            AxisIntervalsOverlap4(
                    moving.centerY + 4u,
                    moving.extentY + 4u,
                    staticBounds.center.y,
                    staticBounds.halfExtents.y));
    low = _mm_and_ps(
            low,
            AxisIntervalsOverlap4(
                    moving.centerX,
                    moving.extentX,
                    staticBounds.center.x,
                    staticBounds.halfExtents.x));
    high = _mm_and_ps(
            high,
            AxisIntervalsOverlap4(
                    moving.centerX + 4u,
                    moving.extentX + 4u,
                    staticBounds.center.x,
                    staticBounds.halfExtents.x));
    return static_cast<std::uint32_t>(_mm_movemask_ps(low)) |
           (static_cast<std::uint32_t>(_mm_movemask_ps(high)) << 4u);
#else
    std::uint32_t result = 0u;
    for (std::size_t lane = 0u; lane < 8u; ++lane) {
        const GmBoxAligned box = {
            {moving.centerX[lane], moving.centerY[lane], moving.centerZ[lane]},
            {moving.extentX[lane], moving.extentY[lane], moving.extentZ[lane]},
        };
        if (OptimizedCpuStaticBoundsOverlap(box, staticBounds)) {
            result |= 1u << static_cast<std::uint32_t>(lane);
        }
    }
    return result;
#endif
}

}  // namespace forevervalidator::simulation

#undef FV_E026_ALWAYS_INLINE

#endif
