#include <cfenv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <random>

#if defined(__i386__) || defined(__x86_64__)
#include <xmmintrin.h>
#endif

#include "engine/core/gm_types.h"
#include "simulation/backends/optimized_cpu/optimized_cpu_static_bounds_overlap.h"

namespace {

using forevervalidator::simulation::OptimizedCpuStaticBoundsOverlap;

struct TestCase {
    const char *name;
    GmBoxAligned moving;
    GmBoxAligned fixed;
};

float FloatFromBits(std::uint32_t bits) noexcept {
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

GmBoxAligned Box(
        float centerX,
        float centerY,
        float centerZ,
        float extentX,
        float extentY,
        float extentZ) {
    return {{centerX, centerY, centerZ}, {extentX, extentY, extentZ}};
}

void SetInexactStatus(void) {
    std::feclearexcept(FE_ALL_EXCEPT);
    std::feraiseexcept(FE_INEXACT);
#if defined(__i386__) || defined(__x86_64__)
    _mm_setcsr((_mm_getcsr() & ~0x3fu) | _MM_EXCEPT_INEXACT);
#endif
}

bool RunCase(const TestCase &testCase) {
    std::feclearexcept(FE_ALL_EXCEPT);
    const int reference = testCase.moving.TestInter(testCase.fixed);
    const int referenceExceptions = std::fetestexcept(FE_ALL_EXCEPT);

    std::feclearexcept(FE_ALL_EXCEPT);
    const int optimized = OptimizedCpuStaticBoundsOverlap(
            testCase.moving, testCase.fixed);
    const int optimizedExceptions = std::fetestexcept(FE_ALL_EXCEPT);

    if (reference != optimized ||
        referenceExceptions != optimizedExceptions) {
        std::fprintf(
                stderr,
                "static_bounds_overlap_differential: %s ref=%d opt=%d "
                "ref_fenv=%x opt_fenv=%x\n",
                testCase.name,
                reference,
                optimized,
                referenceExceptions,
                optimizedExceptions);
        return false;
    }
    return true;
}

bool RunPacketCases(void) {
    std::mt19937 random(0x6e41a259u);
    std::uniform_real_distribution<float> center(-10000.0f, 10000.0f);
    std::uniform_real_distribution<float> extent(0.0f, 100.0f);
    for (std::size_t caseIndex = 0u; caseIndex < 4096u; ++caseIndex) {
        forevervalidator::simulation::OptimizedCpuStaticBoundsPacket8 packet;
        GmBoxAligned lanes[8];
        for (std::size_t lane = 0u; lane < 8u; ++lane) {
            lanes[lane] = Box(
                    center(random), center(random), center(random),
                    extent(random), extent(random), extent(random));
            packet.SetLane(lane, lanes[lane]);
        }
        const GmBoxAligned fixed = Box(
                center(random), center(random), center(random),
                extent(random), extent(random), extent(random));
        std::uint32_t reference = 0u;
        SetInexactStatus();
        for (std::size_t lane = 0u; lane < 8u; ++lane) {
            if (OptimizedCpuStaticBoundsOverlap(lanes[lane], fixed)) {
                reference |= 1u << static_cast<std::uint32_t>(lane);
            }
        }
        const int referenceExceptions = std::fetestexcept(FE_ALL_EXCEPT);
#if defined(__i386__) || defined(__x86_64__)
        const unsigned int referenceMxcsr = _mm_getcsr() & 0x3fu;
#else
        const unsigned int referenceMxcsr = 0u;
#endif

        SetInexactStatus();
        const std::uint32_t optimized =
                forevervalidator::simulation::
                        OptimizedCpuStaticBoundsOverlapPacket8(packet, fixed);
        const int optimizedExceptions = std::fetestexcept(FE_ALL_EXCEPT);
#if defined(__i386__) || defined(__x86_64__)
        const unsigned int optimizedMxcsr = _mm_getcsr() & 0x3fu;
#else
        const unsigned int optimizedMxcsr = 0u;
#endif
        if (reference != optimized ||
            referenceExceptions != optimizedExceptions ||
            referenceMxcsr != optimizedMxcsr) {
            std::fprintf(
                    stderr,
                    "static_bounds_packet_differential: case=%zu "
                    "ref=%x opt=%x ref_fenv=%x opt_fenv=%x "
                    "ref_mxcsr=%x opt_mxcsr=%x\n",
                    caseIndex,
                    reference,
                    optimized,
                    referenceExceptions,
                    optimizedExceptions,
                    referenceMxcsr,
                    optimizedMxcsr);
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    const float positiveInfinity = std::numeric_limits<float>::infinity();
    const float quietNaN = std::numeric_limits<float>::quiet_NaN();
    const float signalingNaN = FloatFromBits(0x7fa00001u);
    const float negativeZero = FloatFromBits(0x80000000u);

    const TestCase cases[] = {
        {"ordinary-overlap",
         Box(1.0f, -2.0f, 3.0f, 2.0f, 3.0f, 4.0f),
         Box(2.5f, 0.0f, 6.0f, 1.0f, 1.0f, 1.0f)},
        {"z-disjoint",
         Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f),
         Box(0.0f, 0.0f, 3.0f, 1.0f, 1.0f, 1.0f)},
        {"y-disjoint",
         Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f),
         Box(0.0f, 3.0f, 0.0f, 1.0f, 1.0f, 1.0f)},
        {"x-disjoint",
         Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f),
         Box(3.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f)},
        {"touching-boundary",
         Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f),
         Box(2.0f, 2.0f, 2.0f, 1.0f, 1.0f, 1.0f)},
        {"signed-zero",
         Box(negativeZero, 0.0f, negativeZero,
             negativeZero, negativeZero, 0.0f),
         Box(0.0f, negativeZero, 0.0f,
             0.0f, 0.0f, negativeZero)},
        {"infinite-centers",
         Box(positiveInfinity, positiveInfinity, positiveInfinity,
             1.0f, 1.0f, 1.0f),
         Box(positiveInfinity, positiveInfinity, positiveInfinity,
             1.0f, 1.0f, 1.0f)},
        {"infinite-extents",
         Box(-positiveInfinity, 0.0f, positiveInfinity,
             positiveInfinity, positiveInfinity, positiveInfinity),
         Box(positiveInfinity, 1.0f, -positiveInfinity,
             positiveInfinity, positiveInfinity, positiveInfinity)},
        {"quiet-nan-centers",
         Box(quietNaN, quietNaN, quietNaN, 1.0f, 1.0f, 1.0f),
         Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f)},
        {"quiet-nan-extents",
         Box(0.0f, 0.0f, 0.0f, quietNaN, quietNaN, quietNaN),
         Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f)},
        {"signaling-nan",
         Box(signalingNaN, signalingNaN, signalingNaN,
             1.0f, 1.0f, 1.0f),
         Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f)},
        {"z-short-circuits-signaling-nan-yx",
         Box(signalingNaN, signalingNaN, 0.0f, 1.0f, 1.0f, 1.0f),
         Box(0.0f, 0.0f, 3.0f, 1.0f, 1.0f, 1.0f)},
        {"y-short-circuits-signaling-nan-x",
         Box(signalingNaN, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f),
         Box(0.0f, 3.0f, 0.0f, 1.0f, 1.0f, 1.0f)},
    };

    std::size_t completed = 0u;
    for (const TestCase &testCase : cases) {
        if (!RunCase(testCase)) {
            return 1;
        }
        ++completed;
    }
    if (!RunPacketCases()) {
        return 1;
    }

    std::printf(
            "static_bounds_overlap_cases=%zu bit_identical=true "
            "packet_cases=4096 fenv_identical=true\n",
            completed);
    return 0;
}
