#ifndef FOREVERVALIDATOR_FINISH_TIME_H
#define FOREVERVALIDATOR_FINISH_TIME_H

#include <cstdint>

namespace forevervalidator {

// The finish transition is known to occur in (lowerBoundNs, upperBoundNs].
// estimatedNs is the deterministic upper-bound estimate.
struct FinishTimeEstimate {
    std::uint64_t lowerBoundNs = 0u;
    std::uint64_t upperBoundNs = 0u;
    std::uint64_t estimatedNs = 0u;

    bool IsValid() const noexcept {
        return lowerBoundNs < upperBoundNs &&
               upperBoundNs - lowerBoundNs <= 1u &&
               estimatedNs == upperBoundNs;
    }
};

inline bool operator==(const FinishTimeEstimate &left,
                       const FinishTimeEstimate &right) noexcept {
    return left.lowerBoundNs == right.lowerBoundNs &&
           left.upperBoundNs == right.upperBoundNs &&
           left.estimatedNs == right.estimatedNs;
}

inline bool operator!=(const FinishTimeEstimate &left,
                       const FinishTimeEstimate &right) noexcept {
    return !(left == right);
}

}  // namespace forevervalidator

#endif
