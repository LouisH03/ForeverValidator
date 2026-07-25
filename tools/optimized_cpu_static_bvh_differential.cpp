#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>

#include "simulation/backends/optimized_cpu/optimized_cpu_static_bounds_overlap.h"
#include "simulation/backends/optimized_cpu/optimized_cpu_static_bvh.h"

namespace {

struct XorShift32 {
    std::uint32_t state = 0x963a7f21u;

    std::uint32_t Next(void) {
        state ^= state << 13u;
        state ^= state >> 17u;
        state ^= state << 5u;
        return state;
    }
};

float SignedCoordinate(XorShift32 &random, std::uint32_t range) {
    const int centered =
            static_cast<int>(random.Next() % (range * 2u + 1u)) -
            static_cast<int>(range);
    return static_cast<float>(centered) / 64.0f;
}

bool SameCandidates(
        const std::vector<OptimizedCpuStaticBvh::Entry> &entries,
        const GmBoxAligned &query,
        const OptimizedCpuStaticUniformGrid::CandidateSpan &actual) {
    std::vector<u32> expected;
    for (const OptimizedCpuStaticBvh::Entry &entry : entries) {
        if (forevervalidator::simulation::OptimizedCpuStaticBoundsOverlap(
                    query, entry.bounds)) {
            expected.push_back(entry.sourceIndex);
        }
    }
    if (expected.size() != actual.size) {
        return false;
    }
    for (std::size_t index = 0u; index < expected.size(); ++index) {
        if (actual.data[index] != expected[index]) {
            return false;
        }
    }
    return true;
}

bool RunCoverage(void) {
    XorShift32 random;
    std::vector<OptimizedCpuStaticBvh::Entry> entries;
    entries.reserve(513u);
    for (u32 index = 0u; index < 513u; ++index) {
        entries.push_back({
            index * 2u + 1u,
            {
                {
                    SignedCoordinate(random, 4096u),
                    SignedCoordinate(random, 1024u),
                    SignedCoordinate(random, 4096u),
                },
                {
                    static_cast<float>(random.Next() % 257u) / 64.0f,
                    static_cast<float>(random.Next() % 129u) / 64.0f,
                    static_cast<float>(random.Next() % 257u) / 64.0f,
                },
            },
        });
    }

    OptimizedCpuStaticBvh bvh;
    if (!bvh.TryBuild(entries, 1026u) || !bvh.IsAvailable()) {
        return false;
    }

    for (u32 caseIndex = 0u; caseIndex < 8192u; ++caseIndex) {
        const GmBoxAligned query = {
            {
                SignedCoordinate(random, 4608u),
                SignedCoordinate(random, 1280u),
                SignedCoordinate(random, 4608u),
            },
            {
                static_cast<float>(random.Next() % 513u) / 64.0f,
                static_cast<float>(random.Next() % 257u) / 64.0f,
                static_cast<float>(random.Next() % 513u) / 64.0f,
            },
        };
        OptimizedCpuStaticUniformGrid::CandidateSpan candidates;
        if (!bvh.CandidateSpanFor(query, &candidates) ||
            !SameCandidates(entries, query, candidates) ||
            bvh.OverlapsAny(query) != (candidates.size != 0u)) {
            std::fprintf(stderr, "bvh query %u differs\n", caseIndex);
            return false;
        }
    }

    const GmBoxAligned wholeScene = {
        {0.0f, 0.0f, 0.0f},
        {100000.0f, 100000.0f, 100000.0f},
    };
    OptimizedCpuStaticUniformGrid::CandidateSpan allCandidates;
    if (!bvh.CandidateSpanFor(wholeScene, &allCandidates) ||
        !SameCandidates(entries, wholeScene, allCandidates) ||
        allCandidates.size != entries.size()) {
        return false;
    }

    const GmBoxAligned emptyQuery = {
        {1000000.0f, 1000000.0f, 1000000.0f},
        {0.0f, 0.0f, 0.0f},
    };
    OptimizedCpuStaticUniformGrid::CandidateSpan emptyCandidates;
    return bvh.CandidateSpanFor(emptyQuery, &emptyCandidates) &&
           emptyCandidates.data == nullptr &&
           emptyCandidates.size == 0u &&
           !bvh.OverlapsAny(emptyQuery);
}

bool RunValidationCases(void) {
    OptimizedCpuStaticBvh bvh;
    if (bvh.TryBuild({}, 1u) || bvh.IsAvailable()) {
        return false;
    }

    const GmBoxAligned bounds = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
    };
    const std::vector<OptimizedCpuStaticBvh::Entry> duplicate = {
        {1u, bounds},
        {1u, bounds},
    };
    if (bvh.TryBuild(duplicate, 2u)) {
        return false;
    }
    const std::vector<OptimizedCpuStaticBvh::Entry> unordered = {
        {2u, bounds},
        {1u, bounds},
    };
    if (bvh.TryBuild(unordered, 3u)) {
        return false;
    }
    const std::vector<OptimizedCpuStaticBvh::Entry> outOfRange = {
        {3u, bounds},
    };
    if (bvh.TryBuild(outOfRange, 3u)) {
        return false;
    }
    const std::vector<OptimizedCpuStaticBvh::Entry> valid = {
        {0u, bounds},
    };
    OptimizedCpuStaticUniformGrid::CandidateSpan candidates;
    if (!bvh.TryBuild(valid, 1u) ||
        !bvh.CandidateSpanFor(bounds, &candidates) ||
        candidates.size != 1u || candidates.data[0u] != 0u ||
        !bvh.OverlapsAny(bounds)) {
        return false;
    }
    GmBoxAligned invalidQuery = bounds;
    invalidQuery.center.x = std::numeric_limits<float>::quiet_NaN();
    return !bvh.CandidateSpanFor(invalidQuery, &candidates) &&
           bvh.OverlapsAny(invalidQuery);
}

}  // namespace

int main(void) {
    const bool identical = RunCoverage() && RunValidationCases();
    std::printf(
            "static_bvh_query_cases=8194 validation_cases=5 result=%s\n",
            identical ? "identical" : "different");
    return identical ? 0 : 1;
}
