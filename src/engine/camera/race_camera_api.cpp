#include <forevervalidator/camera.h>

#include <array>
#include <memory>
#include <stdexcept>
#include <utility>

#include "engine/camera/race_camera_internal.h"

namespace forevervalidator::camera {
namespace {

constexpr std::array<RaceCameraProfileInfo, 3u> ProfileInfos{{
        {RaceCameraProfile::Race, 0x24085000u,
         "CGameControlCameraTrackManiaRace", std::nullopt},
        {RaceCameraProfile::Race2, 0x24086000u,
         "CGameControlCameraTrackManiaRace2", 1u},
        {RaceCameraProfile::Race3, 0x24087000u,
         "CGameControlCameraTrackManiaRace3", 2u},
}};

} // namespace

const RaceCameraProfileInfo &ProfileInfo(RaceCameraProfile profile) noexcept {
    const std::size_t index = static_cast<std::size_t>(profile);
    return ProfileInfos[index < ProfileInfos.size() ? index : 0u];
}

class RaceCameraEnvironment::Impl {
  public:
    explicit Impl(detail::EnvironmentConfig value)
        : config(std::move(value)) {}

    detail::EnvironmentConfig config;
};

RaceCameraEnvironment::RaceCameraEnvironment() = default;
RaceCameraEnvironment::~RaceCameraEnvironment() = default;
RaceCameraEnvironment::RaceCameraEnvironment(
        const RaceCameraEnvironment &) noexcept = default;
RaceCameraEnvironment &RaceCameraEnvironment::operator=(
        const RaceCameraEnvironment &) noexcept = default;
RaceCameraEnvironment::RaceCameraEnvironment(
        RaceCameraEnvironment &&) noexcept = default;
RaceCameraEnvironment &RaceCameraEnvironment::operator=(
        RaceCameraEnvironment &&) noexcept = default;

RaceCameraEnvironment::RaceCameraEnvironment(
        std::shared_ptr<const Impl> impl)
    : impl_(std::move(impl)) {}

bool RaceCameraEnvironment::HasProfile(
        RaceCameraProfile profile) const noexcept {
    if (!impl_) {
        return false;
    }
    switch (profile) {
    case RaceCameraProfile::Race:
        return !impl_->config.raceResources.empty();
    case RaceCameraProfile::Race2:
        return !impl_->config.race2Resources.empty();
    case RaceCameraProfile::Race3:
        return !impl_->config.race3Resources.empty();
    }
    return false;
}

std::string_view RaceCameraEnvironment::SourceName() const noexcept {
    return impl_ ? std::string_view(impl_->config.sourceName)
                 : std::string_view{};
}

std::size_t RaceCameraEnvironment::ResourceCount(
        RaceCameraProfile profile) const noexcept {
    if (!impl_) {
        return 0u;
    }
    switch (profile) {
    case RaceCameraProfile::Race:
        return impl_->config.raceResources.size();
    case RaceCameraProfile::Race2:
        return impl_->config.race2Resources.size();
    case RaceCameraProfile::Race3:
        return impl_->config.race3Resources.size();
    }
    return 0u;
}

std::string_view RaceCameraEnvironment::ResourceName(
        RaceCameraProfile profile,
        std::size_t resourceIndex) const noexcept {
    if (!impl_) {
        return {};
    }
    switch (profile) {
    case RaceCameraProfile::Race:
        return resourceIndex < impl_->config.raceResources.size()
                       ? std::string_view(
                                 impl_->config.raceResources[resourceIndex]
                                         .sourceName)
                       : std::string_view{};
    case RaceCameraProfile::Race2:
        return resourceIndex < impl_->config.race2Resources.size()
                       ? std::string_view(
                                 impl_->config.race2Resources[resourceIndex]
                                         .sourceName)
                       : std::string_view{};
    case RaceCameraProfile::Race3:
        return resourceIndex < impl_->config.race3Resources.size()
                       ? std::string_view(
                                 impl_->config.race3Resources[resourceIndex]
                                         .sourceName)
                       : std::string_view{};
    }
    return {};
}

RaceCameraEnvironment detail::RaceCameraEnvironmentFactory::Create(
        EnvironmentConfig config) {
    if (!config.race && !config.raceResources.empty()) {
        config.race = config.raceResources.back().config;
    }
    if (!config.race2 && !config.race2Resources.empty()) {
        config.race2 = config.race2Resources.back().config;
    }
    if (!config.race3 && !config.race3Resources.empty()) {
        config.race3 = config.race3Resources.back().config;
    }
    if (config.race && config.raceResources.empty()) {
        config.raceResources.push_back(
                {config.sourceName, *config.race});
    }
    if (config.race2 && config.race2Resources.empty()) {
        config.race2Resources.push_back(
                {config.sourceName, *config.race2});
    }
    if (config.race3 && config.race3Resources.empty()) {
        config.race3Resources.push_back(
                {config.sourceName, *config.race3});
    }
    return RaceCameraEnvironment(
            std::make_shared<const RaceCameraEnvironment::Impl>(
                    std::move(config)));
}

const detail::EnvironmentConfig &
detail::RaceCameraEnvironmentFactory::Config(
        const RaceCameraEnvironment &environment) {
    if (!environment.impl_) {
        throw std::invalid_argument("race camera environment is empty");
    }
    return environment.impl_->config;
}

class RaceCameraSession::Impl {
  public:
    Impl(const RaceCameraEnvironment &environment,
         RaceCameraProfile profile,
         std::optional<std::size_t> resourceIndex)
        : controller(detail::CreateController(
                  detail::RaceCameraEnvironmentFactory::Config(environment),
                  profile, resourceIndex)) {
        if (!controller) {
            throw std::invalid_argument("unsupported race camera profile");
        }
    }

    std::unique_ptr<detail::Controller> controller;
};

RaceCameraSession::RaceCameraSession(
        const RaceCameraEnvironment &environment,
        RaceCameraProfile profile)
    : impl_(std::make_unique<Impl>(environment, profile, std::nullopt)) {}

RaceCameraSession::RaceCameraSession(
        const RaceCameraEnvironment &environment,
        RaceCameraProfile profile,
        std::size_t resourceIndex)
    : impl_(std::make_unique<Impl>(
              environment, profile, resourceIndex)) {}

RaceCameraSession::~RaceCameraSession() = default;

RaceCameraSession::RaceCameraSession(RaceCameraSession &&) noexcept = default;

RaceCameraSession &
RaceCameraSession::operator=(RaceCameraSession &&) noexcept = default;

RaceCameraProfile RaceCameraSession::Profile() const noexcept {
    return impl_->controller->Profile();
}

void RaceCameraSession::Reset(const RaceCameraVehicleState &vehicle) {
    impl_->controller->Reset(detail::ToInternal(vehicle));
}

RaceCameraOutput RaceCameraSession::Evaluate(const RaceCameraQuery &query) {
    return detail::ToPublic(impl_->controller->Evaluate(
            detail::ToInternal(query.vehicle), query.segmentCollision));
}

} // namespace forevervalidator::camera
