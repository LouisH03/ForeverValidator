#include "simulation/backends/optimized_cpu/optimized_cpu_static_bvh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <new>
#include <utility>

#include "simulation/backends/optimized_cpu/optimized_cpu_static_bounds_overlap.h"

namespace {

constexpr u32 BinCount = 32u;
constexpr u32 LeafPrimitiveCount = 4u;
constexpr u32 MaximumBuildDepth = 64u;

bool IsUsableBounds(const GmBoxAligned &bounds) noexcept {
    return std::isfinite(bounds.center.x) &&
           std::isfinite(bounds.center.y) &&
           std::isfinite(bounds.center.z) &&
           std::isfinite(bounds.halfExtents.x) &&
           std::isfinite(bounds.halfExtents.y) &&
           std::isfinite(bounds.halfExtents.z) &&
           bounds.halfExtents.x >= 0.0f &&
           bounds.halfExtents.y >= 0.0f &&
           bounds.halfExtents.z >= 0.0f;
}

double CenterAt(const GmBoxAligned &bounds, u32 axis) noexcept {
    switch (axis) {
        case 0u:
            return bounds.center.x;
        case 1u:
            return bounds.center.y;
        default:
            return bounds.center.z;
    }
}

double ExtentAt(const GmBoxAligned &bounds, u32 axis) noexcept {
    switch (axis) {
        case 0u:
            return bounds.halfExtents.x;
        case 1u:
            return bounds.halfExtents.y;
        default:
            return bounds.halfExtents.z;
    }
}

struct DoubleBounds {
    std::array<double, 3u> minimum = {
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
    };
    std::array<double, 3u> maximum = {
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
    };
    bool valid = false;

    void Include(const GmBoxAligned &bounds) noexcept {
        for (u32 axis = 0u; axis < 3u; ++axis) {
            const double center = CenterAt(bounds, axis);
            const double extent = ExtentAt(bounds, axis);
            minimum[axis] = std::min(minimum[axis], center - extent);
            maximum[axis] = std::max(maximum[axis], center + extent);
        }
        valid = true;
    }

    void Include(const DoubleBounds &bounds) noexcept {
        if (!bounds.valid) {
            return;
        }
        for (u32 axis = 0u; axis < 3u; ++axis) {
            minimum[axis] = std::min(minimum[axis], bounds.minimum[axis]);
            maximum[axis] = std::max(maximum[axis], bounds.maximum[axis]);
        }
        valid = true;
    }
};

float RoundExtentOutward(double value) noexcept {
    const float rounded = static_cast<float>(value);
    if (!std::isfinite(rounded) ||
        static_cast<double>(rounded) >= value) {
        return rounded;
    }
    return std::nextafter(
            rounded, std::numeric_limits<float>::infinity());
}

GmBoxAligned ToConservativeBox(const DoubleBounds &bounds) noexcept {
    GmBoxAligned result{};
    const double centerX =
            (bounds.minimum[0u] + bounds.maximum[0u]) * 0.5;
    const double centerY =
            (bounds.minimum[1u] + bounds.maximum[1u]) * 0.5;
    const double centerZ =
            (bounds.minimum[2u] + bounds.maximum[2u]) * 0.5;
    result.center = {
        static_cast<float>(centerX),
        static_cast<float>(centerY),
        static_cast<float>(centerZ),
    };
    result.halfExtents = {
        RoundExtentOutward(std::max(
                std::fabs(static_cast<double>(result.center.x) -
                          bounds.minimum[0u]),
                std::fabs(bounds.maximum[0u] -
                          static_cast<double>(result.center.x)))),
        RoundExtentOutward(std::max(
                std::fabs(static_cast<double>(result.center.y) -
                          bounds.minimum[1u]),
                std::fabs(bounds.maximum[1u] -
                          static_cast<double>(result.center.y)))),
        RoundExtentOutward(std::max(
                std::fabs(static_cast<double>(result.center.z) -
                          bounds.minimum[2u]),
                std::fabs(bounds.maximum[2u] -
                          static_cast<double>(result.center.z)))),
    };
    return result;
}

double SurfaceArea(const DoubleBounds &bounds) noexcept {
    if (!bounds.valid) {
        return 0.0;
    }
    const double x = std::max(0.0, bounds.maximum[0u] - bounds.minimum[0u]);
    const double y = std::max(0.0, bounds.maximum[1u] - bounds.minimum[1u]);
    const double z = std::max(0.0, bounds.maximum[2u] - bounds.minimum[2u]);
    return 2.0 * (x * y + y * z + z * x);
}

u32 BinFor(double center, double minimum, double inverseSpan) noexcept {
    const double scaled = (center - minimum) * inverseSpan * BinCount;
    if (!(scaled > 0.0)) {
        return 0u;
    }
    if (scaled >= static_cast<double>(BinCount)) {
        return BinCount - 1u;
    }
    return static_cast<u32>(scaled);
}

}  // namespace

bool OptimizedCpuStaticBvh::TryBuild(
        const std::vector<Entry> &entries,
        std::size_t sourceCount) noexcept {
    Clear();
    if (entries.empty() || sourceCount == 0u ||
        sourceCount > std::numeric_limits<u32>::max()) {
        return false;
    }

    try {
        OptimizedCpuStaticBvh rebuilt;
        rebuilt.primitives_.reserve(entries.size());
        u32 previousSourceIndex = 0u;
        bool first = true;
        for (const Entry &entry : entries) {
            if (entry.sourceIndex >= sourceCount ||
                !IsUsableBounds(entry.bounds) ||
                (!first && entry.sourceIndex <= previousSourceIndex)) {
                return false;
            }
            first = false;
            previousSourceIndex = entry.sourceIndex;
            rebuilt.primitives_.push_back(
                    {entry.sourceIndex, entry.bounds});
        }

        rebuilt.nodes_.reserve(entries.size() * 2u);
        rebuilt.candidateIndices_.reserve(entries.size());
        rebuilt.traversalStack_.reserve(64u);
        rebuilt.BuildNode(
                0u,
                static_cast<u32>(rebuilt.primitives_.size()),
                0u);
        if (rebuilt.nodes_.empty()) {
            return false;
        }
        *this = std::move(rebuilt);
        return true;
    } catch (const std::bad_alloc &) {
        Clear();
        return false;
    }
}

void OptimizedCpuStaticBvh::Clear(void) noexcept {
    primitives_.clear();
    nodes_.clear();
    candidateIndices_.clear();
    traversalStack_.clear();
}

u32 OptimizedCpuStaticBvh::BuildNode(u32 begin, u32 end, u32 depth) {
    const u32 nodeIndex = static_cast<u32>(nodes_.size());
    nodes_.push_back({});

    DoubleBounds rangeBounds;
    DoubleBounds centroidBounds;
    for (u32 index = begin; index < end; ++index) {
        rangeBounds.Include(primitives_[index].bounds);
        const GmBoxAligned centroid = {
            primitives_[index].bounds.center,
            {0.0f, 0.0f, 0.0f},
        };
        centroidBounds.Include(centroid);
    }
    nodes_[nodeIndex].bounds = ToConservativeBox(rangeBounds);

    const u32 primitiveCount = end - begin;
    if (primitiveCount <= LeafPrimitiveCount) {
        nodes_[nodeIndex].begin = begin;
        nodes_[nodeIndex].count = primitiveCount;
        return nodeIndex;
    }

    double bestCost = static_cast<double>(primitiveCount);
    u32 bestAxis = 3u;
    u32 bestSplitBin = 0u;
    const double parentArea = SurfaceArea(rangeBounds);

    for (u32 axis = 0u; axis < 3u; ++axis) {
        const double minimum = centroidBounds.minimum[axis];
        const double maximum = centroidBounds.maximum[axis];
        const double span = maximum - minimum;
        if (!(span > 0.0) || !std::isfinite(span)) {
            continue;
        }

        struct Bin {
            DoubleBounds bounds;
            u32 count = 0u;
        };
        std::array<Bin, BinCount> bins{};
        const double inverseSpan = 1.0 / span;
        for (u32 index = begin; index < end; ++index) {
            const u32 bin = BinFor(
                    CenterAt(primitives_[index].bounds, axis),
                    minimum,
                    inverseSpan);
            ++bins[bin].count;
            bins[bin].bounds.Include(primitives_[index].bounds);
        }

        std::array<DoubleBounds, BinCount> prefixBounds{};
        std::array<DoubleBounds, BinCount> suffixBounds{};
        std::array<u32, BinCount> prefixCounts{};
        std::array<u32, BinCount> suffixCounts{};

        DoubleBounds runningBounds;
        u32 runningCount = 0u;
        for (u32 bin = 0u; bin < BinCount; ++bin) {
            runningBounds.Include(bins[bin].bounds);
            runningCount += bins[bin].count;
            prefixBounds[bin] = runningBounds;
            prefixCounts[bin] = runningCount;
        }
        runningBounds = {};
        runningCount = 0u;
        for (u32 reverse = BinCount; reverse != 0u; --reverse) {
            const u32 bin = reverse - 1u;
            runningBounds.Include(bins[bin].bounds);
            runningCount += bins[bin].count;
            suffixBounds[bin] = runningBounds;
            suffixCounts[bin] = runningCount;
        }

        for (u32 splitBin = 0u;
             splitBin + 1u < BinCount;
             ++splitBin) {
            const u32 leftCount = prefixCounts[splitBin];
            const u32 rightCount = suffixCounts[splitBin + 1u];
            if (leftCount == 0u || rightCount == 0u) {
                continue;
            }
            const double weightedArea =
                    SurfaceArea(prefixBounds[splitBin]) * leftCount +
                    SurfaceArea(suffixBounds[splitBin + 1u]) * rightCount;
            const double cost = parentArea > 0.0
                    ? 1.0 + weightedArea / parentArea
                    : 1.0 + static_cast<double>(
                            std::max(leftCount, rightCount));
            if (cost < bestCost) {
                bestCost = cost;
                bestAxis = axis;
                bestSplitBin = splitBin;
            }
        }
    }

    u32 middle = begin;
    if (depth < MaximumBuildDepth && bestAxis != 3u) {
        const double minimum = centroidBounds.minimum[bestAxis];
        const double span =
                centroidBounds.maximum[bestAxis] - minimum;
        const double inverseSpan = 1.0 / span;
        auto split = std::stable_partition(
                primitives_.begin() + begin,
                primitives_.begin() + end,
                [&](const Primitive &primitive) {
                    return BinFor(
                                   CenterAt(primitive.bounds, bestAxis),
                                   minimum,
                                   inverseSpan) <= bestSplitBin;
                });
        middle = static_cast<u32>(
                std::distance(primitives_.begin(), split));
    }

    if (middle == begin || middle == end) {
        u32 axis = 0u;
        double largestSpan =
                centroidBounds.maximum[0u] - centroidBounds.minimum[0u];
        for (u32 candidateAxis = 1u;
             candidateAxis < 3u;
             ++candidateAxis) {
            const double candidateSpan =
                    centroidBounds.maximum[candidateAxis] -
                    centroidBounds.minimum[candidateAxis];
            if (candidateSpan > largestSpan) {
                axis = candidateAxis;
                largestSpan = candidateSpan;
            }
        }
        middle = begin + primitiveCount / 2u;
        std::stable_sort(
                primitives_.begin() + begin,
                primitives_.begin() + end,
                [axis](const Primitive &left, const Primitive &right) {
                    const double leftCenter =
                            CenterAt(left.bounds, axis);
                    const double rightCenter =
                            CenterAt(right.bounds, axis);
                    if (leftCenter != rightCenter) {
                        return leftCenter < rightCenter;
                    }
                    return left.sourceIndex < right.sourceIndex;
                });
    }

    const u32 left = BuildNode(begin, middle, depth + 1u);
    const u32 right = BuildNode(middle, end, depth + 1u);
    nodes_[nodeIndex].left = left;
    nodes_[nodeIndex].right = right;
    return nodeIndex;
}

bool OptimizedCpuStaticBvh::CandidateSpanFor(
        const GmBoxAligned &query,
        OptimizedCpuStaticUniformGrid::CandidateSpan *result) const
        noexcept {
    if (result == nullptr || nodes_.empty() || !IsUsableBounds(query)) {
        return false;
    }

    try {
        candidateIndices_.clear();
        traversalStack_.clear();
        traversalStack_.push_back(0u);
        while (!traversalStack_.empty()) {
            const u32 nodeIndex = traversalStack_.back();
            traversalStack_.pop_back();
            const Node &node = nodes_[nodeIndex];
            if (!forevervalidator::simulation::
                        OptimizedCpuStaticBoundsOverlap(
                                query, node.bounds)) {
                continue;
            }
            if (node.count != 0u) {
                for (u32 index = node.begin;
                     index < node.begin + node.count;
                     ++index) {
                    const Primitive &primitive = primitives_[index];
                    if (forevervalidator::simulation::
                                OptimizedCpuStaticBoundsOverlap(
                                        query, primitive.bounds)) {
                        candidateIndices_.push_back(
                                primitive.sourceIndex);
                    }
                }
                continue;
            }
            traversalStack_.push_back(node.right);
            traversalStack_.push_back(node.left);
        }
        std::sort(candidateIndices_.begin(), candidateIndices_.end());
        result->size = candidateIndices_.size();
        result->data = result->size == 0u
                ? nullptr
                : candidateIndices_.data();
        return true;
    } catch (const std::bad_alloc &) {
        candidateIndices_.clear();
        traversalStack_.clear();
        return false;
    }
}

bool OptimizedCpuStaticBvh::OverlapsAny(
        const GmBoxAligned &query) const noexcept {
    if (nodes_.empty()) {
        return false;
    }
    if (!IsUsableBounds(query)) {
        return true;
    }

    try {
        traversalStack_.clear();
        traversalStack_.push_back(0u);
        while (!traversalStack_.empty()) {
            const u32 nodeIndex = traversalStack_.back();
            traversalStack_.pop_back();
            const Node &node = nodes_[nodeIndex];
            if (!forevervalidator::simulation::
                        OptimizedCpuStaticBoundsOverlap(
                                query, node.bounds)) {
                continue;
            }
            if (node.count != 0u) {
                for (u32 index = node.begin;
                     index < node.begin + node.count;
                     ++index) {
                    if (forevervalidator::simulation::
                                OptimizedCpuStaticBoundsOverlap(
                                        query,
                                        primitives_[index].bounds)) {
                        return true;
                    }
                }
                continue;
            }
            traversalStack_.push_back(node.right);
            traversalStack_.push_back(node.left);
        }
        return false;
    } catch (const std::bad_alloc &) {
        traversalStack_.clear();
        return true;
    }
}
