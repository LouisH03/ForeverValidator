#ifndef FOREVERVALIDATOR_INPUT_STATE_H
#define FOREVERVALIDATOR_INPUT_STATE_H

#include <cstdint>

namespace forevervalidator {

using AnalogInputState = std::int32_t;

inline constexpr AnalogInputState kAnalogInputMinimum = -65536;
inline constexpr AnalogInputState kAnalogInputMaximum = 65536;
inline constexpr AnalogInputState kAnalogInputScale = 65536;

constexpr bool IsAnalogInputStateValid(AnalogInputState value) noexcept {
    return value >= kAnalogInputMinimum && value <= kAnalogInputMaximum;
}

}  // namespace forevervalidator

#endif
