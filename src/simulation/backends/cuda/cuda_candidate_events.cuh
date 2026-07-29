#ifndef FOREVERVALIDATOR_CUDA_CANDIDATE_EVENTS_CUH
#define FOREVERVALIDATOR_CUDA_CANDIDATE_EVENTS_CUH

#include <cstddef>
#include <cstdint>

#include "simulation/backends/cuda/cuda_search_executor.h"

#if defined(__CUDACC__)
#define FOREVERVALIDATOR_CANDIDATE_HD __host__ __device__
#else
#define FOREVERVALIDATOR_CANDIDATE_HD
#endif

namespace forevervalidator::simulation::cuda::candidate_events {

using Event = CudaSearchInputEvent;

struct CanonicalSuffix {
    const Event *events = nullptr;
    std::uint32_t count = 0u;
    std::int64_t mutableFromTimeMs = 0;
};

FOREVERVALIDATOR_CANDIDATE_HD inline CanonicalSuffix SuffixFrom(
        const Event *baseline,
        std::uint32_t count,
        std::int64_t mutableFromTimeMs) {
    std::uint32_t first = 0u;
    std::uint32_t remaining = count;
    while (remaining != 0u) {
        const std::uint32_t step = remaining / 2u;
        const std::uint32_t middle = first + step;
        if (baseline[middle].timeMs < mutableFromTimeMs) {
            first = middle + 1u;
            remaining -= step + 1u;
        } else {
            remaining = step;
        }
    }
    return {
            baseline + first,
            count - first,
            mutableFromTimeMs};
}

enum class EditKind : std::uint32_t {
    ChangeValue,
    ChangeTime,
    Replace,
    Insert,
    Erase,
};

struct Edit {
    EditKind kind = EditKind::ChangeValue;
    std::uint32_t index = 0u;
    Event event{};
};

// Edit ordinal is the major dimension. Adjacent candidate lanes therefore
// read adjacent words while executing the same modifier pass.
struct CoalescedEditStorage {
    std::uint32_t *counts = nullptr;
    std::uint32_t *kinds = nullptr;
    std::uint32_t *indices = nullptr;
    std::int32_t *times = nullptr;
    std::uint32_t *actions = nullptr;
    std::uint32_t *valueKinds = nullptr;
    std::int32_t *values = nullptr;
    std::uint32_t candidateStride = 0u;
    std::uint32_t editCapacity = 0u;
};

FOREVERVALIDATOR_CANDIDATE_HD inline std::uint64_t EditOffset(
        const CoalescedEditStorage &storage,
        std::uint32_t candidate,
        std::uint32_t ordinal) {
    return static_cast<std::uint64_t>(ordinal) *
                    storage.candidateStride +
            candidate;
}

FOREVERVALIDATOR_CANDIDATE_HD inline Edit LoadEdit(
        const CoalescedEditStorage &storage,
        std::uint32_t candidate,
        std::uint32_t ordinal) {
    const std::uint64_t offset = EditOffset(storage, candidate, ordinal);
    return {
            static_cast<EditKind>(storage.kinds[offset]),
            storage.indices[offset],
            {storage.times[offset],
             storage.actions[offset],
             storage.valueKinds[offset],
             storage.values[offset]}};
}

class EditWriter {
public:
    FOREVERVALIDATOR_CANDIDATE_HD EditWriter(
            CoalescedEditStorage storage,
            std::uint32_t candidate)
        : storage_(storage), candidate_(candidate) {}

    FOREVERVALIDATOR_CANDIDATE_HD bool Append(const Edit &edit) {
        if (storage_.counts == nullptr ||
            storage_.counts[candidate_] >= storage_.editCapacity) {
            return false;
        }
        const std::uint32_t ordinal = storage_.counts[candidate_]++;
        const std::uint64_t offset =
                EditOffset(storage_, candidate_, ordinal);
        storage_.kinds[offset] = static_cast<std::uint32_t>(edit.kind);
        storage_.indices[offset] = edit.index;
        storage_.times[offset] = edit.event.timeMs;
        storage_.actions[offset] = edit.event.action;
        storage_.valueKinds[offset] = edit.event.valueKind;
        storage_.values[offset] = edit.event.value;
        return true;
    }

    FOREVERVALIDATOR_CANDIDATE_HD bool ChangeValue(
            std::uint32_t index,
            std::int32_t value) {
        Edit edit;
        edit.kind = EditKind::ChangeValue;
        edit.index = index;
        edit.event.value = value;
        return Append(edit);
    }

    FOREVERVALIDATOR_CANDIDATE_HD bool ChangeTime(
            std::uint32_t index,
            std::int32_t timeMs) {
        Edit edit;
        edit.kind = EditKind::ChangeTime;
        edit.index = index;
        edit.event.timeMs = timeMs;
        return Append(edit);
    }

    FOREVERVALIDATOR_CANDIDATE_HD bool Replace(
            std::uint32_t index,
            const Event &event) {
        return Append({EditKind::Replace, index, event});
    }

    FOREVERVALIDATOR_CANDIDATE_HD bool Insert(
            std::uint32_t index,
            const Event &event) {
        return Append({EditKind::Insert, index, event});
    }

    FOREVERVALIDATOR_CANDIDATE_HD bool Erase(std::uint32_t index) {
        return Append({EditKind::Erase, index, {}});
    }

private:
    CoalescedEditStorage storage_{};
    std::uint32_t candidate_ = 0u;
};

struct CandidateView {
    CanonicalSuffix baseline{};
    CoalescedEditStorage edits{};
    std::uint32_t candidate = 0u;

    FOREVERVALIDATOR_CANDIDATE_HD std::uint32_t EditCount() const {
        return edits.counts == nullptr ? 0u : edits.counts[candidate];
    }

    FOREVERVALIDATOR_CANDIDATE_HD bool RequiresMaterialization() const {
        for (std::uint32_t ordinal = 0u;
             ordinal < EditCount(); ++ordinal) {
            if (LoadEdit(edits, candidate, ordinal).kind !=
                EditKind::ChangeValue) {
                return true;
            }
        }
        return false;
    }

    // Value-only modifier pipelines can be consumed without a candidate-sized
    // event array. Later writes win, matching ordered modifier passes.
    FOREVERVALIDATOR_CANDIDATE_HD Event OverlayAt(
            std::uint32_t index) const {
        Event result = baseline.events[index];
        for (std::uint32_t ordinal = 0u;
             ordinal < EditCount(); ++ordinal) {
            const Edit edit = LoadEdit(edits, candidate, ordinal);
            if (edit.kind == EditKind::ChangeValue &&
                edit.index == index) {
                result.value = edit.event.value;
            }
        }
        return result;
    }
};

FOREVERVALIDATOR_CANDIDATE_HD inline std::int32_t SaturateValue(
        const Event &event) {
    if (event.valueKind == 2u) {
        return event.value < -65536
                ? -65536
                : event.value > 65536 ? 65536 : event.value;
    }
    if (event.valueKind == 1u) {
        return event.value == 0 ? 0 : 1;
    }
    return event.value;
}

FOREVERVALIDATOR_CANDIDATE_HD inline std::uint32_t HashAction(
        std::uint32_t action) {
    action ^= action >> 16u;
    action *= 0x7feb352du;
    action ^= action >> 15u;
    action *= 0x846ca68bu;
    return action ^ (action >> 16u);
}

// Stable mergesort by time followed by an expected-linear hash deduplication
// per equal-time group. The first occurrence fixes stable order and the last
// occurrence supplies the value.
FOREVERVALIDATOR_CANDIDATE_HD inline std::uint32_t NormalizeSuffix(
        Event *events,
        std::uint32_t count,
        Event *scratch) {
    for (std::uint32_t index = 0u; index < count; ++index) {
        if (events[index].timeMs < 0) {
            events[index].timeMs = 0;
        }
        events[index].value = SaturateValue(events[index]);
    }

    Event *source = events;
    Event *destination = scratch;
    for (std::uint32_t width = 1u; width < count;) {
        for (std::uint32_t begin = 0u; begin < count;
             begin += width * 2u) {
            const std::uint32_t middle =
                    begin + width < count ? begin + width : count;
            const std::uint32_t end =
                    middle + width < count ? middle + width : count;
            std::uint32_t left = begin;
            std::uint32_t right = middle;
            std::uint32_t output = begin;
            while (left < middle || right < end) {
                if (right >= end ||
                    (left < middle &&
                     source[left].timeMs <= source[right].timeMs)) {
                    destination[output++] = source[left++];
                } else {
                    destination[output++] = source[right++];
                }
            }
        }
        Event *swap = source;
        source = destination;
        destination = swap;
        if (width > count / 2u) {
            break;
        }
        width *= 2u;
    }

    std::uint32_t outputCount = 0u;
    for (std::uint32_t groupBegin = 0u; groupBegin < count;) {
        std::uint32_t groupEnd = groupBegin + 1u;
        while (groupEnd < count &&
               source[groupEnd].timeMs == source[groupBegin].timeMs) {
            ++groupEnd;
        }
        const std::uint32_t groupCount = groupEnd - groupBegin;
        for (std::uint32_t index = 0u; index < groupCount; ++index) {
            destination[outputCount + index] =
                    source[groupBegin + index];
            source[groupBegin + index].timeMs = 0;
        }

        for (std::uint32_t index = 0u; index < groupCount; ++index) {
            const Event value = destination[outputCount + index];
            std::uint32_t bucket =
                    HashAction(value.action) % groupCount;
            while (source[groupBegin + bucket].timeMs != 0 &&
                   source[groupBegin + bucket].action != value.action) {
                bucket = bucket + 1u == groupCount
                        ? 0u : bucket + 1u;
            }
            Event &entry = source[groupBegin + bucket];
            if (entry.timeMs == 0) {
                entry.timeMs = 1;
                entry.action = value.action;
                entry.valueKind = index;
            } else {
                destination[
                        outputCount + entry.valueKind] = value;
            }
        }

        std::uint32_t uniqueCount = 0u;
        for (std::uint32_t index = 0u; index < groupCount; ++index) {
            const Event value = destination[outputCount + index];
            std::uint32_t bucket =
                    HashAction(value.action) % groupCount;
            while (source[groupBegin + bucket].action != value.action) {
                bucket = bucket + 1u == groupCount
                        ? 0u : bucket + 1u;
            }
            if (source[groupBegin + bucket].valueKind == index) {
                destination[outputCount + uniqueCount++] = value;
            }
        }
        outputCount += uniqueCount;
        groupBegin = groupEnd;
    }

    if (destination != events) {
        for (std::uint32_t index = 0u; index < outputCount; ++index) {
            events[index] = destination[index];
        }
    }
    return outputCount;
}

FOREVERVALIDATOR_CANDIDATE_HD inline std::uint32_t NormalizeWithPrefix(
        Event *events,
        std::uint32_t count,
        Event *scratch,
        const Event *baseline,
        std::uint32_t baselineCount,
        std::int64_t mutableFromTimeMs,
        std::uint32_t capacity) {
    std::uint32_t suffixCount = 0u;
    for (std::uint32_t index = 0u; index < count; ++index) {
        if (events[index].timeMs >= mutableFromTimeMs) {
            events[suffixCount++] = events[index];
        }
    }
    suffixCount = NormalizeSuffix(events, suffixCount, scratch);

    std::uint32_t prefixCount = 0u;
    for (std::uint32_t index = 0u;
         index < baselineCount; ++index) {
        if (baseline[index].timeMs < mutableFromTimeMs) {
            ++prefixCount;
        }
    }
    if (prefixCount + suffixCount > capacity) {
        return UINT32_MAX;
    }
    for (std::uint32_t index = suffixCount; index != 0u; --index) {
        events[prefixCount + index - 1u] = events[index - 1u];
    }
    std::uint32_t prefixOutput = 0u;
    for (std::uint32_t index = 0u;
         index < baselineCount; ++index) {
        if (baseline[index].timeMs < mutableFromTimeMs) {
            events[prefixOutput++] = baseline[index];
        }
    }
    return prefixCount + suffixCount;
}

FOREVERVALIDATOR_CANDIDATE_HD inline bool ApplyOrderedEdits(
        const CandidateView &candidate,
        Event *events,
        std::uint32_t *count,
        std::uint32_t capacity) {
    if (candidate.baseline.count > capacity) {
        return false;
    }
    *count = candidate.baseline.count;
    for (std::uint32_t index = 0u; index < *count; ++index) {
        events[index] = candidate.baseline.events[index];
    }
    for (std::uint32_t ordinal = 0u;
         ordinal < candidate.EditCount(); ++ordinal) {
        const Edit edit =
                LoadEdit(candidate.edits, candidate.candidate, ordinal);
        switch (edit.kind) {
        case EditKind::ChangeValue:
            if (edit.index >= *count) {
                return false;
            }
            events[edit.index].value = edit.event.value;
            break;
        case EditKind::ChangeTime:
            if (edit.index >= *count) {
                return false;
            }
            events[edit.index].timeMs = edit.event.timeMs;
            break;
        case EditKind::Replace:
            if (edit.index >= *count) {
                return false;
            }
            events[edit.index] = edit.event;
            break;
        case EditKind::Insert:
            if (edit.index > *count || *count >= capacity) {
                return false;
            }
            for (std::uint32_t index = *count;
                 index > edit.index; --index) {
                events[index] = events[index - 1u];
            }
            events[edit.index] = edit.event;
            ++*count;
            break;
        case EditKind::Erase:
            if (edit.index >= *count) {
                return false;
            }
            for (std::uint32_t index = edit.index + 1u;
                 index < *count; ++index) {
                events[index - 1u] = events[index];
            }
            --*count;
            break;
        }
    }
    return true;
}

FOREVERVALIDATOR_CANDIDATE_HD inline bool MaterializeNormalized(
        const CandidateView &candidate,
        Event *events,
        Event *scratch,
        std::uint32_t *count,
        std::uint32_t capacity) {
    if (!ApplyOrderedEdits(candidate, events, count, capacity)) {
        return false;
    }
    *count = NormalizeSuffix(events, *count, scratch);
    return true;
}

}  // namespace forevervalidator::simulation::cuda::candidate_events

#undef FOREVERVALIDATOR_CANDIDATE_HD

#endif
