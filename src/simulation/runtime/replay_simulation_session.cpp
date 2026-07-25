#include "simulation/runtime/replay_simulation_session.h"
#include <array>
#include <cmath>
#include <limits>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>

#include "engine/core/binary32_math.h"
#include "engine/physics/geometry/gm_surface.h"
#include "engine/physics/geometry/plug_surface.h"
#include "engine/scene/plug_solid.h"
#include "engine/rendering/plug_material.h"
#include "engine/rendering/plug_tree.h"
#include "engine/rendering/plug_visual.h"
#include "format/archive/archive_class_ids.h"
#include "simulation/replay/replay_map_scene.h"
#include "simulation/runtime/replay_environment.h"
#include "simulation/runtime/replay_physics_world.h"
#include "simulation/runtime/replay_simulation_runtime.h"
#include "simulation/runtime/replay_vehicle_body.h"
#include "simulation/runtime/replay_vehicle_simulation.h"
#include "engine/game/trackmania_race.h"
#include "simulation/runtime/replay_deterministic_execution.h"
namespace {

GmVec3 TransformPoint(const GmVec3 &point, const GmIso4 &iso) {
    GmVec3 transformed;
    transformed.SetMult(point, iso);
    return transformed;
}

void AppendTriangle(std::vector<ReplayStaticCollisionTriangle> &triangles,
                    const GmVec3 &a,
                    const GmVec3 &b,
                    const GmVec3 &c,
                    const GmIso4 &iso) {
    triangles.push_back({
            TransformPoint(a, iso),
            TransformPoint(b, iso),
            TransformPoint(c, iso)});
}

void AppendBox(std::vector<ReplayStaticCollisionTriangle> &triangles,
               const GmSurfBox &box,
               const GmIso4 &iso) {
    const GmVec3 &c = box.center;
    const GmVec3 &h = box.halfExtents;
    const std::array<GmVec3, 8u> vertices{{
            {c.x - h.x, c.y - h.y, c.z - h.z},
            {c.x + h.x, c.y - h.y, c.z - h.z},
            {c.x + h.x, c.y + h.y, c.z - h.z},
            {c.x - h.x, c.y + h.y, c.z - h.z},
            {c.x - h.x, c.y - h.y, c.z + h.z},
            {c.x + h.x, c.y - h.y, c.z + h.z},
            {c.x + h.x, c.y + h.y, c.z + h.z},
            {c.x - h.x, c.y + h.y, c.z + h.z},
    }};
    constexpr std::array<std::array<unsigned, 3u>, 12u> Faces{{
            {{0u, 2u, 1u}}, {{0u, 3u, 2u}},
            {{4u, 5u, 6u}}, {{4u, 6u, 7u}},
            {{0u, 1u, 5u}}, {{0u, 5u, 4u}},
            {{3u, 7u, 6u}}, {{3u, 6u, 2u}},
            {{0u, 4u, 7u}}, {{0u, 7u, 3u}},
            {{1u, 2u, 6u}}, {{1u, 6u, 5u}},
    }};
    for (const auto &face : Faces) {
        AppendTriangle(triangles,
                       vertices[face[0]],
                       vertices[face[1]],
                       vertices[face[2]],
                       iso);
    }
}

void AppendEllipsoid(std::vector<ReplayStaticCollisionTriangle> &triangles,
                     const GmVec3 &radii,
                     const GmIso4 &iso) {
    constexpr unsigned Latitudes = 8u;
    constexpr unsigned Longitudes = 12u;
    constexpr float Pi = 3.14159265358979323846f;
    const auto point = [&](unsigned latitude, unsigned longitude) {
        const float phi = -0.5f * Pi +
                Pi * static_cast<float>(latitude) /
                        static_cast<float>(Latitudes);
        const float theta = 2.0f * Pi * static_cast<float>(longitude) /
                static_cast<float>(Longitudes);
        const float ring = std::cos(phi);
        return GmVec3{
                radii.x * ring * std::cos(theta),
                radii.y * std::sin(phi),
                radii.z * ring * std::sin(theta)};
    };
    for (unsigned latitude = 0u; latitude < Latitudes; ++latitude) {
        for (unsigned longitude = 0u; longitude < Longitudes; ++longitude) {
            const unsigned nextLongitude = (longitude + 1u) % Longitudes;
            const GmVec3 a = point(latitude, longitude);
            const GmVec3 b = point(latitude, nextLongitude);
            const GmVec3 c = point(latitude + 1u, nextLongitude);
            const GmVec3 d = point(latitude + 1u, longitude);
            if (latitude != 0u) {
                AppendTriangle(triangles, a, c, b, iso);
            }
            if (latitude + 1u != Latitudes) {
                AppendTriangle(triangles, a, d, c, iso);
            }
        }
    }
}

void AppendSurface(std::vector<ReplayStaticCollisionTriangle> &triangles,
                   const GmSurf &surface,
                   const GmIso4 &iso) {
    if (const auto *mesh = dynamic_cast<const GmSurfMesh *>(&surface)) {
        for (u32 index = 0u; index < mesh->TriangleCount(); ++index) {
            const GmSurfMeshTriangle &triangle = mesh->Triangle(index);
            AppendTriangle(triangles,
                           mesh->Vertex(triangle.vertexIndex[0]),
                           mesh->Vertex(triangle.vertexIndex[1]),
                           mesh->Vertex(triangle.vertexIndex[2]),
                           iso);
        }
        return;
    }
    if (const auto *polygon = dynamic_cast<const GmSurfPolygon *>(&surface)) {
        for (std::uint8_t index = 1u;
             index + 1u < polygon->vertexCount;
             ++index) {
            AppendTriangle(triangles,
                           polygon->vertices[0],
                           polygon->vertices[index],
                           polygon->vertices[index + 1u],
                           iso);
        }
        return;
    }
    if (const auto *box = dynamic_cast<const GmSurfBox *>(&surface)) {
        AppendBox(triangles, *box, iso);
        return;
    }
    if (const auto *ellipsoid =
                dynamic_cast<const GmSurfEllipsoid *>(&surface)) {
        AppendEllipsoid(triangles, ellipsoid->radii, iso);
        return;
    }
    if (const auto *sphere = dynamic_cast<const GmSurfSphere *>(&surface)) {
        AppendEllipsoid(triangles,
                        {sphere->radius, sphere->radius, sphere->radius},
                        iso);
    }
}

void AppendTree(std::vector<ReplayStaticCollisionTriangle> &triangles,
                const CPlugTree &tree,
                const GmIso4 &parentIso) {
    GmIso4 iso;
    tree.ComposeCollisionIso(parentIso, iso);
    if (tree.AllowsSurfaceCollision()) {
        const CPlugSurface *surface = tree.Surface();
        if (surface != nullptr && surface->Geometry() != nullptr) {
            AppendSurface(triangles, *surface->Geometry(), iso);
        }
    }
    for (u32 index = 0u; index < tree.GetChildCount(); ++index) {
        const CPlugTree *child = tree.GetChild(index);
        if (child != nullptr) {
            AppendTree(triangles, *child, iso);
        }
    }
}

bool BuildStaticCollisionTriangles(
        const StaticSceneModelCollection &models,
        std::vector<ReplayStaticCollisionTriangle> &triangles) {
    try {
        for (const StaticSceneModel &model : models.Models()) {
            if (model.Purpose() ==
                    StaticScenePurpose::DedicatedInitialCollision) {
                continue;
            }
            CPlugSolid *solid = model.Prototype().SourceSolid();
            CPlugTree *tree = solid != nullptr ? solid->CollisionTree() : nullptr;
            if (tree != nullptr) {
                AppendTree(triangles, *tree, model.WorldIso());
            }
        }
    } catch (const std::bad_alloc &) {
        triangles.clear();
        return false;
    }
    return true;
}

namespace sandbox = forevervalidator::experimental;

sandbox::PhysicsSandboxScenePurpose PublicPurpose(
        StaticScenePurpose purpose) {
    switch (purpose) {
    case StaticScenePurpose::PlacedBlock:
        return sandbox::PhysicsSandboxScenePurpose::PlacedBlock;
    case StaticScenePurpose::SubMobil:
        return sandbox::PhysicsSandboxScenePurpose::SubMobil;
    case StaticScenePurpose::Clip:
        return sandbox::PhysicsSandboxScenePurpose::Clip;
    case StaticScenePurpose::Helper:
        return sandbox::PhysicsSandboxScenePurpose::Helper;
    case StaticScenePurpose::CheckpointTrigger:
        return sandbox::PhysicsSandboxScenePurpose::CheckpointTrigger;
    case StaticScenePurpose::DedicatedInitialCollision:
        return sandbox::PhysicsSandboxScenePurpose::
                DedicatedInitialCollision;
    case StaticScenePurpose::Pylon:
        return sandbox::PhysicsSandboxScenePurpose::Pylon;
    case StaticScenePurpose::Decoration:
        return sandbox::PhysicsSandboxScenePurpose::Decoration;
    case StaticScenePurpose::Terrain:
        return sandbox::PhysicsSandboxScenePurpose::Terrain;
    case StaticScenePurpose::Generated:
        return sandbox::PhysicsSandboxScenePurpose::Generated;
    case StaticScenePurpose::Environment:
    default:
        return sandbox::PhysicsSandboxScenePurpose::Environment;
    }
}

sandbox::PhysicsSandboxRenderProvenance PublicProvenance(
        const StaticSceneProvenance &source) {
    sandbox::PhysicsSandboxRenderProvenance result;
    result.blockName = source.blockName;
    result.collection = source.collection;
    result.descriptorPath = source.descriptorPath;
    result.sceneObjectId = source.sceneObjectId;
    result.placementIdentity = source.placementIdentity;
    result.blockInstanceId = source.blockInstanceId;
    result.variant = source.variant;
    result.componentIndex = source.componentIndex;
    result.authored = source.authored;
    return result;
}

sandbox::PhysicsSandboxTransform PublicTransform(const GmIso4 &source) {
    return {
            {source.rotation.basisX.x,
             source.rotation.basisX.y,
             source.rotation.basisX.z},
            {source.rotation.basisY.x,
             source.rotation.basisY.y,
             source.rotation.basisY.z},
            {source.rotation.basisZ.x,
             source.rotation.basisZ.y,
             source.rotation.basisZ.z},
            {source.translation.x,
             source.translation.y,
             source.translation.z}};
}

std::string PreferredPath(const std::string &selected,
                          const std::string &plain) {
    return !selected.empty() ? selected : plain;
}

class StaticRenderSceneBuilder {
public:
    sandbox::PhysicsSandboxRenderSceneHandle Build(
            const StaticSceneModelCollection &models) {
        scene_ = std::make_shared<sandbox::PhysicsSandboxRenderScene>();
        for (const StaticSceneModel &model : models.Models()) {
            if (model.Purpose() ==
                    StaticScenePurpose::DedicatedInitialCollision) {
                AddDiagnostic(
                        sandbox::PhysicsSandboxRenderDiagnosticCode::
                                CollisionOnlyObjectSkipped,
                        "collision-only object omitted from visual scene",
                        model.Provenance());
                continue;
            }
            CPlugSolid *solid = model.Prototype().SourceSolid();
            CPlugTree *root =
                    solid != nullptr ? solid->CollisionTree() : nullptr;
            if (root == nullptr) {
                AddDiagnostic(
                        sandbox::PhysicsSandboxRenderDiagnosticCode::
                                UnsupportedVisual,
                        "scene model has no visual tree",
                        model.Provenance());
                continue;
            }
            AppendTree(model, *root, model.WorldIso(), nullptr, true, 0u,
                       0.0f);
        }
        return scene_;
    }

private:
    void AddDiagnostic(
            sandbox::PhysicsSandboxRenderDiagnosticCode code,
            std::string message,
            const StaticSceneProvenance &provenance) {
        scene_->diagnostics.push_back(
                {code, std::move(message), PublicProvenance(provenance)});
    }

    std::uint32_t MaterialIndex(const CPlugMaterial *material) {
        const auto found = materialIndices_.find(material);
        if (found != materialIndices_.end()) {
            return found->second;
        }
        sandbox::PhysicsSandboxRenderMaterial output;
        output.id = scene_->materials.size() + 1u;
        if (material != nullptr) {
            const MaterialRenderDefinition &definition =
                    material->ReplayRenderDefinition();
            output.sourcePath = PreferredPath(
                    definition.MaterialSelectedPath(),
                    definition.MaterialPlainPath());
            output.modelPath = PreferredPath(
                    definition.MaterialModelSelectedPath(),
                    definition.MaterialModelPlainPath());
            output.shaderPath = PreferredPath(
                    definition.ShaderSelectedPath(),
                    definition.ShaderPlainPath());
            output.shaderFlags = definition.ShaderFlags();
            output.surfaceMaterialId =
                    static_cast<std::uint8_t>(
                            material->SurfaceMaterialId());
            output.water = definition.HasBitmapRenderWater();
            for (const MaterialRenderBitmapDefinition &bitmap :
                 definition.Bitmaps()) {
                output.bitmaps.push_back({
                        bitmap.samplerName,
                        PreferredPath(bitmap.selectedPath,
                                      bitmap.plainPath),
                        bitmap.bitmapClassId,
                        bitmap.renderClassId});
                output.cubeMap = output.cubeMap ||
                        bitmap.renderClassId ==
                                TMNF_CLASS_CPlugBitmapRenderCubeMap;
                output.renderTarget = output.renderTarget ||
                        bitmap.renderClassId ==
                                TMNF_CLASS_CPlugBitmapRenderScene3d;
            }
        }
        const std::uint32_t index =
                static_cast<std::uint32_t>(scene_->materials.size());
        scene_->materials.push_back(std::move(output));
        materialIndices_.emplace(material, index);
        return index;
    }

    std::optional<std::uint32_t> MeshIndex(
            CPlugVisual &visual,
            const StaticSceneProvenance &provenance) {
        const auto found = meshIndices_.find(&visual);
        if (found != meshIndices_.end()) {
            return found->second;
        }
        const unsigned long vertexCount = visual.GetTotalVertexCount();
        std::vector<GxVertex> source =
                visual.CanonicalVertices(1, 1, 1);
        if (vertexCount == 0u || source.size() != vertexCount) {
            AddDiagnostic(
                    sandbox::PhysicsSandboxRenderDiagnosticCode::
                            UnsupportedVisual,
                    "visual has no supported vertex stream",
                    provenance);
            return std::nullopt;
        }

        sandbox::PhysicsSandboxRenderMesh mesh;
        mesh.id = scene_->meshes.size() + 1u;
        mesh.vertices.resize(vertexCount);

        GxTexCoordSet uv0;
        GxTexCoordSet uv1;
        mesh.hasUv0 = visual.VStreamOrClassic_GetTexCoordSet(
                uv0, 0u, nullptr) != 0 && uv0.Count() == vertexCount;
        mesh.hasUv1 = visual.VStreamOrClassic_GetTexCoordSet(
                uv1, 1u, nullptr) != 0 && uv1.Count() == vertexCount;
        const auto *visual3d = dynamic_cast<const CPlugVisual3D *>(&visual);
        mesh.hasTangents = visual3d != nullptr &&
                visual3d->TangentCount() == vertexCount;
        mesh.hasNormals = visual.HasVertexNormal();
        mesh.hasVertexColors = visual.HasVertexColor();

        for (u32 index = 0u; index < vertexCount; ++index) {
            const GxVertex &input = source[index];
            auto &output = mesh.vertices[index];
            output.position = {
                    input.position.x, input.position.y, input.position.z};
            output.normal = {
                    input.normal.x, input.normal.y, input.normal.z};
            output.color = {
                    input.color[0], input.color[1],
                    input.color[2], input.color[3]};
            if (mesh.hasTangents) {
                const GmVec3 &tangent = visual3d->tangents[index];
                output.tangent = {
                        tangent.x, tangent.y, tangent.z, 1.0f};
            }
            if (mesh.hasUv0) {
                const GxTexCoord4 uv = uv0.Coordinate4At(index);
                output.uv0 = {uv.u, uv.v};
            }
            if (mesh.hasUv1) {
                const GxTexCoord4 uv = uv1.Coordinate4At(index);
                output.uv1 = {uv.u, uv.v};
            }
        }

        unsigned long indexCount = 0u;
        unsigned short *indices = nullptr;
        visual.GetVertexIndexation(indexCount, indices);
        if (indexCount == 0u) {
            indexCount = vertexCount;
            mesh.indices.reserve(indexCount);
            for (u32 index = 0u; index < indexCount; ++index) {
                mesh.indices.push_back(index);
            }
        } else if (indices == nullptr) {
            AddDiagnostic(
                    sandbox::PhysicsSandboxRenderDiagnosticCode::
                            InvalidTopology,
                    "visual index stream has no backing storage",
                    provenance);
            return std::nullopt;
        } else {
            mesh.indices.reserve(indexCount);
            for (u32 index = 0u; index < indexCount; ++index) {
                mesh.indices.push_back(indices[index]);
            }
        }
        bool topologyValid = mesh.indices.size() % 3u == 0u;
        for (std::uint32_t index : mesh.indices) {
            topologyValid = topologyValid && index < vertexCount;
        }
        if (!topologyValid) {
            AddDiagnostic(
                    sandbox::PhysicsSandboxRenderDiagnosticCode::
                            InvalidTopology,
                    "visual index stream is not a valid triangle list",
                    provenance);
            return std::nullopt;
        }

        const GmBoxAligned &box = visual.BoundingBox();
        mesh.boundsMin = {
                box.center.x - std::fabs(box.halfExtents.x),
                box.center.y - std::fabs(box.halfExtents.y),
                box.center.z - std::fabs(box.halfExtents.z)};
        mesh.boundsMax = {
                box.center.x + std::fabs(box.halfExtents.x),
                box.center.y + std::fabs(box.halfExtents.y),
                box.center.z + std::fabs(box.halfExtents.z)};
        mesh.subsets.push_back(
                {0u, static_cast<std::uint32_t>(mesh.indices.size()), 0u});
        const std::uint32_t result =
                static_cast<std::uint32_t>(scene_->meshes.size());
        scene_->meshes.push_back(std::move(mesh));
        meshIndices_.emplace(&visual, result);
        return result;
    }

    void AppendTree(
            const StaticSceneModel &model,
            CPlugTree &tree,
            const GmIso4 &parentIso,
            const CPlugMaterial *inheritedMaterial,
            bool inheritedVisibility,
            std::uint32_t lodLevel,
            float lodFarDistance) {
        GmIso4 worldIso;
        tree.ComposeCollisionIso(parentIso, worldIso);
        const bool visible = inheritedVisibility && tree.IsVisible();
        const CPlugMaterial *material =
                tree.Material() != nullptr
                ? tree.Material()
                : inheritedMaterial;
        if (CPlugVisual *visual = tree.Visual()) {
            const std::optional<std::uint32_t> meshIndex =
                    MeshIndex(*visual, model.Provenance());
            if (meshIndex.has_value()) {
                sandbox::PhysicsSandboxRenderInstance instance;
                instance.id = scene_->instances.size() + 1u;
                instance.meshIndex = *meshIndex;
                instance.materialIndex = MaterialIndex(material);
                instance.worldTransform = PublicTransform(worldIso);
                instance.provenance =
                        PublicProvenance(model.Provenance());
                instance.purpose = PublicPurpose(model.Purpose());
                instance.lodLevel = lodLevel;
                instance.lodFarDistance = lodFarDistance;
                instance.visible = visible;
                instance.castsShadows = tree.IsShadowCaster();
                scene_->instances.push_back(std::move(instance));
                if (!scene_->meshes[*meshIndex].hasUv0) {
                    AddDiagnostic(
                            sandbox::PhysicsSandboxRenderDiagnosticCode::
                                    MissingUv,
                            "visual has no authored UV0 stream",
                            model.Provenance());
                }
                if (!scene_->meshes[*meshIndex].hasTangents) {
                    AddDiagnostic(
                            sandbox::PhysicsSandboxRenderDiagnosticCode::
                                    MissingTangent,
                            "visual has no authored tangent stream",
                            model.Provenance());
                }
                if (material == nullptr) {
                    AddDiagnostic(
                            sandbox::PhysicsSandboxRenderDiagnosticCode::
                                    MissingMaterial,
                            "visual has no final material assignment",
                            model.Provenance());
                }
            }
        }

        auto *mip = dynamic_cast<CPlugTreeVisualMip *>(&tree);
        for (u32 childIndex = 0u;
             childIndex < tree.GetChildCount();
             ++childIndex) {
            CPlugTree *child = tree.GetChild(childIndex);
            if (child == nullptr) {
                continue;
            }
            std::uint32_t childLod = lodLevel;
            float childFar = lodFarDistance;
            if (mip != nullptr) {
                for (u32 level = 0u; level < mip->LevelCount(); ++level) {
                    if (mip->LevelTree(level) == child) {
                        childLod = level;
                        childFar = mip->LevelFarZ(level);
                        break;
                    }
                }
            }
            AppendTree(model, *child, worldIso, material, visible,
                       childLod, childFar);
        }
    }

    std::shared_ptr<sandbox::PhysicsSandboxRenderScene> scene_;
    std::unordered_map<const CPlugVisual *, std::uint32_t> meshIndices_;
    std::unordered_map<const CPlugMaterial *, std::uint32_t>
            materialIndices_;
};

sandbox::PhysicsSandboxRenderSceneHandle BuildStaticRenderScene(
        const StaticSceneModelCollection &models) {
    try {
        return StaticRenderSceneBuilder().Build(models);
    } catch (const std::bad_alloc &) {
        return {};
    }
}

ReplayTrajectoryObservation ObserveReplayTrajectory(
        const ReplaySimulationStepExecution &execution,
        const ReplayControlTick &tick) {
    ReplayTrajectoryObservation observation;
    observation.simulatedPosition = execution.simulatedFrame.position;
    observation.writePosition = execution.writeFrame.position;
    observation.finishTickMs = execution.finishTickMs;
    if (!tick.comparisonTarget.has_value()) {
        return observation;
    }

    ReplayTrajectoryDeviation comparison;
    comparison.targetPosition = *tick.comparisonTarget;
    comparison.delta = {
            observation.writePosition.x - comparison.targetPosition.x,
            observation.writePosition.y - comparison.targetPosition.y,
            observation.writePosition.z - comparison.targetPosition.z};
    const float horizontalDistanceSquared =
            comparison.delta.x * comparison.delta.x +
            comparison.delta.y * comparison.delta.y;
    comparison.distance = CIsqrt(
            horizontalDistanceSquared +
            comparison.delta.z * comparison.delta.z);
    observation.comparison = comparison;
    return observation;
}

}  // namespace

struct ReplaySimulationInstance {
    CTrackManiaRace race;
    std::unique_ptr<ReplaySimulationRuntime> runtime;
    std::uint32_t incrementalRespawnCount = 0u;

    void ResetRuntime() {
        runtime.reset();
        incrementalRespawnCount = 0u;
    }
};

struct ReplaySimulationSession::Impl {
    ReplayMapScene mapScene;
    ReplaySimulationInstance instance;
    std::vector<ReplayStaticCollisionTriangle> staticCollisionTriangles;
    sandbox::PhysicsSandboxRenderSceneHandle staticRenderScene;

    void ResetRuntime() { instance.ResetRuntime(); }
};

ReplaySimulationSession::ReplaySimulationSession()
    : impl(std::make_unique<Impl>()) {}

ReplaySimulationSession::~ReplaySimulationSession() = default;

void ReplaySimulationSession::Reset() {
    impl->ResetRuntime();
    impl->mapScene.Reset(impl->instance.race);
    impl->staticCollisionTriangles.clear();
    impl->staticRenderScene.reset();
}

bool ReplaySimulationSession::PreloadChallenge(
        CGameCtnChallengeConstruction &construction) {
    return impl->mapScene.PreloadChallenge(construction) ==
           ReplayMapSceneResult::Ready;
}

bool ReplaySimulationSession::InstallStaticScene(
        StaticSceneModelCollection models) {
    std::vector<ReplayStaticCollisionTriangle> triangles;
    sandbox::PhysicsSandboxRenderSceneHandle renderScene =
            BuildStaticRenderScene(models);
    if (!renderScene ||
        !BuildStaticCollisionTriangles(models, triangles) ||
        impl->mapScene.InstallModels(std::move(models)) !=
                ReplayMapSceneResult::Ready) {
        return false;
    }
    impl->staticCollisionTriangles = std::move(triangles);
    impl->staticRenderScene = std::move(renderScene);
    return true;
}

void ReplaySimulationSession::ActivateStaticScene() {
    impl->mapScene.Activate();
}

void ReplaySimulationSession::ConfigureReplayRace(
        EChallengePlayMode playMode,
        bool isLapRace,
        std::uint32_t lapCount) {
    impl->instance.race.SetReplayChallengePlayMode(playMode);
    impl->instance.race.InitNbLapsAndCheckpoints(
            isLapRace ? lapCount : 1u);
}

const std::vector<ReplayStaticCollisionTriangle> &
ReplaySimulationSession::StaticCollisionTriangles() const noexcept {
    return impl->staticCollisionTriangles;
}

sandbox::PhysicsSandboxRenderSceneHandle
ReplaySimulationSession::StaticRenderScene() const noexcept {
    return impl->staticRenderScene;
}

ReplaySimulationTimelineResult ReplaySimulationSession::SimulateTimeline(
        const ReplaySimulationDefinition &simulationDefinition,
        const std::vector<ReplayControlTick> &controlTicks,
        std::uint32_t validationSeed) {
    ReplaySimulationTimelineResult result;
    if (controlTicks.empty()) {
        return result;
    }

    if (!tmnf::simulation::DeterministicExecutionScope::IsActive()) {
        result.result =
                ReplaySimulationRunResult::DeterministicExecutionUnavailable;
        return result;
    }
    impl->ResetRuntime();

    const ReplayMapSceneResult readyResult =
            impl->mapScene.EnsureReady(impl->instance.race);
    if (readyResult != ReplayMapSceneResult::Ready) {
        result.result = MapReplaySceneResult(readyResult);
        return result;
    }
    GmIso4 startLocation;
    if (!impl->mapScene.FirstStartLineSpawnLocation(startLocation)) {
        result.result = ReplaySimulationRunResult::MapStartUnavailable;
        return result;
    }

    impl->instance.runtime = std::make_unique<ReplaySimulationRuntime>(
            impl->instance.race);
    result.result = impl->instance.runtime->Start(
            simulationDefinition,
            impl->mapScene,
            startLocation,
            controlTicks.front(),
            validationSeed);
    if (result.result != ReplaySimulationRunResult::Success) {
        return result;
    }

    for (const ReplayControlTick &tick : controlTicks) {
        const ReplaySimulationStepExecution execution =
                impl->instance.runtime->Step(tick);
        if (execution.result != ReplaySimulationRunResult::Success) {
            result.result = execution.result;
            return result;
        }
        result.executedRespawnCount +=
                execution.respawnExecutedCount;

        if (tick.observe) {
            try {
                result.observations.push_back(
                        ObserveReplayTrajectory(execution, tick));
            } catch (const std::bad_alloc &) {
                result.result =
                        ReplaySimulationRunResult::ObservationAllocationFailed;
                return result;
            }
        }
    }
    result.finishTimeMs = impl->instance.runtime->FinishTimeMs();
    result.stuntsScore = impl->instance.runtime->StuntsScore();
    result.raceCompleted = result.finishTimeMs.has_value();
    result.result = ReplaySimulationRunResult::Success;
    return result;
}

ReplaySimulationRunResult ReplaySimulationSession::StartIncremental(
        const ReplaySimulationDefinition &simulationDefinition,
        const ReplayControlTick &firstTick,
        std::uint32_t validationSeed) {
    if (!tmnf::simulation::DeterministicExecutionScope::IsActive()) {
        return ReplaySimulationRunResult::DeterministicExecutionUnavailable;
    }
    impl->ResetRuntime();
    const ReplayMapSceneResult readyResult =
            impl->mapScene.EnsureReady(impl->instance.race);
    if (readyResult != ReplayMapSceneResult::Ready) {
        return MapReplaySceneResult(readyResult);
    }
    GmIso4 startLocation;
    if (!impl->mapScene.FirstStartLineSpawnLocation(startLocation)) {
        return ReplaySimulationRunResult::MapStartUnavailable;
    }
    impl->instance.runtime = std::make_unique<ReplaySimulationRuntime>(
            impl->instance.race);
    return impl->instance.runtime->Start(
            simulationDefinition,
            impl->mapScene,
            startLocation,
            firstTick,
            validationSeed);
}

ReplaySimulationTimelineResult ReplaySimulationSession::AdvanceIncremental(
        const std::vector<ReplayControlTick> &controlTicks,
        std::size_t begin,
        std::size_t count) {
    ReplaySimulationTimelineResult result;
    if (!impl->instance.runtime || begin > controlTicks.size() ||
        count > controlTicks.size() - begin) {
        return result;
    }
    for (std::size_t index = begin; index < begin + count; ++index) {
        const ReplayControlTick &tick = controlTicks[index];
        const ReplaySimulationStepExecution execution =
                impl->instance.runtime->Step(tick);
        if (execution.result != ReplaySimulationRunResult::Success) {
            result.result = execution.result;
            return result;
        }
        impl->instance.incrementalRespawnCount +=
                execution.respawnExecutedCount;
    }
    result.finishTimeMs = impl->instance.runtime->FinishTimeMs();
    result.stuntsScore = impl->instance.runtime->StuntsScore();
    result.raceCompleted = result.finishTimeMs.has_value();
    result.executedRespawnCount = impl->instance.incrementalRespawnCount;
    result.result = ReplaySimulationRunResult::Success;
    return result;
}

std::optional<ReplaySimulationStateView>
ReplaySimulationSession::CurrentState() const {
    if (!impl->instance.runtime) {
        return std::nullopt;
    }
    ReplaySimulationStateView result;
    result.frame = impl->instance.runtime->CurrentFrame();
    result.controls = impl->instance.runtime->CurrentControls();
    result.race = impl->instance.runtime->RaceProgress();
    result.finishTimeMs = impl->instance.runtime->FinishTimeMs();
    result.stuntsScore = impl->instance.runtime->StuntsScore();
    result.respawnCount = impl->instance.incrementalRespawnCount;
    return result;
}

std::optional<std::uint32_t>
ReplaySimulationSession::ApplyReplayStuntTimePenalty(
        std::uint32_t overtimeMs) {
    if (!impl->instance.runtime) {
        return std::nullopt;
    }
    return impl->instance.runtime->ApplyReplayStuntTimePenalty(overtimeMs);
}

std::shared_ptr<const ReplaySimulationInstanceClone>
ReplaySimulationSession::CaptureRuntimeClone() const {
    if (!impl->instance.runtime ||
        impl->instance.runtime->CurrentPhase() !=
                                  ReplaySimulationRuntime::Phase::Idle) {
        return {};
    }
    std::optional<ReplaySimulationRuntime::RuntimeClone> runtime =
            impl->instance.runtime->CaptureRuntimeClone();
    if (!runtime.has_value()) {
        return {};
    }
    auto clone = std::make_shared<ReplaySimulationInstanceClone>();
    clone->race = impl->instance.race.CaptureRuntimeClone();
    clone->runtime = std::move(*runtime);
    clone->incrementalRespawnCount =
            impl->instance.incrementalRespawnCount;
    return clone;
}

bool ReplaySimulationSession::PrepareRuntimeCloneRestore(
        const ReplaySimulationInstanceClone &clone) {
    return impl->instance.runtime &&
           impl->instance.race.PrepareRuntimeCloneRestore(clone.race) &&
           impl->instance.runtime->PrepareRuntimeCloneRestore(clone.runtime);
}

void ReplaySimulationSession::RestoreRuntimeClone(
        ReplaySimulationInstanceClone clone) noexcept {
    impl->instance.race.RestoreRuntimeClone(std::move(clone.race));
    impl->instance.runtime->RestoreRuntimeClone(std::move(clone.runtime));
    impl->instance.incrementalRespawnCount = clone.incrementalRespawnCount;
}
