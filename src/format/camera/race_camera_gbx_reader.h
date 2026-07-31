#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "engine/camera/race_camera_config.h"

namespace forevervalidator::camera::detail {

bool DecodeRaceCameraProfileGbx(
        const std::uint8_t *bytes,
        std::size_t byteCount,
        EnvironmentConfig *environment,
        std::string *error);

} // namespace forevervalidator::camera::detail
