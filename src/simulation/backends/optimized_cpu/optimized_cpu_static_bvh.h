#pragma once

#include <cstddef>
#include <vector>

#include "engine/core/gm_types.h"
#include "simulation/backends/optimized_cpu/optimized_cpu_static_uniform_grid.h"

class OptimizedCpuStaticBvh {
public:
    struct Entry {
        u32 sourceIndex = 0u;
        GmBoxAligned bounds{};
    };

    bool TryBuild(const std::vector<Entry> &entries,
                  std::size_t sourceCount) noexcept;
    void Clear(void) noexcept;

    bool CandidateSpanFor(
            const GmBoxAligned &query,
            OptimizedCpuStaticUniformGrid::CandidateSpan *result) const
            noexcept;
    bool OverlapsAny(const GmBoxAligned &query) const noexcept;

    bool IsAvailable(void) const noexcept {
        return !nodes_.empty();
    }

private:
    struct Primitive {
        u32 sourceIndex = 0u;
        GmBoxAligned bounds{};
    };

    struct Node {
        GmBoxAligned bounds{};
        u32 left = 0u;
        u32 right = 0u;
        u32 begin = 0u;
        u32 count = 0u;
    };

    u32 BuildNode(u32 begin, u32 end, u32 depth);

    std::vector<Primitive> primitives_;
    std::vector<Node> nodes_;
    mutable std::vector<u32> candidateIndices_;
    mutable std::vector<u32> traversalStack_;
};
