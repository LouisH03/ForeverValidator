#include <forevervalidator/experimental/physics_sandbox.h>

#include "engine/rendering/plug_tree.h"
#include "engine/scene/static_scene_model.h"
#include "format/static_solid/static_solid_geometry_decoder.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

bool Check(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

void AppendFloat(std::vector<std::uint8_t> *bytes, float value) {
    const std::size_t offset = bytes->size();
    bytes->resize(offset + sizeof(value));
    std::memcpy(bytes->data() + offset, &value, sizeof(value));
}

bool NearlyEqual(float lhs, float rhs) {
    return std::fabs(lhs - rhs) < 0.0001f;
}

bool TestUvDecoding() {
    std::vector<std::uint8_t> uv2Bytes;
    for (float value : {0.25f, 0.75f, -1.0f, 2.0f}) {
        AppendFloat(&uv2Bytes, value);
    }
    GxTexCoordSet uv2;
    bool okay = Check(
            DecodeStaticSolidTexCoordStream(
                    uv2Bytes.data(), 2u, 2u, 8u, &uv2),
            "2D UV stream was rejected");
    const GxTexCoord4 first = uv2.Coordinate4At(0u);
    const GxTexCoord4 second = uv2.Coordinate4At(1u);
    okay &= Check(
            uv2.Dimension() == GxTexCoordDimension::Two &&
                    uv2.Count() == 2u &&
                    NearlyEqual(first.u, 0.25f) &&
                    NearlyEqual(first.v, 0.75f) &&
                    NearlyEqual(second.u, -1.0f) &&
                    NearlyEqual(second.v, 2.0f),
            "2D UV values were not preserved");

    std::vector<std::uint8_t> uv4Bytes;
    for (float value : {1.0f, 2.0f, 3.0f, 4.0f}) {
        AppendFloat(&uv4Bytes, value);
    }
    GxTexCoordSet uv4;
    okay &= Check(
            DecodeStaticSolidTexCoordStream(
                    uv4Bytes.data(), 1u, 4u, 16u, &uv4),
            "4D UV stream was rejected");
    const GxTexCoord4 expanded = uv4.Coordinate4At(0u);
    okay &= Check(
            uv4.Dimension() == GxTexCoordDimension::Four &&
                    NearlyEqual(expanded.u, 1.0f) &&
                    NearlyEqual(expanded.v, 2.0f) &&
                    NearlyEqual(expanded.w, 3.0f) &&
                    NearlyEqual(expanded.q, 4.0f),
            "4D UV values were not preserved");
    okay &= Check(
            !DecodeStaticSolidTexCoordStream(
                    uv2Bytes.data(), 2u, 2u, 12u, &uv2),
            "invalid UV stride was accepted");
    return okay;
}

bool TestTransformComposition() {
    GmIso4 parent;
    parent.SetIdentity();
    parent.SetTranslation({10.0f, 20.0f, 30.0f});
    GmIso4 local;
    local.SetIdentity();
    local.SetTranslation({1.0f, 2.0f, 3.0f});

    CPlugTree tree;
    tree.SetUseLocation(1);
    tree.SetLocation(local);
    GmIso4 world;
    tree.ComposeCollisionIso(parent, world);
    return Check(
            NearlyEqual(world.translation.x, 11.0f) &&
                    NearlyEqual(world.translation.y, 22.0f) &&
                    NearlyEqual(world.translation.z, 33.0f),
            "tree local transform was not composed with its parent");
}

bool TestProvenanceAndImmutableScene() {
    GmIso4 identity;
    identity.SetIdentity();
    StaticSceneModel model(
            StaticSolidPrototype{},
            identity,
            StaticScenePurpose::SubMobil);
    StaticSceneProvenance provenance;
    provenance.blockName = "StadiumRoadMain";
    provenance.collection = "Stadium";
    provenance.descriptorPath = "GameData/StadiumRoad.Block.Gbx";
    provenance.sceneObjectId = "mobil-3";
    provenance.placementIdentity = 42u;
    provenance.blockInstanceId = 7u;
    provenance.variant = 2u;
    provenance.componentIndex = 3u;
    provenance.authored = true;
    model.SetProvenance(provenance);

    StaticSceneModelCollection models;
    bool okay = Check(models.Add(std::move(model)),
                      "scene model could not be stored");
    const StaticSceneModel &stored = models.Models().front();
    okay &= Check(
            stored.Purpose() == StaticScenePurpose::SubMobil &&
                    stored.Provenance().blockName == "StadiumRoadMain" &&
                    stored.Provenance().placementIdentity == 42u &&
                    stored.Provenance().blockInstanceId == 7u &&
                    stored.Provenance().variant == 2u &&
                    stored.Provenance().componentIndex == 3u &&
                    stored.Provenance().authored,
            "authored provenance changed after scene-model storage");

    using Scene =
            forevervalidator::experimental::PhysicsSandboxRenderScene;
    using Handle =
            forevervalidator::experimental::
                    PhysicsSandboxRenderSceneHandle;
    static_assert(std::is_same_v<Handle, std::shared_ptr<const Scene>>);
    const Handle scene = std::make_shared<const Scene>();
    okay &= Check(scene->meshes.empty() && scene->instances.empty(),
                  "immutable render-scene handle was not readable");
    return okay;
}

}  // namespace

int main() {
    bool okay = TestUvDecoding();
    okay &= TestTransformComposition();
    okay &= TestProvenanceAndImmutableScene();
    return okay ? 0 : 1;
}
