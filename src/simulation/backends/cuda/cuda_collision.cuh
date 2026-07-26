#ifndef FOREVERVALIDATOR_CUDA_COLLISION_CUH
#define FOREVERVALIDATOR_CUDA_COLLISION_CUH

#include <cstdint>

#include "simulation/backends/cuda/cuda_exact_math.cuh"
#include "simulation/backends/cuda/cuda_collision_layout.h"
#include "simulation/backends/cuda/cuda_scene_storage.h"
#include "simulation/backends/cuda/cuda_state_layout.h"
#include "simulation/backends/cuda/cuda_static_configuration.h"
#include "simulation/backends/cuda/cuda_tuning.cuh"

namespace forevervalidator::simulation::cuda::collision {

namespace detail {

constexpr float DirectionEpsilonSquared = 1.0e-10f;
constexpr float CollisionDistance = 1.0e-5f;
constexpr float SphereNormalAlignment = 0.8660254f;

__device__ inline CudaCollision &CollisionAt(
        CudaCollisionScratch &scratch,
        std::uint32_t index) {
    return scratch.collisions[index];
}

__device__ inline const CudaCollision &CollisionAt(
        const CudaCollisionScratch &scratch,
        std::uint32_t index) {
    return scratch.collisions[index];
}

__device__ inline CudaCollision &CollisionAt(
        CudaCollisionSearchScratch &scratch,
        std::uint32_t index) {
    return scratch.collisionStorage[
            static_cast<std::uint64_t>(index) * scratch.stride +
            scratch.slot];
}

__device__ inline const CudaCollision &CollisionAt(
        const CudaCollisionSearchScratch &scratch,
        std::uint32_t index) {
    return scratch.collisionStorage[
            static_cast<std::uint64_t>(index) * scratch.stride +
            scratch.slot];
}

__device__ inline CudaCollision &ShapeCollisionAt(
        CudaCollisionScratch &scratch,
        std::uint32_t index) {
    return scratch.shapeCollisions[index];
}

__device__ inline const CudaCollision &ShapeCollisionAt(
        const CudaCollisionScratch &scratch,
        std::uint32_t index) {
    return scratch.shapeCollisions[index];
}

__device__ inline CudaCollision &ShapeCollisionAt(
        CudaCollisionSearchScratch &scratch,
        std::uint32_t index) {
    return scratch.shapeCollisionStorage[
            static_cast<std::uint64_t>(index) * scratch.stride +
            scratch.slot];
}

__device__ inline const CudaCollision &ShapeCollisionAt(
        const CudaCollisionSearchScratch &scratch,
        std::uint32_t index) {
    return scratch.shapeCollisionStorage[
            static_cast<std::uint64_t>(index) * scratch.stride +
            scratch.slot];
}

template <bool TrackDiagnostics, typename Scratch>
__device__ inline void Clear(Scratch &scratch) {
    scratch.collisionCount = 0u;
    scratch.shapeCollisionCount = 0u;
    if constexpr (TrackDiagnostics) {
        scratch.accelerationCellVisits = 0u;
        scratch.accelerationSurfaceVisits = 0u;
        scratch.meshCellVisits = 0u;
        scratch.meshCellIntersections = 0u;
        scratch.meshTriangleCells = 0u;
        scratch.triangleTests = 0u;
        scratch.triangleHits = 0u;
        scratch.firstVisitedShape = UINT32_MAX;
        scratch.firstVisitedSurface = UINT32_MAX;
    }
    scratch.overflow = false;
}

template <typename Scratch>
__device__ inline CudaCollision *AddShape(
        Scratch &scratch) {
    if (scratch.shapeCollisionCount >=
        ShapeCollisionCapacity) {
        scratch.overflow = true;
        return nullptr;
    }
    CudaCollision *result = &ShapeCollisionAt(
            scratch, scratch.shapeCollisionCount++);
    *result = {};
    return result;
}

template <typename Scratch>
__device__ inline void AddMain(
        Scratch &scratch,
        const CudaCollision &value) {
    if (scratch.collisionCount >= CollisionCapacity) {
        scratch.overflow = true;
        return;
    }
    CollisionAt(scratch, scratch.collisionCount++) = value;
}

template<typename T>
__device__ inline const T *SceneSection(
        const CudaPackedSceneHeader *scene,
        const CudaSceneSection &section) {
    return reinterpret_cast<const T *>(
            reinterpret_cast<const std::byte *>(scene) +
            section.offset);
}

__device__ inline float Dot(
        const GmVec3 &left, const GmVec3 &right) {
    const float xy = left.x * right.x + left.y * right.y;
    return xy + left.z * right.z;
}

__device__ inline GmVec3 Add(
        const GmVec3 &left, const GmVec3 &right) {
    return {
            left.x + right.x,
            left.y + right.y,
            left.z + right.z,
    };
}

__device__ inline GmVec3 Subtract(
        const GmVec3 &left, const GmVec3 &right) {
    return {
            left.x - right.x,
            left.y - right.y,
            left.z - right.z,
    };
}

__device__ inline GmVec3 Scale(
        const GmVec3 &value, float scale) {
    return {
            value.x * scale,
            value.y * scale,
            value.z * scale,
    };
}

__device__ inline GmVec3 Negate(const GmVec3 &value) {
    return {-value.x, -value.y, -value.z};
}

__device__ inline GmVec3 Cross(
        const GmVec3 &left, const GmVec3 &right) {
    return {
            left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x,
    };
}

__device__ inline GmVec3 Normalize(
        const GmVec3 &value, float epsilonSquared) {
    GmVec3 result = value;
    const float lengthSquared = Dot(result, result);
    if (epsilonSquared < lengthSquared) {
        result = Scale(
                result, 1.0f / exact::Sqrt(lengthSquared));
    }
    return result;
}

__device__ inline GmVec3 TransformDirection(
        const GmMat3 &matrix, const GmVec3 &direction) {
    const GmVec3 rowX = {
            matrix.basisX.x,
            matrix.basisY.x,
            matrix.basisZ.x,
    };
    const GmVec3 rowY = {
            matrix.basisX.y,
            matrix.basisY.y,
            matrix.basisZ.y,
    };
    const GmVec3 rowZ = {
            matrix.basisX.z,
            matrix.basisY.z,
            matrix.basisZ.z,
    };
    return {
            Dot(rowX, direction),
            Dot(rowY, direction),
            Dot(rowZ, direction),
    };
}

__device__ inline GmVec3 TransformPoint(
        const GmIso4 &transform, const GmVec3 &point) {
    return Add(
            TransformDirection(transform.rotation, point),
            transform.translation);
}

__device__ inline GmMat3 Compose(
        const GmMat3 &first, const GmMat3 &second) {
    return {
            TransformDirection(second, first.basisX),
            TransformDirection(second, first.basisY),
            TransformDirection(second, first.basisZ),
    };
}

__device__ inline GmMat3 Transpose(const GmMat3 &matrix) {
    return {
            {matrix.basisX.x, matrix.basisY.x,
             matrix.basisZ.x},
            {matrix.basisX.y, matrix.basisY.y,
             matrix.basisZ.y},
            {matrix.basisX.z, matrix.basisY.z,
             matrix.basisZ.z},
    };
}

__device__ inline GmIso4 Inverse(const GmIso4 &transform) {
    const GmMat3 inverseRotation =
            Transpose(transform.rotation);
    return {
            inverseRotation,
            TransformDirection(
                    inverseRotation,
                    Negate(transform.translation)),
    };
}

__device__ inline GmIso4 Compose(
        const GmIso4 &first, const GmIso4 &second) {
    const GmIso4 left = first;
    const GmIso4 right = second;
    return {
            Compose(left.rotation, right.rotation),
            TransformPoint(right, left.translation),
    };
}

__device__ inline GmIso4 MultInverse(
        const GmIso4 &transform, const GmIso4 &right) {
    return Compose(transform, Inverse(right));
}

__device__ inline GmIso4 DiagonalTransform(
        const GmVec3 &scale, const GmVec3 &translation) {
    return {
            {{scale.x, 0.0f, 0.0f},
             {0.0f, scale.y, 0.0f},
             {0.0f, 0.0f, scale.z}},
            translation,
    };
}

__device__ inline void ScaleRows(
        GmIso4 &transform, const GmVec3 &scale) {
    transform.rotation.basisX.x =
            scale.x * transform.rotation.basisX.x;
    transform.rotation.basisY.x =
            transform.rotation.basisY.x * scale.x;
    transform.rotation.basisZ.x =
            scale.x * transform.rotation.basisZ.x;
    transform.translation.x *= scale.x;
    transform.rotation.basisX.y =
            scale.y * transform.rotation.basisX.y;
    transform.rotation.basisY.y =
            transform.rotation.basisY.y * scale.y;
    transform.rotation.basisZ.y =
            scale.y * transform.rotation.basisZ.y;
    transform.translation.y *= scale.y;
    transform.rotation.basisX.z =
            scale.z * transform.rotation.basisX.z;
    transform.rotation.basisY.z =
            transform.rotation.basisY.z * scale.z;
    transform.rotation.basisZ.z =
            scale.z * transform.rotation.basisZ.z;
    transform.translation.z *= scale.z;
}

__device__ inline GmBoxAligned TransformBox(
        const GmBoxAligned &box, const GmIso4 &transform) {
    GmMat3 absoluteRotation;
    absoluteRotation.basisX = {
            fabsf(transform.rotation.basisX.x),
            fabsf(transform.rotation.basisX.y),
            fabsf(transform.rotation.basisX.z),
    };
    absoluteRotation.basisY = {
            fabsf(transform.rotation.basisY.x),
            fabsf(transform.rotation.basisY.y),
            fabsf(transform.rotation.basisY.z),
    };
    absoluteRotation.basisZ = {
            fabsf(transform.rotation.basisZ.x),
            fabsf(transform.rotation.basisZ.y),
            fabsf(transform.rotation.basisZ.z),
    };
    return {
            TransformPoint(transform, box.center),
            TransformDirection(
                    absoluteRotation, box.halfExtents),
    };
}

__device__ inline bool BoundsIntersect(
        const GmBoxAligned &query,
        const GmBoxAligned &candidate) {
    if (candidate.halfExtents.z + query.halfExtents.z <
        fabsf(candidate.center.z - query.center.z)) {
        return false;
    }
    if (candidate.halfExtents.y + query.halfExtents.y <
        fabsf(candidate.center.y - query.center.y)) {
        return false;
    }
    return !(candidate.halfExtents.x + query.halfExtents.x <
             fabsf(candidate.center.x - query.center.x));
}

__device__ inline std::uint16_t LocalMaterialIndex(
        GmLocalMaterialIndex value) {
    std::uint16_t result = 0u;
    __builtin_memcpy(&result, &value, sizeof(result));
    return result;
}

__device__ inline std::uint32_t SurfaceMaterial(
        const CudaPackedSceneHeader *scene,
        const CudaSceneSurface &surface,
        GmLocalMaterialIndex local) {
    const std::uint32_t *materials =
            SceneSection<std::uint32_t>(
                    scene, scene->materials);
    const std::uint32_t index = LocalMaterialIndex(local);
    return index < surface.materialCount
            ? materials[surface.firstMaterial + index]
            : static_cast<std::uint32_t>(
                      EPlugSurfaceMaterialId_Concrete);
}

__device__ inline GmVec3 TransformEllipsoidNormal(
        const GmVec3 &normal, const GmMat3 &rotation) {
    return {
            (rotation.basisX.x * normal.x +
             rotation.basisY.x * normal.y) +
                    rotation.basisZ.x * normal.z,
            (rotation.basisX.y * normal.x +
             rotation.basisY.y * normal.y) +
                    rotation.basisZ.y * normal.z,
            (rotation.basisX.z * normal.x +
             rotation.basisY.z * normal.y) +
                    rotation.basisZ.z * normal.z,
    };
}

template <typename Scratch>
struct UnitSphereTriangleQuery {
    Scratch &scratch;
    GmVec3 center{};
    float radius = 1.0f;
    std::uint32_t materialA = 0u;
    GmVec3 triangleNormal{};
    std::uint32_t materialB = 0u;
    std::uint32_t movingShapeIndex = UINT32_MAX;
    std::uint32_t staticSurfaceIndex = UINT32_MAX;
    std::uint32_t staticActorIndex = UINT32_MAX;

    __device__ CudaCollision *AddCollision(void) {
        CudaCollision *collision = AddShape(scratch);
        if (collision == nullptr) return nullptr;
        collision->materialA = materialA;
        collision->materialB = materialB;
        collision->movingShapeIndex = movingShapeIndex;
        collision->staticSurfaceIndex = staticSurfaceIndex;
        collision->staticActorIndex = staticActorIndex;
        return collision;
    }

    __device__ int EmitFeature(
            const GmVec3 &point,
            float minimumDistanceSquared,
            bool requireContainment) {
        const GmVec3 toCenter = Subtract(center, point);
        const float distanceSquared = Dot(toCenter, toCenter);
        if ((requireContainment &&
             radius * radius < distanceSquared) ||
            !(minimumDistanceSquared < distanceSquared)) {
            return 0;
        }
        const float distance = exact::Sqrt(distanceSquared);
        const float inverse = 1.0f / distance;
        const GmVec3 normal = Scale(toCenter, inverse);
        const GmVec3 penetration = Scale(
                toCenter, (distance - radius) * inverse);
        CudaCollision *collision = AddCollision();
        if (collision == nullptr) return 0;
        collision->impulseNormal = normal;
        collision->separation = Scale(
                triangleNormal,
                Dot(penetration, triangleNormal));
        collision->contactPoint = point;
        collision->extraNegated = triangleNormal;
        return 1;
    }

    __device__ int EmitEndpointB(
            const GmVec3 &point, float minimumDistance) {
        const GmVec3 toCenter = Subtract(center, point);
        const float distanceSquared = Dot(toCenter, toCenter);
        const float distance = exact::Sqrt(distanceSquared);
        if (radius * radius < distance ||
            !(minimumDistance < distance)) {
            return 0;
        }
        const float endpointDistance = exact::Sqrt(distance);
        const float inverse = 1.0f / endpointDistance;
        const GmVec3 normal = Scale(toCenter, inverse);
        const GmVec3 penetration = Scale(
                toCenter,
                (endpointDistance - radius) * inverse);
        CudaCollision *collision = AddCollision();
        if (collision == nullptr) return 0;
        collision->impulseNormal = normal;
        collision->separation = Scale(
                triangleNormal,
                Dot(penetration, triangleNormal));
        collision->contactPoint = point;
        collision->extraNegated = triangleNormal;
        return 1;
    }

    __device__ int Collide(const GmVec3 vertices[3]) {
        const float planeDistance = Dot(
                Subtract(center, vertices[0]), triangleNormal);
        if (radius < planeDistance || planeDistance < 0.0f) {
            return 0;
        }
        const float edgeReach = exact::Sqrt(
                radius * radius -
                planeDistance * planeDistance);
        const GmVec3 projected = Add(
                center,
                Scale(triangleNormal, -planeDistance));
        for (std::uint32_t edge = 0u; edge < 3u; ++edge) {
            const std::uint32_t next =
                    edge == 2u ? 0u : edge + 1u;
            const GmVec3 start = vertices[edge];
            const GmVec3 end = vertices[next];
            const GmVec3 direction = Normalize(
                    Subtract(end, start),
                    DirectionEpsilonSquared);
            const GmVec3 edgeNormal =
                    Cross(direction, triangleNormal);
            const float edgeDistance = Dot(
                    Subtract(projected, start), edgeNormal);
            if (edgeReach < edgeDistance) return 0;
            if (edgeDistance > 0.0f) {
                const float fromStart = Dot(
                        Subtract(projected, start), direction);
                if (fromStart < 0.0f) {
                    return EmitFeature(
                            start, DirectionEpsilonSquared, true);
                }
                const float fromEnd = Dot(
                        Subtract(projected, end), direction);
                if (!(0.0f < fromEnd)) {
                    return EmitFeature(
                            Add(projected,
                                Scale(edgeNormal, -edgeDistance)),
                            CollisionDistance, false);
                }
                return EmitEndpointB(
                        end, DirectionEpsilonSquared);
            }
        }
        if (planeDistance > 0.0f) {
            CudaCollision *collision = AddCollision();
            if (collision == nullptr) return 0;
            collision->impulseNormal = triangleNormal;
            collision->separation = Scale(
                    triangleNormal, planeDistance - radius);
            collision->contactPoint = projected;
            collision->sphereMergePrimary = true;
            collision->extraNegated = triangleNormal;
            return 1;
        }
        return 0;
    }
};

template <typename Scratch>
__device__ inline void TransformNewCollisions(
        Scratch &scratch,
        std::uint32_t firstNew,
        const GmIso4 &contactToWorld,
        const GmIso4 &normalToWorld) {
    for (std::uint32_t index = firstNew;
         index < scratch.shapeCollisionCount; ++index) {
        CudaCollision &collision =
                ShapeCollisionAt(scratch, index);
        collision.contactPoint = TransformPoint(
                contactToWorld, collision.contactPoint);
        collision.impulseNormal = Normalize(
                TransformEllipsoidNormal(
                        collision.impulseNormal,
                        normalToWorld.rotation),
                DirectionEpsilonSquared);
        collision.separation = TransformDirection(
                contactToWorld.rotation, collision.separation);
    }
}

template <bool TrackDiagnostics, typename Scratch>
__device__ inline int EllipsoidMesh(
        const CudaPackedSceneHeader *scene,
        const CudaPackedStaticConfigurationHeader *configuration,
        const CudaSceneSurface &surface,
        std::uint32_t surfaceIndex,
        std::uint32_t actorIndex,
        const CudaVehicleCollisionShape &shape,
        std::uint32_t shapeIndex,
        const GmIso4 &shapeWorld,
        Scratch &scratch) {
    const GmVec3 radii = shape.localBounds.halfExtents;
    const GmVec3 inverseRadii = {
            1.0f / radii.x,
            1.0f / radii.y,
            1.0f / radii.z,
    };
    const GmIso4 ellipsoidToMesh =
            Compose(shapeWorld, surface.worldToLocal);
    const GmBoxAligned ellipsoidBox = TransformBox(
            {{0.0f, 0.0f, 0.0f}, radii},
            ellipsoidToMesh);
    if constexpr (TrackDiagnostics) {
        if (scratch.firstVisitedSurface == UINT32_MAX) {
            scratch.firstVisitedShape = shapeIndex;
            scratch.firstVisitedSurface = surfaceIndex;
            scratch.firstShapeWorld = shapeWorld;
            scratch.firstEllipsoidBox = ellipsoidBox;
            scratch.firstSurfaceWorldBounds = surface.worldBounds;
        }
    }
    const GmIso4 meshToEllipsoid = Inverse(ellipsoidToMesh);
    GmIso4 meshToUnit = meshToEllipsoid;
    ScaleRows(meshToUnit, inverseRadii);
    GmIso4 contactToWorld = DiagonalTransform(
            radii, {0.0f, 0.0f, 0.0f});
    contactToWorld =
            MultInverse(contactToWorld, meshToEllipsoid);
    contactToWorld =
            Compose(contactToWorld, surface.localToWorld);
    GmIso4 normalToWorld = DiagonalTransform(
            inverseRadii, {0.0f, 0.0f, 0.0f});
    normalToWorld =
            MultInverse(normalToWorld, meshToEllipsoid);
    normalToWorld =
            Compose(normalToWorld, surface.localToWorld);

    const GmVec3 *vertices =
            SceneSection<GmVec3>(scene, scene->vertices);
    const CudaSceneTriangle *triangles =
            SceneSection<CudaSceneTriangle>(
                    scene, scene->triangles);
    const CudaSceneOctreeCell *cells =
            SceneSection<CudaSceneOctreeCell>(
                    scene, scene->octreeCells);
    if constexpr (TrackDiagnostics) {
        if (scratch.firstVisitedSurface == surfaceIndex &&
            surface.octreeCellCount != 0u) {
            scratch.firstMeshRootBounds =
                    cells[surface.firstOctreeCell].bounds;
        }
    }
    std::uint32_t cell = 0u;
    int hit = 0;
    while (cell < surface.octreeCellCount) {
        if constexpr (TrackDiagnostics) {
            ++scratch.meshCellVisits;
        }
        const CudaSceneOctreeCell &entry =
                cells[surface.firstOctreeCell + cell];
        if (!BoundsIntersect(ellipsoidBox, entry.bounds)) {
            cell += entry.subtreeEntryCount;
            continue;
        }
        if constexpr (TrackDiagnostics) {
            ++scratch.meshCellIntersections;
        }
        ++cell;
        if (!entry.containsTriangle ||
            entry.triangleIndex >= surface.triangleCount) {
            continue;
        }
        if constexpr (TrackDiagnostics) {
            ++scratch.meshTriangleCells;
        }
        const CudaSceneTriangle &triangle =
                triangles[surface.firstTriangle +
                          entry.triangleIndex];
        if constexpr (TrackDiagnostics) {
            ++scratch.triangleTests;
        }
        const GmVec3 unitVertices[3] = {
                TransformPoint(
                        meshToUnit,
                        vertices[surface.firstVertex +
                                 triangle.vertexIndices[0]]),
                TransformPoint(
                        meshToUnit,
                        vertices[surface.firstVertex +
                                 triangle.vertexIndices[1]]),
                TransformPoint(
                        meshToUnit,
                        vertices[surface.firstVertex +
                                 triangle.vertexIndices[2]]),
        };
        const GmVec3 edge01 =
                Subtract(unitVertices[1], unitVertices[0]);
        const GmVec3 edge02 =
                Subtract(unitVertices[2], unitVertices[0]);
        const float normalX =
                edge02.z * edge01.y - edge02.y * edge01.z;
        const float normalY =
                edge01.z * edge02.x - edge02.z * edge01.x;
        const float normalZ =
                edge01.x * edge02.y - edge02.x * edge01.y;
        const float normalLengthSquared =
                (normalY * normalY + normalX * normalX) +
                normalZ * normalZ;
        if (!(normalLengthSquared >
              DirectionEpsilonSquared)) {
            continue;
        }
        const float normalLength =
                exact::Sqrt(normalLengthSquared);
        const float inverseNormalLength = 1.0f / normalLength;
        const GmVec3 triangleNormal = {
                normalX * inverseNormalLength,
                normalY * inverseNormalLength,
                inverseNormalLength * normalZ,
        };
        const std::uint32_t firstNew =
                scratch.shapeCollisionCount;
        UnitSphereTriangleQuery<Scratch> query{
                scratch,
                {},
                1.0f,
                shape.wheelRole != UINT32_MAX &&
                                configuration->tuning.contactResponse.
                                        singleMaterial <
                                        EPlugSurfaceMaterialId_Count
                        ? static_cast<std::uint32_t>(
                                  configuration->tuning.
                                          contactResponse.
                                          singleMaterial)
                        : shape.surfaceMaterial,
                triangleNormal,
                SurfaceMaterial(scene, surface,
                                triangle.material),
                shapeIndex,
                surfaceIndex,
                actorIndex,
        };
        if (query.Collide(unitVertices)) {
            if constexpr (TrackDiagnostics) {
                ++scratch.triangleHits;
            }
            TransformNewCollisions(
                    scratch, firstNew,
                    contactToWorld, normalToWorld);
            hit = 1;
        }
        if (scratch.overflow) return hit;
    }
    return hit;
}

__device__ inline bool NearlyEqual(
        float value, float reference) {
    const float tolerance = fabsf(reference) * 1.0e-5f;
    return reference - tolerance <= value &&
           value <= reference + tolerance;
}

__device__ inline bool NearlyEqual(
        const GmVec3 &left, const GmVec3 &right) {
    return NearlyEqual(left.x, right.x) &&
           NearlyEqual(left.y, right.y) &&
           NearlyEqual(left.z, right.z);
}

template <typename Scratch>
__device__ inline void MergeShapeContacts(
        Scratch &scratch) {
    const std::uint32_t firstTarget =
            scratch.collisionCount;
    for (std::uint32_t index = 0u;
         index < scratch.shapeCollisionCount; ++index) {
        if (ShapeCollisionAt(
                    scratch, index).sphereMergePrimary) {
            AddMain(scratch, ShapeCollisionAt(scratch, index));
        }
    }
    const std::uint32_t targetAfterPrimaries =
            scratch.collisionCount;
    for (std::uint32_t index = 0u;
         index < scratch.shapeCollisionCount; ++index) {
        const CudaCollision &collision =
                ShapeCollisionAt(scratch, index);
        if (collision.sphereMergePrimary) continue;
        std::uint32_t target = firstTarget;
        for (; target < targetAfterPrimaries; ++target) {
            const CudaCollision &primary =
                    CollisionAt(scratch, target);
            if (NearlyEqual(
                        collision.extraNegated,
                        primary.extraNegated) ||
                SphereNormalAlignment <
                        Dot(collision.impulseNormal,
                            primary.impulseNormal)) {
                break;
            }
        }
        if (target == targetAfterPrimaries) {
            AddMain(scratch, collision);
        }
    }
    scratch.shapeCollisionCount = 0u;
}

__device__ inline GmIso4 BodyPose(
        const CudaDynamicBodyState &body) {
    return {body.current.rotation, body.current.position};
}

__device__ inline GmIso4 ShapeBodyPose(
        const CudaVehicleCollisionShape &shape,
        const CudaPackedStaticConfigurationHeader *configuration,
        const CudaCandidatePhysicsState &candidate) {
    if (shape.wheelRole == UINT32_MAX) {
        return shape.bodyPose;
    }
    const VehicleWheelDefinition *wheelDefinitions =
            reinterpret_cast<const VehicleWheelDefinition *>(
                    &configuration->wheels.wheels);
    for (std::uint32_t wheel = 0u;
         wheel < candidate.vehicle.wheels.count &&
         wheel < VehicleWheelSetDefinition::OfficialWheelCount;
         ++wheel) {
        if (static_cast<std::uint32_t>(
                    wheelDefinitions[wheel].collisionRole) ==
            shape.wheelRole) {
            return candidate.vehicle.wheels.values[wheel].
                    currentPose;
        }
    }
    return shape.bodyPose;
}

__device__ inline int CompareForResponse(
        const CudaCollision &left,
        const CudaCollision &right) {
    const float leftValues[] = {
            left.contactPoint.x,
            left.contactPoint.y,
            left.contactPoint.z,
            left.impulseNormal.x,
            left.impulseNormal.y,
            left.impulseNormal.z,
            left.separation.x,
            left.separation.y,
            left.separation.z,
    };
    const float rightValues[] = {
            right.contactPoint.x,
            right.contactPoint.y,
            right.contactPoint.z,
            right.impulseNormal.x,
            right.impulseNormal.y,
            right.impulseNormal.z,
            right.separation.x,
            right.separation.y,
            right.separation.z,
    };
    for (std::uint32_t index = 0u; index < 9u; ++index) {
        const float leftValue = leftValues[index];
        const float rightValue = rightValues[index];
        if (!(rightValue <= leftValue)) return 1;
        if (rightValue < leftValue) return -1;
    }
    if (!left.sphereMergePrimary &&
        right.sphereMergePrimary) {
        return -1;
    }
    return 1;
}

__device__ inline void Swap(
        CudaCollision &left, CudaCollision &right) {
    const CudaCollision temporary = left;
    left = right;
    right = temporary;
}

template <typename Scratch>
__device__ inline void SortForResponse(
        Scratch &scratch) {
    constexpr std::uint32_t Cutoff = 8u;
    constexpr std::uint32_t StackSize = 30u;
    if (scratch.collisionCount < 2u) return;
    std::uint32_t lowStack[StackSize]{};
    std::uint32_t highStack[StackSize]{};
    std::uint32_t stackDepth = 0u;
    std::uint32_t low = 0u;
    std::uint32_t high = scratch.collisionCount - 1u;
    for (;;) {
        const std::uint32_t count = high - low + 1u;
        if (count <= Cutoff) {
            while (high > low) {
                std::uint32_t selected = low;
                for (std::uint32_t cursor = low + 1u;
                     cursor <= high; ++cursor) {
                    if (CompareForResponse(
                                CollisionAt(scratch, cursor),
                                CollisionAt(scratch, selected)) > 0) {
                        selected = cursor;
                    }
                }
                if (selected != high) {
                    Swap(CollisionAt(scratch, selected),
                         CollisionAt(scratch, high));
                }
                --high;
            }
        } else {
            std::uint32_t middle = low + count / 2u;
            if (CompareForResponse(
                        CollisionAt(scratch, low),
                        CollisionAt(scratch, middle)) > 0) {
                Swap(CollisionAt(scratch, low),
                     CollisionAt(scratch, middle));
            }
            if (CompareForResponse(
                        CollisionAt(scratch, low),
                        CollisionAt(scratch, high)) > 0) {
                Swap(CollisionAt(scratch, low),
                     CollisionAt(scratch, high));
            }
            if (CompareForResponse(
                        CollisionAt(scratch, middle),
                        CollisionAt(scratch, high)) > 0) {
                Swap(CollisionAt(scratch, middle),
                     CollisionAt(scratch, high));
            }
            std::uint32_t lowCursor = low;
            std::uint32_t highCursor = high;
            for (;;) {
                if (middle > lowCursor) {
                    do {
                        ++lowCursor;
                    } while (
                            lowCursor < middle &&
                            CompareForResponse(
                                    CollisionAt(scratch, lowCursor),
                                    CollisionAt(scratch, middle)) <= 0);
                }
                if (middle <= lowCursor) {
                    do {
                        ++lowCursor;
                    } while (
                            lowCursor <= high &&
                            CompareForResponse(
                                    CollisionAt(scratch, lowCursor),
                                    CollisionAt(scratch, middle)) <= 0);
                }
                do {
                    --highCursor;
                } while (
                        highCursor > middle &&
                        CompareForResponse(
                                CollisionAt(scratch, highCursor),
                                CollisionAt(scratch, middle)) > 0);
                if (highCursor < lowCursor) break;
                Swap(CollisionAt(scratch, lowCursor),
                     CollisionAt(scratch, highCursor));
                if (middle == highCursor) {
                    middle = lowCursor;
                } else if (middle == lowCursor) {
                    middle = highCursor;
                }
            }
            ++highCursor;
            if (middle < highCursor) {
                do {
                    --highCursor;
                } while (
                        highCursor > middle &&
                        CompareForResponse(
                                CollisionAt(scratch, highCursor),
                                CollisionAt(scratch, middle)) == 0);
            }
            if (middle >= highCursor) {
                do {
                    --highCursor;
                } while (
                        highCursor > low &&
                        CompareForResponse(
                                CollisionAt(scratch, highCursor),
                                CollisionAt(scratch, middle)) == 0);
            }
            const std::uint32_t leftSpan =
                    highCursor - low;
            const std::uint32_t rightSpan =
                    high - lowCursor;
            if (leftSpan >= rightSpan) {
                if (low < highCursor) {
                    if (stackDepth >= StackSize) {
                        scratch.overflow = true;
                        return;
                    }
                    lowStack[stackDepth] = low;
                    highStack[stackDepth] = highCursor;
                    ++stackDepth;
                }
                if (lowCursor < high) {
                    low = lowCursor;
                    continue;
                }
            } else {
                if (lowCursor < high) {
                    if (stackDepth >= StackSize) {
                        scratch.overflow = true;
                        return;
                    }
                    lowStack[stackDepth] = lowCursor;
                    highStack[stackDepth] = high;
                    ++stackDepth;
                }
                if (low < highCursor) {
                    high = highCursor;
                    continue;
                }
            }
        }
        if (stackDepth == 0u) return;
        --stackDepth;
        low = lowStack[stackDepth];
        high = highStack[stackDepth];
    }
}

}  // namespace detail

template <
        bool TrackDiagnostics = true,
        typename Scratch = CudaCollisionScratch>
__device__ inline Status Detect(
        const CudaPackedSceneHeader *scene,
        const CudaPackedStaticConfigurationHeader *configuration,
        const CudaCandidatePhysicsState &candidate,
        Scratch &scratch) {
    detail::Clear<TrackDiagnostics>(scratch);
    if (scene == nullptr || configuration == nullptr ||
        scene->magic != CudaPackedSceneHeader::Magic ||
        configuration->magic !=
                CudaPackedStaticConfigurationHeader::Magic) {
        return Status::InvalidScene;
    }
    const CudaSceneSurface *surfaces =
            detail::SceneSection<CudaSceneSurface>(
                    scene, scene->surfaces);
    const CudaSceneAccelerationCell *acceleration =
            detail::SceneSection<CudaSceneAccelerationCell>(
                    scene, scene->accelerationCells);
    const CudaVehicleCollisionShape *shapes =
            tuning::Section<CudaVehicleCollisionShape>(
                    configuration,
                    configuration->collisionShapes);
    const GmIso4 bodyPose = detail::BodyPose(candidate.body);

    for (std::uint32_t traversal = 0u;
         traversal < configuration->collisionShapes.count;
         ++traversal) {
        std::uint32_t shapeIndex = UINT32_MAX;
        for (std::uint32_t index = 0u;
             index < configuration->collisionShapes.count; ++index) {
            if (shapes[index].traversalOrder == traversal) {
                shapeIndex = index;
                break;
            }
        }
        if (shapeIndex == UINT32_MAX) {
            return Status::InvalidScene;
        }
        const CudaVehicleCollisionShape &shape =
                shapes[shapeIndex];
        if (shape.surfaceType != static_cast<std::uint32_t>(
                    GmSurf::EGmSurfType::Ellipsoid)) {
            return Status::UnsupportedGeometry;
        }
        const GmIso4 shapeWorld =
                detail::Compose(
                        detail::ShapeBodyPose(
                                shape, configuration, candidate),
                        bodyPose);
        const GmBoxAligned movingBounds =
                detail::TransformBox(
                        shape.localBounds, shapeWorld);
        constexpr std::uint32_t TargetGroups[] = {1u, 3u, 4u};
        for (std::uint32_t group : TargetGroups) {
            const CudaSceneAccelerationRange range =
                    scene->accelerationGroups[group - 1u];
            if (range.cellCount <= 1u) continue;
            std::uint32_t index = 0u;
            while (index < range.cellCount) {
                if constexpr (TrackDiagnostics) {
                    ++scratch.accelerationCellVisits;
                }
                const CudaSceneAccelerationCell &cell =
                        acceleration[range.firstCell + index];
                if (!detail::BoundsIntersect(
                            movingBounds, cell.bounds)) {
                    index += cell.subtreeEntryCount;
                    continue;
                }
                ++index;
                if (cell.surfaceIndex == UINT32_MAX ||
                    cell.surfaceIndex >= scene->surfaces.count) {
                    continue;
                }
                const CudaSceneSurface &surface =
                        surfaces[cell.surfaceIndex];
                if constexpr (TrackDiagnostics) {
                    ++scratch.accelerationSurfaceVisits;
                }
                if (surface.type != static_cast<std::uint32_t>(
                            GmSurf::EGmSurfType::Mesh)) {
                    return Status::UnsupportedGeometry;
                }
                detail::EllipsoidMesh<TrackDiagnostics>(
                        scene, configuration, surface,
                        cell.surfaceIndex,
                        surface.actorIndex, shape, shapeIndex,
                        shapeWorld, scratch);
                if (scratch.overflow) return Status::Overflow;
            }
        }
        detail::MergeShapeContacts(scratch);
        if (scratch.overflow) return Status::Overflow;
    }
    detail::SortForResponse(scratch);
    if (scratch.overflow) return Status::Overflow;
    return Status::Success;
}

}  // namespace forevervalidator::simulation::cuda::collision

#endif
