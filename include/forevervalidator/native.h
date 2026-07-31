#ifndef FOREVERVALIDATOR_NATIVE_H
#define FOREVERVALIDATOR_NATIVE_H

#include <string>

#include <forevervalidator/camera.h>
#include <forevervalidator/validation.h>

namespace forevervalidator {
Result<AssetSource> OpenInstalledPackDirectory(
        const std::string &packDirectory) noexcept;
// Authenticates <packName>.pak with packlist.dat, resolves the vehicle
// collector's declared race-camera GBXs, and decodes their loaded values.
Result<camera::RaceCameraEnvironment> LoadInstalledRaceCameraEnvironment(
        const std::string &packDirectory,
        const std::string &packName) noexcept;
Result<AssetBytes> ReadNativeReplayFile(
        const std::string &path,
        const ReplayIdentity &identity = {}) noexcept;
}  // namespace forevervalidator

#endif
