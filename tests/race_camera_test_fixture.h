#pragma once

#include <forevervalidator/camera.h>

#include "engine/camera/race_camera_internal.h"

namespace race_camera_test_fixture {

inline forevervalidator::camera::detail::SmoothRealConfig Smooth(
        float delta,
        std::uint32_t rise,
        std::uint32_t fall) {
    return {delta, rise, fall};
}

inline forevervalidator::camera::RaceCameraEnvironment BuildEnvironment() {
    using namespace forevervalidator::camera::detail;

    EnvironmentConfig environment;
    environment.sourceName = "captured camera test fixture";

    RaceConfig race;
    race.common.height = 30.0f;
    race.common.minimumDistance = 10.0f;
    race.common.maximumDistance = 30.0f;
    race.common.baseFieldOfView = 80.0f;
    race.common.initialFieldOfView = 80.0f;
    race.common.lensParameter1 = 30.0f;
    race.common.lensParameter2 = 3.0f;
    race.common.lookFactor = 0.0f;
    race.common.ignoreTargetRotation = true;
    race.coneAperture = 0.0f;
    race.coneMinimumSpeed = 0.0f;
    race.coneMaximumSpeed = 60.0f;
    race.useSpeedDirection = false;
    race.cameraHeight = 30.0f;
    race.cameraDistance = 15.0f;
    environment.race = race;

    Race2Config race2;
    race2.common.height = 3.299999952316284f;
    race2.common.minimumDistance = 4.0f;
    race2.common.maximumDistance = 5.0f;
    race2.common.baseFieldOfView = 75.0f;
    race2.common.initialFieldOfView = 75.0f;
    race2.common.lensParameter1 = 30.0f;
    race2.common.lensParameter2 = 3.0f;
    race2.common.lookFactor = 0.699999988079071f;
    race2.smoothers = {{
            Smooth(1.0f, 2000u, 1000u),
            Smooth(-0.699999988f, 750u, 1000u),
            Smooth(-0.800000012f, 2000u, 1500u),
            Smooth(10.5f, 1000u, 1400u),
            Smooth(-0.600000024f, 1000u, 1400u),
            Smooth(-0.349999994f, 500u, 2000u),
            Smooth(-0.400000006f, 2000u, 1500u),
            Smooth(1.0f, 2000u, 1500u),
            Smooth(2.0f, 3000u, 2500u),
            Smooth(4.0f, 3000u, 2500u),
            Smooth(-0.200000003f, 3000u, 2500u),
            Smooth(1.0f, 2000u, 1200u),
            Smooth(1.0f, 2000u, 1200u),
            Smooth(1.0f, 2500u, 1500u),
            Smooth(1.0f, 2000u, 1500u),
    }};
    race2.flyingLookCurve = {
            {0.0f, 0.0f}, {1.0f, 0.0f}, {5.0f, -0.300000012f},
            {10.0f, -0.349999994f}, {20.0f, -0.400000006f},
            {40.0f, -0.449999988f}, {60.0f, -0.5f},
    };
    race2.yawFromSpeedCurve = {
            {-10.0f, 2.0f}, {0.0f, 0.0f}, {50.0f, 2.0f},
            {100.0f, 4.0f}, {200.0f, 8.0f}, {400.0f, 14.0f},
    };
    race2.rollFromSpeedCurve = {
            {-10.0f, 1.0f}, {0.0f, 0.0f}, {50.0f, 1.0f},
            {100.0f, 2.0f}, {200.0f, 2.5f}, {400.0f, 3.0f},
    };
    race2.rollFromInput = false;
    environment.race2 = race2;

    Race3Config race3;
    race3.common.height = 1.5f;
    race3.common.minimumDistance = 3.0f;
    race3.common.maximumDistance = 4.0f;
    race3.common.baseFieldOfView = 75.0f;
    race3.common.initialFieldOfView = 75.0f;
    race3.common.lensParameter1 = 5.008991718292236f;
    race3.common.lensParameter2 = 1.0f;
    race3.common.lookFactor = 0.8799999952316284f;
    race3.reverse = Smooth(1.0f, 2000u, 2000u);
    race3.lowSpeedFlightTarget = Smooth(1.0f, 2000u, 2000u);
    race3.flyingDirectionBlend = Smooth(1.0f, 2000u, 1000u);
    race3.flyingUpBlend = Smooth(1.0f, 2000u, 2000u);
    race3.flyingSpeedEffect = Smooth(1.0f, 1000u, 1000u);
    race3.flyingUpSpeedEffect = Smooth(1.0f, 1000u, 1000u);
    race3.flyingLook = Smooth(-0.200000003f, 3000u, 2500u);
    race3.flyingRadius = Smooth(3.0f, 3000u, 2500u);
    race3.gasFar = Smooth(1.0f, 2000u, 1000u);
    race3.brakeFar = Smooth(-0.699999988f, 750u, 1000u);
    race3.steerFar = Smooth(-0.800000012f, 2000u, 1500u);
    race3.turboFieldOfView = Smooth(10.5f, 1000u, 1400u);
    race3.turboFar = Smooth(-0.600000024f, 1000u, 1400u);
    race3.gearFar = Smooth(-0.349999994f, 500u, 2000u);
    race3.burningLook = Smooth(-0.400000006f, 2000u, 1500u);
    race3.burningRadius = Smooth(1.0f, 2000u, 1500u);
    race3.speedModulationCurve = {
            {-100.0f, 1.5f}, {-50.0f, 1.20000005f}, {0.0f, 1.0f},
            {250.0f, 1.5f}, {500.0f, 2.0f}, {700.0f, 3.0f},
    };
    race3.flyingLookCurve = race2.flyingLookCurve;
    race3.flyingDownCurve = {
            {0.0f, 0.0f}, {1.0f, 0.0f},
            {1.10000002f, 0.0299999993f},
            {1.29999995f, 0.100000001f}, {2.0f, 0.300000012f},
            {10.0f, 0.349999994f}, {20.0f, 0.400000006f},
            {40.0f, 0.5f}, {60.0f, 0.600000024f},
    };
    environment.race3 = race3;

    return RaceCameraEnvironmentFactory::Create(std::move(environment));
}

inline const forevervalidator::camera::RaceCameraEnvironment &Environment() {
    static const auto environment = BuildEnvironment();
    return environment;
}

} // namespace race_camera_test_fixture
