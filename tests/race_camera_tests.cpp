#include <forevervalidator/camera.h>

#include "engine/camera/race_camera_internal.h"
#include "race_camera_test_fixture.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>

namespace {

using forevervalidator::camera::ProfileInfo;
using forevervalidator::camera::RaceCameraOutput;
using forevervalidator::camera::RaceCameraEnvironment;
using forevervalidator::camera::RaceCameraProfile;
using forevervalidator::camera::RaceCameraQuery;
using forevervalidator::camera::RaceCameraSession;
using forevervalidator::camera::RaceCameraVehicleState;
using forevervalidator::camera::SegmentHit;
using forevervalidator::camera::SegmentQuery;
using forevervalidator::camera::detail::ToInternal;

const RaceCameraEnvironment &FixtureEnvironment() {
    return race_camera_test_fixture::Environment();
}

bool Check(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

bool NearlyEqual(float lhs, float rhs, float tolerance = 1.0e-5f) {
    return std::fabs(lhs - rhs) <= tolerance;
}

bool NearlyEqual(const GmVec3 &lhs, const GmVec3 &rhs,
                 float tolerance = 1.0e-5f) {
    return NearlyEqual(lhs.x, rhs.x, tolerance) &&
           NearlyEqual(lhs.y, rhs.y, tolerance) &&
           NearlyEqual(lhs.z, rhs.z, tolerance);
}

GmVec3 Normalize(const GmVec3 &value) {
    const float inverse =
            1.0f / std::sqrt(value.x * value.x + value.y * value.y +
                             value.z * value.z);
    return {value.x * inverse, value.y * inverse, value.z * inverse};
}

RaceCameraVehicleState IdentityVehicle(std::uint32_t timeMs = 0u) {
    RaceCameraVehicleState vehicle;
    vehicle.targetId = 0x1234u;
    vehicle.timeMs = timeMs;
    vehicle.transform.rotation = {1.0f, 0.0f, 0.0f, 0.0f};
    vehicle.transform.position = {0.0f, 0.0f, 0.0f};
    vehicle.cameraSupportUp = {0.0f, 1.0f, 0.0f};
    return vehicle;
}

RaceCameraOutput Evaluate(RaceCameraSession &session,
                          const RaceCameraVehicleState &vehicle) {
    RaceCameraQuery query;
    query.vehicle = vehicle;
    return session.Evaluate(query);
}

bool CheckPose(const RaceCameraOutput &output, const GmVec3 &position,
               const GmVec3 &direction, const char *name) {
    const GmIso4 transform = ToInternal(output.transform);
    bool okay = true;
    okay &= Check(NearlyEqual(transform.translation, position, 2.0e-5f), name);
    okay &= Check(NearlyEqual(transform.rotation.Row(GmAxis::Z),
                              Normalize(direction), 2.0e-5f),
                  name);
    return okay;
}

bool TestProfileMetadata() {
    const auto &race = ProfileInfo(RaceCameraProfile::Race);
    const auto &race2 = ProfileInfo(RaceCameraProfile::Race2);
    const auto &race3 = ProfileInfo(RaceCameraProfile::Race3);
    bool okay = true;
    okay &= Check(race.classId == 0x24085000u &&
                          race.className ==
                                  "CGameControlCameraTrackManiaRace" &&
                          !race.stadiumVehicleResourceIndex,
                  "Race profile metadata differs from the executable");
    okay &= Check(race2.classId == 0x24086000u &&
                          race2.stadiumVehicleResourceIndex == 1u,
                  "Race2 profile metadata differs from the executable");
    okay &= Check(race3.classId == 0x24087000u &&
                          race3.stadiumVehicleResourceIndex == 2u,
                  "Race3 profile metadata differs from the executable");
    return okay;
}

bool TestLoadedIdentityFixtures() {
    const RaceCameraVehicleState vehicle = IdentityVehicle();
    bool okay = true;

    RaceCameraSession race(FixtureEnvironment(), RaceCameraProfile::Race);
    race.Reset(vehicle);
    const RaceCameraOutput raceOutput = Evaluate(race, vehicle);
    okay &= CheckPose(raceOutput, {0.0f, 30.0f, -15.0f}, {0.0f, -30.0f, 15.0f},
                      "Race identity pose differs from loaded-profile oracle");
    okay &= Check(NearlyEqual(raceOutput.lens.fieldOfViewDegrees, 80.0f) &&
                          NearlyEqual(raceOutput.lens.parameter1, 30.0f) &&
                          NearlyEqual(raceOutput.lens.parameter2, 3.0f),
                  "Race identity lens differs from loaded-profile oracle");

    RaceCameraSession race2(FixtureEnvironment(), RaceCameraProfile::Race2);
    race2.Reset(vehicle);
    const RaceCameraOutput race2Output = Evaluate(race2, vehicle);
    okay &= CheckPose(race2Output, {0.0f, 3.299999952316284f, -5.0f},
                      {0.0f, -0.9899999499320984f, 5.0f},
                      "Race2 identity pose differs from loaded-profile oracle");
    okay &= Check(NearlyEqual(race2Output.lens.fieldOfViewDegrees, 75.0f) &&
                          NearlyEqual(race2Output.lens.parameter1, 30.0f) &&
                          NearlyEqual(race2Output.lens.parameter2, 3.0f),
                  "Race2 identity lens differs from loaded-profile oracle");

    RaceCameraSession race3(FixtureEnvironment(), RaceCameraProfile::Race3);
    race3.Reset(vehicle);
    const RaceCameraOutput race3Output = Evaluate(race3, vehicle);
    okay &= CheckPose(race3Output, {0.0f, 2.200000047683716f, -4.5f},
                      {0.0f, -0.2640000581741333f, 4.5f},
                      "Race3 identity pose differs from loaded-profile oracle");
    okay &= Check(
            NearlyEqual(race3Output.lens.fieldOfViewDegrees, 75.0f) &&
                    NearlyEqual(race3Output.lens.parameter1,
                                5.008991718292236f) &&
                    NearlyEqual(race3Output.lens.parameter2, 1.0f),
            "Race3 identity lens differs from captured executable fixture");
    return okay;
}

bool TestRaceWorldAlignedCone() {
    RaceCameraVehicleState resetVehicle = IdentityVehicle();
    RaceCameraSession identityRotation(
            FixtureEnvironment(), RaceCameraProfile::Race);
    RaceCameraSession rotatedVehicle(
            FixtureEnvironment(), RaceCameraProfile::Race);
    identityRotation.Reset(resetVehicle);
    rotatedVehicle.Reset(resetVehicle);

    RaceCameraVehicleState identityQuery = resetVehicle;
    identityQuery.transform.position = {20.0f, 0.0f, 0.0f};
    identityQuery.signedSpeed = 60.0f;

    RaceCameraVehicleState rotatedQuery = identityQuery;
    constexpr float HalfSqrtTwo = 0.7071067690849304f;
    rotatedQuery.transform.rotation = {HalfSqrtTwo, 0.0f, HalfSqrtTwo, 0.0f};

    const GmIso4 identityOutput =
            ToInternal(Evaluate(identityRotation, identityQuery).transform);
    const GmIso4 rotatedOutput =
            ToInternal(Evaluate(rotatedVehicle, rotatedQuery).transform);

    bool okay = Check(NearlyEqual(identityOutput.translation,
                                  {20.0f, 30.0f, -25.0f}, 2.0e-5f),
                      "Race cone edge differs from the executable");
    okay &= Check(NearlyEqual(identityOutput.translation,
                              rotatedOutput.translation, 2.0e-5f),
                  "Race incorrectly followed target rotation");
    return okay;
}

bool TestRace2LoadedSteeringDisable() {
    RaceCameraSession neutral(FixtureEnvironment(), RaceCameraProfile::Race2);
    RaceCameraSession steering(FixtureEnvironment(), RaceCameraProfile::Race2);
    RaceCameraVehicleState neutralVehicle = IdentityVehicle();
    RaceCameraVehicleState steeringVehicle = neutralVehicle;
    neutral.Reset(neutralVehicle);
    steering.Reset(steeringVehicle);

    bool okay = true;
    for (const std::uint32_t timeMs : {0u, 100u, 300u, 700u, 1500u, 3000u}) {
        neutralVehicle.timeMs = timeMs;
        steeringVehicle.timeMs = timeMs;
        neutralVehicle.signedSpeed = 200.0f;
        steeringVehicle.signedSpeed = 200.0f;
        steeringVehicle.steering = 1.0f;
        const GmIso4 neutralTransform =
                ToInternal(Evaluate(neutral, neutralVehicle).transform);
        const GmIso4 steeringTransform =
                ToInternal(Evaluate(steering, steeringVehicle).transform);
        okay &= Check(
                NearlyEqual(neutralTransform.translation,
                            steeringTransform.translation) &&
                        NearlyEqual(neutralTransform.rotation.basisX,
                                    steeringTransform.rotation.basisX) &&
                        NearlyEqual(neutralTransform.rotation.basisY,
                                    steeringTransform.rotation.basisY) &&
                        NearlyEqual(neutralTransform.rotation.basisZ,
                                    steeringTransform.rotation.basisZ),
                "Race2 applied steering despite loaded isRollFromInput=false");
    }
    return okay;
}

bool TestTurboFovFixture() {
    bool okay = true;
    for (const RaceCameraProfile profile :
         {RaceCameraProfile::Race2, RaceCameraProfile::Race3}) {
        RaceCameraSession session(FixtureEnvironment(), profile);
        RaceCameraVehicleState vehicle = IdentityVehicle();
        vehicle.turbo = 1.0f;
        session.Reset(vehicle);
        (void)Evaluate(session, vehicle);
        vehicle.timeMs = 500u;
        const RaceCameraOutput output = Evaluate(session, vehicle);
        okay &= Check(
                NearlyEqual(output.lens.fieldOfViewDegrees, 80.26664733886719f,
                            2.0e-5f),
                "Turbo FOV inverse-cosine fixture differs from executable");
    }
    return okay;
}

bool TestRace3CollisionGate() {
    RaceCameraSession session(FixtureEnvironment(), RaceCameraProfile::Race3);
    RaceCameraVehicleState vehicle = IdentityVehicle();
    session.Reset(vehicle);

    int queryCount = 0;
    SegmentQuery lastQuery{};
    auto collision =
            [&](const SegmentQuery &query) -> std::optional<SegmentHit> {
        ++queryCount;
        lastQuery = query;
        return std::nullopt;
    };

    RaceCameraQuery query;
    query.vehicle = vehicle;
    query.segmentCollision = collision;
    (void)session.Evaluate(query);

    query.vehicle.timeMs = 1u;
    query.vehicle.wheelHasSurface = {{false, false, false, false}};
    (void)session.Evaluate(query);
    bool okay = Check(queryCount == 0,
                      "Race3 collision ran before the prior low-surface frame");

    query.vehicle.timeMs = 3u;
    (void)session.Evaluate(query);
    okay &= Check(queryCount == 1,
                  "Race3 collision did not run after two low-surface frames");
    okay &= Check(
            NearlyEqual(lastQuery.start.x, lastQuery.end.x) &&
                    NearlyEqual(lastQuery.start.z, lastQuery.end.z) &&
                    NearlyEqual(lastQuery.end.y - lastQuery.start.y, -4.0f),
            "Race3 collision segment differs from loaded radius 2.0");
    return okay;
}

bool TestRace3LowSpeedFlightTransition() {
    RaceCameraSession normal(FixtureEnvironment(), RaceCameraProfile::Race3);
    RaceCameraSession transitioned(
            FixtureEnvironment(), RaceCameraProfile::Race3);
    RaceCameraVehicleState normalVehicle = IdentityVehicle();
    RaceCameraVehicleState transitionedVehicle = normalVehicle;
    normal.Reset(normalVehicle);
    transitioned.Reset(transitionedVehicle);

    for (const std::uint32_t timeMs : {0u, 1u, 3u, 503u, 1003u}) {
        normalVehicle.timeMs = timeMs;
        transitionedVehicle.timeMs = timeMs;
        normalVehicle.wheelHasSurface =
                timeMs == 0u
                        ? std::array<bool, 4>{{true, true, true, true}}
                        : std::array<bool, 4>{{false, false, false, false}};
        transitionedVehicle.wheelHasSurface = normalVehicle.wheelHasSurface;
        transitionedVehicle.cameraFlightTransition = 1.0f;
        (void)Evaluate(normal, normalVehicle);
        (void)Evaluate(transitioned, transitionedVehicle);
    }

    const GmIso4 normalOutput =
            ToInternal(Evaluate(normal, normalVehicle).transform);
    const GmIso4 transitionedOutput =
            ToInternal(Evaluate(transitioned, transitionedVehicle).transform);
    return Check(
            !NearlyEqual(normalOutput.translation,
                         transitionedOutput.translation, 1.0e-4f),
            "Race3 ignored the low-speed airborne camera transition input");
}

bool TestDistinctWheelSignals() {
    RaceCameraVehicleState baseVehicle = IdentityVehicle();
    bool okay = true;

    RaceCameraSession race2Ground(
            FixtureEnvironment(), RaceCameraProfile::Race2);
    RaceCameraSession race2Airborne(
            FixtureEnvironment(), RaceCameraProfile::Race2);
    race2Ground.Reset(baseVehicle);
    race2Airborne.Reset(baseVehicle);
    RaceCameraVehicleState race2GroundVehicle = baseVehicle;
    RaceCameraVehicleState race2AirborneVehicle = baseVehicle;
    for (const std::uint32_t timeMs : {0u, 1u, 202u, 1000u, 2000u}) {
        race2GroundVehicle.timeMs = timeMs;
        race2AirborneVehicle.timeMs = timeMs;
        if (timeMs != 0u) {
            race2AirborneVehicle.wheelContact = {{false, false, false, false}};
        }
        race2AirborneVehicle.wheelHasSurface = {{true, true, true, true}};
        (void)Evaluate(race2Ground, race2GroundVehicle);
        (void)Evaluate(race2Airborne, race2AirborneVehicle);
    }
    const GmIso4 race2GroundOutput =
            ToInternal(Evaluate(race2Ground, race2GroundVehicle).transform);
    const GmIso4 race2AirborneOutput =
            ToInternal(Evaluate(race2Airborne, race2AirborneVehicle).transform);
    okay &= Check(!NearlyEqual(race2GroundOutput.translation,
                               race2AirborneOutput.translation, 1.0e-4f),
                  "Race2 ignored the wheel contact-present signal");

    RaceCameraSession race3ContactPresent(
            FixtureEnvironment(), RaceCameraProfile::Race3);
    RaceCameraSession race3ContactAbsent(
            FixtureEnvironment(), RaceCameraProfile::Race3);
    RaceCameraVehicleState contactPresentVehicle = baseVehicle;
    RaceCameraVehicleState contactAbsentVehicle = baseVehicle;
    contactAbsentVehicle.wheelContact = {{false, false, false, false}};
    race3ContactPresent.Reset(contactPresentVehicle);
    race3ContactAbsent.Reset(contactAbsentVehicle);
    for (const std::uint32_t timeMs : {0u, 3u, 500u}) {
        contactPresentVehicle.timeMs = timeMs;
        contactAbsentVehicle.timeMs = timeMs;
        const GmIso4 present = ToInternal(
                Evaluate(race3ContactPresent, contactPresentVehicle).transform);
        const GmIso4 absent = ToInternal(
                Evaluate(race3ContactAbsent, contactAbsentVehicle).transform);
        okay &= Check(
                NearlyEqual(present.translation, absent.translation, 2.0e-5f),
                "Race3 incorrectly used wheel contact-present state");
    }
    return okay;
}

bool TestRace3CollisionCorrection() {
    RaceCameraSession noHit(FixtureEnvironment(), RaceCameraProfile::Race3);
    RaceCameraSession hit(FixtureEnvironment(), RaceCameraProfile::Race3);
    RaceCameraQuery noHitQuery;
    RaceCameraQuery hitQuery;
    noHitQuery.vehicle = IdentityVehicle();
    hitQuery.vehicle = noHitQuery.vehicle;
    noHit.Reset(noHitQuery.vehicle);
    hit.Reset(hitQuery.vehicle);

    noHitQuery.segmentCollision =
            [](const SegmentQuery &) -> std::optional<SegmentHit> {
        return std::nullopt;
    };
    hitQuery.segmentCollision =
            [](const SegmentQuery &) -> std::optional<SegmentHit> {
        return SegmentHit{0.0f};
    };

    for (const std::uint32_t timeMs : {0u, 1u, 3u, 13u}) {
        noHitQuery.vehicle.timeMs = timeMs;
        hitQuery.vehicle.timeMs = timeMs;
        if (timeMs != 0u) {
            noHitQuery.vehicle.wheelHasSurface = {{false, false, false, false}};
            hitQuery.vehicle.wheelHasSurface =
                    noHitQuery.vehicle.wheelHasSurface;
        }
        (void)noHit.Evaluate(noHitQuery);
        (void)hit.Evaluate(hitQuery);
    }

    const RaceCameraOutput noHitOutput = noHit.Evaluate(noHitQuery);
    const RaceCameraOutput hitOutput = hit.Evaluate(hitQuery);
    return Check(hitOutput.transform.position.y >
                         noHitOutput.transform.position.y + 0.01f,
                 "Race3 did not apply the segment collision correction");
}

} // namespace

int main() {
    bool okay = true;
    okay &= TestProfileMetadata();
    okay &= TestLoadedIdentityFixtures();
    okay &= TestRaceWorldAlignedCone();
    okay &= TestRace2LoadedSteeringDisable();
    okay &= TestTurboFovFixture();
    okay &= TestRace3CollisionGate();
    okay &= TestRace3LowSpeedFlightTransition();
    okay &= TestDistinctWheelSignals();
    okay &= TestRace3CollisionCorrection();
    return okay ? 0 : 1;
}
