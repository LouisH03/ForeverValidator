#include "engine/core/func_keys_real.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "engine/core/binary32_math.h"

namespace {

constexpr float KeyEpsilon = 1.0e-5f;

// United evaluates the promoted epsilon with 24-bit x87 precision.
float LowerKeyBound(float key) {
    return Binary32::FromDouble(
            static_cast<double>(key) - static_cast<double>(KeyEpsilon));
}

float UpperKeyBound(float key) {
    return Binary32::FromDouble(
            static_cast<double>(key) + static_cast<double>(KeyEpsilon));
}

bool IsWithinKeyBounds(float x, float lower, float upper) {
    const float lowerBound = LowerKeyBound(lower);
    const float upperBound = UpperKeyBound(upper);
    return !std::isnan(x) && !std::isnan(lowerBound) &&
           !std::isnan(upperBound) && x >= lowerBound && x <= upperBound;
}

} // namespace

CFunc::CFunc() = default;

CFunc::~CFunc() = default;

CFuncKeys::CFuncKeys() = default;

CFuncKeys::~CFuncKeys() = default;

void CFuncKeys::GetPreviousKey(float x, unsigned long &keyIndex) const {
    const unsigned long count = KeyCount();
    if (count == 0u) {
        keyIndex = InvalidEngineIndex;
        return;
    }
    if (count == 1u || x < LowerKeyBound(keyPositions.front())) {
        keyIndex = 0u;
        return;
    }
    if (x > UpperKeyBound(keyPositions.back())) {
        keyIndex = count - 1u;
        return;
    }

    unsigned long current = keyIndex < count ? keyIndex : 0ul;
    for (unsigned long scanned = 0ul; scanned <= count; ++scanned) {
        const unsigned long next =
                current + 1ul < count ? current + 1ul : 0ul;
        if (IsWithinKeyBounds(x, XAt(current), XAt(next))) {
            keyIndex = current;
            return;
        }
        current = next;
    }
    keyIndex = current;
}

void CFuncKeys::GetBoundingIndices(
        float x,
        unsigned long &keyIndex,
        unsigned long &nextKeyIndex,
        int searchForward) const {
    const unsigned long count = KeyCount();
    if (count == 0u) {
        keyIndex = InvalidEngineIndex;
        nextKeyIndex = InvalidEngineIndex;
        return;
    }
    if (count == 1u || x < LowerKeyBound(keyPositions.front())) {
        keyIndex = 0u;
        nextKeyIndex = 0u;
        return;
    }
    if (x > UpperKeyBound(keyPositions.back())) {
        keyIndex = count - 1u;
        nextKeyIndex = count - 1u;
        return;
    }

    if (searchForward != 0) {
        unsigned long current = keyIndex < count ? keyIndex : 0ul;
        for (unsigned long scanned = 0ul; scanned <= count; ++scanned) {
            const unsigned long next =
                    current + 1ul < count ? current + 1ul : 0ul;
            if (IsWithinKeyBounds(x, XAt(current), XAt(next))) {
                keyIndex = current;
                nextKeyIndex = next;
                return;
            }
            current = next;
        }
        keyIndex = current;
        nextKeyIndex = current + 1u < count ? current + 1u : 0u;
        return;
    }

    unsigned long next = keyIndex + 1ul < count ? keyIndex + 1ul : 0ul;
    for (unsigned long scanned = 0ul; scanned <= count; ++scanned) {
        const unsigned long current =
                next == 0ul ? count - 1ul : next - 1ul;
        if (IsWithinKeyBounds(x, XAt(current), XAt(next))) {
            keyIndex = current;
            nextKeyIndex = next;
            return;
        }
        next = current;
    }
    keyIndex = next == 0u ? count - 1u : next - 1u;
    nextKeyIndex = next;
}

int CFuncKeys::ComputeBlendCoef(
        float x,
        unsigned long &keyIndex,
        unsigned long &nextKeyIndex,
        float &blendCoef,
        int searchForward) const {
    GetBoundingIndices(x, keyIndex, nextKeyIndex, searchForward);
    if (keyIndex == InvalidEngineIndex) {
        return 0;
    }
    if (keyIndex == nextKeyIndex) {
        blendCoef = 0.0f;
        return 1;
    }

    const float x0 = XAt(keyIndex);
    const float span = XAt(nextKeyIndex) - x0;
    blendCoef = std::fabs(span) >= KeyEpsilon
            ? (x - x0) / span
            : 0.0f;
    return 1;
}

void CFuncKeys::RemoveKey(unsigned long index) {
    if (index < keyPositions.size()) {
        keyPositions.erase(keyPositions.begin() + index);
        MarkStorageChanged();
    }
}

void CFuncKeys::Reset() {
    if (!keyPositions.empty()) {
        keyPositions.clear();
        MarkStorageChanged();
    }
}

void CFuncKeys::AddKey(float x) {
    keyPositions.push_back(x);
    MarkStorageChanged();
}

unsigned long CFuncKeys::InsertKeyX(float x) {
    auto next = std::lower_bound(keyPositions.begin(), keyPositions.end(), x);
    unsigned long index = static_cast<unsigned long>(
            next - keyPositions.begin());

    if (next != keyPositions.begin()) {
        const unsigned long previousIndex = index - 1ul;
        if (x - keyPositions[previousIndex] < KeyEpsilon) {
            RemoveKey(previousIndex);
            index = previousIndex;
        }
    }
    if (index < keyPositions.size() &&
        keyPositions[index] - x < KeyEpsilon) {
        RemoveKey(index);
    }

    keyPositions.insert(keyPositions.begin() + index, x);
    MarkStorageChanged();
    return index;
}

const CMwId *CFuncKeys::MwGetId() const {
    return &id;
}

unsigned long CFuncKeys::KeyCount() const {
    return static_cast<unsigned long>(keyPositions.size());
}

float CFuncKeys::XAt(unsigned long index) const {
    return keyPositions[index];
}

std::uint64_t CFuncKeys::StorageRevision() const noexcept {
    return storageRevision_;
}

void CFuncKeys::MarkStorageChanged() noexcept {
    ++storageRevision_;
}

CFuncKeysReal::CFuncKeysReal() = default;

CFuncKeysReal::~CFuncKeysReal() = default;

void CFuncKeysReal::GetRealAt(
        float x,
        float &out,
        unsigned long &keyIndex,
        unsigned long &nextKeyIndex,
        float &blendCoef,
        ERealInterp interpolation,
        int searchForward) const {
    if (!ComputeBlendCoef(
                x,
                keyIndex,
                nextKeyIndex,
                blendCoef,
                searchForward)) {
        out = 0.0f;
        return;
    }

    if (interpolation == Constant) {
        out = ValueAt(keyIndex);
        return;
    }

    const float value0 = ValueAt(keyIndex);
    const float value1 = ValueAt(nextKeyIndex);
    out = (1.0f - blendCoef) * value0 + blendCoef * value1;
}

void CFuncKeysReal::GetValue(
        float x,
        float &out,
        unsigned long &keyIndex) const {
    unsigned long nextKeyIndex = keyIndex + 1ul;
    float blendCoef = 0.0f;
    GetRealAt(
            x,
            out,
            keyIndex,
            nextKeyIndex,
            blendCoef,
            interpolationMode,
            1);
}

float CFuncKeysReal::GetValue(float x, unsigned long *keyIndex) const {
    unsigned long localKeyIndex = keyIndex != nullptr ? *keyIndex : 0ul;
    unsigned long nextKeyIndex = localKeyIndex + 1ul;
    float blendCoef = 0.0f;
    float value = x;
    GetRealAt(
            x,
            value,
            localKeyIndex,
            nextKeyIndex,
            blendCoef,
            interpolationMode,
            1);
    if (keyIndex != nullptr) {
        *keyIndex = localKeyIndex;
    }
    return value;
}

void CFuncKeysReal::RemoveKey(unsigned long index) {
    CFuncKeys::RemoveKey(index);
    if (index < values.size()) {
        values.erase(values.begin() + index);
    }
}

void CFuncKeysReal::AddKeyReal(float x, float value) {
    AddKey(x);
    values.back() = value;
}

unsigned long CFuncKeysReal::InsertKeyReal(float x, float value) {
    const unsigned long index = InsertKeyX(x);
    values.insert(values.begin() + index, value);
    return index;
}

void CFuncKeysReal::Reset() {
    const bool valuesOnly = keyPositions.empty() && !values.empty();
    CFuncKeys::Reset();
    values.clear();
    if (valuesOnly) {
        MarkStorageChanged();
    }
}

void CFuncKeysReal::AddKey(float x) {
    CFuncKeys::AddKey(x);
    values.push_back(0.0f);
}

float CFuncKeysReal::ValueAt(unsigned long index) const {
    return values[index];
}

CFuncKeysReal::ERealInterp CFuncKeysReal::Interpolation() const {
    return interpolationMode;
}

void CFuncKeysReal::SetInterpolation(ERealInterp interpolation) {
    interpolationMode = interpolation;
}

void CFuncKeysReal::SetKeys(
        std::vector<Key> keys,
        ERealInterp interpolation) {
    Reset();
    keyPositions.reserve(keys.size());
    values.reserve(keys.size());
    for (const Key &key : keys) {
        keyPositions.push_back(key.x);
        values.push_back(key.value);
    }
    if (!keys.empty()) {
        MarkStorageChanged();
    }
    interpolationMode = interpolation;
}
