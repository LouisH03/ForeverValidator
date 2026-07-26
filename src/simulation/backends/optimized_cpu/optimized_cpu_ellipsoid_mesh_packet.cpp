#include "simulation/backends/optimized_cpu/optimized_cpu_ellipsoid_mesh_packet.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <typeinfo>

#include "engine/physics/collision/gm_collision_buffer.h"
#include "engine/physics/geometry/gm_surface.h"
#include "engine/physics/geometry/physics_tolerances.h"
#include "simulation/backends/optimized_cpu/optimized_cpu_static_mesh_triangle_sidecar.h"

#if (defined(__i386__) || defined(__x86_64__)) && \
        (defined(__GNUC__) || defined(__clang__))
#define FV_E031_HAS_X86_PACKET 1
#include <immintrin.h>
#else
#define FV_E031_HAS_X86_PACKET 0
#endif

namespace {

constexpr std::size_t PacketWidth =
        OptimizedCpuPreparedEllipsoidMeshPacket::Width;
// Archived TMNF Stadium meshes in the broad replay corpus peak at depth 20.
// Deeper valid meshes remain exact through the scalar fallback because the
// certified depth is rejected before any packet collision is emitted.
constexpr std::size_t PacketTraversalCapacity = 24u;

bool IsFiniteTransform(const GmIso4 &transform) noexcept {
    const float values[] = {
        transform.rotation.basisX.x,
        transform.rotation.basisX.y,
        transform.rotation.basisX.z,
        transform.rotation.basisY.x,
        transform.rotation.basisY.y,
        transform.rotation.basisY.z,
        transform.rotation.basisZ.x,
        transform.rotation.basisZ.y,
        transform.rotation.basisZ.z,
        transform.translation.x,
        transform.translation.y,
        transform.translation.z,
    };
    for (float value : values) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return true;
}

bool BuildPreparedPacket(
        const OptimizedCpuEllipsoidMeshPacketLane *lanes,
        std::size_t laneCount,
        std::uint32_t requestedMask,
        OptimizedCpuPreparedEllipsoidMeshPacket *prepared) noexcept {
    if (lanes == nullptr || prepared == nullptr || laneCount < 2u ||
        laneCount > PacketWidth) {
        return false;
    }
    const std::uint32_t laneMask =
            (1u << static_cast<unsigned int>(laneCount)) - 1u;
    requestedMask &= laneMask;
    OptimizedCpuPreparedEllipsoidMeshPacket candidate;
    candidate.laneCount = laneCount;
    candidate.preparedMask = requestedMask;
    if (requestedMask == 0u) {
        *prepared = candidate;
        return true;
    }

    for (std::size_t lane = 0u; lane < laneCount; ++lane) {
        if ((requestedMask & (1u << lane)) == 0u) {
            continue;
        }
        const LocatedGmSurf *located = lanes[lane].ellipsoid;
        if (located == nullptr || located->surf == nullptr ||
            located->iso == nullptr ||
            lanes[lane].collisionBuffer == nullptr ||
            typeid(*located->surf) != typeid(GmSurfEllipsoid)) {
            return false;
        }
        const GmSurfEllipsoid &ellipsoid =
                static_cast<const GmSurfEllipsoid &>(*located->surf);
        const GmVec3 radii = ellipsoid.radii;
        if (!(0.0f < radii.x && 0.0f < radii.y && 0.0f < radii.z) ||
            !std::isfinite(radii.x) || !std::isfinite(radii.y) ||
            !std::isfinite(radii.z)) {
            return false;
        }
        const GmIso4 ellipsoidWorld = located->enabled == 0
                ? GmIso4{
                      {{1.0f, 0.0f, 0.0f},
                       {0.0f, 1.0f, 0.0f},
                       {0.0f, 0.0f, 1.0f}},
                      {0.0f, 0.0f, 0.0f}}
                : *located->iso;
        candidate.worldXx.values[lane] =
                ellipsoidWorld.rotation.basisX.x;
        candidate.worldXy.values[lane] =
                ellipsoidWorld.rotation.basisY.x;
        candidate.worldXz.values[lane] =
                ellipsoidWorld.rotation.basisZ.x;
        candidate.worldYx.values[lane] =
                ellipsoidWorld.rotation.basisX.y;
        candidate.worldYy.values[lane] =
                ellipsoidWorld.rotation.basisY.y;
        candidate.worldYz.values[lane] =
                ellipsoidWorld.rotation.basisZ.y;
        candidate.worldZx.values[lane] =
                ellipsoidWorld.rotation.basisX.z;
        candidate.worldZy.values[lane] =
                ellipsoidWorld.rotation.basisY.z;
        candidate.worldZz.values[lane] =
                ellipsoidWorld.rotation.basisZ.z;
        candidate.worldTx.values[lane] = ellipsoidWorld.translation.x;
        candidate.worldTy.values[lane] = ellipsoidWorld.translation.y;
        candidate.worldTz.values[lane] = ellipsoidWorld.translation.z;
        candidate.radiiX.values[lane] = radii.x;
        candidate.radiiY.values[lane] = radii.y;
        candidate.radiiZ.values[lane] = radii.z;
        candidate.inverseRadiiX.values[lane] = 1.0f / radii.x;
        candidate.inverseRadiiY.values[lane] = 1.0f / radii.y;
        candidate.inverseRadiiZ.values[lane] = 1.0f / radii.z;
        candidate.materials[lane] = ellipsoid.material;
        candidate.buffers[lane] = lanes[lane].collisionBuffer;
    }
    *prepared = candidate;
    return true;
}

#if FV_E031_HAS_X86_PACKET

#if defined(__GNUC__) || defined(__clang__)
#define FV_E031_AVX2 __attribute__((target("avx2")))
#define FV_E031_INLINE inline __attribute__((always_inline, target("avx2")))
#else
#define FV_E031_AVX2
#define FV_E031_INLINE inline
#endif

struct Vec3x8 {
    __m256 x;
    __m256 y;
    __m256 z;
};

struct Mat3x8 {
    __m256 xx, xy, xz;
    __m256 yx, yy, yz;
    __m256 zx, zy, zz;
};

struct Iso3x8 {
    Mat3x8 rotation;
    Vec3x8 translation;
};

struct Boxx8 {
    Vec3x8 center;
    Vec3x8 halfExtents;
};

FV_E031_INLINE __m256 Load(const std::array<float, PacketWidth> &values) {
    return _mm256_load_ps(values.data());
}

FV_E031_INLINE __m256 MaskForBits(std::uint32_t bits) {
    return _mm256_castsi256_ps(
            _mm256_set_epi32(
                    (bits & 0x80u) != 0u ? -1 : 0,
                    (bits & 0x40u) != 0u ? -1 : 0,
                    (bits & 0x20u) != 0u ? -1 : 0,
                    (bits & 0x10u) != 0u ? -1 : 0,
                    (bits & 0x08u) != 0u ? -1 : 0,
                    (bits & 0x04u) != 0u ? -1 : 0,
                    (bits & 0x02u) != 0u ? -1 : 0,
                    (bits & 0x01u) != 0u ? -1 : 0));
}

FV_E031_INLINE std::uint32_t Bits(__m256 mask) {
    return static_cast<std::uint32_t>(_mm256_movemask_ps(mask));
}

FV_E031_INLINE __m256 And(__m256 left, __m256 right) {
    return _mm256_and_ps(left, right);
}

FV_E031_INLINE __m256 AndNot(__m256 left, __m256 right) {
    return _mm256_andnot_ps(left, right);
}

FV_E031_INLINE __m256 Or(__m256 left, __m256 right) {
    return _mm256_or_ps(left, right);
}

FV_E031_INLINE __m256 Select(__m256 mask, __m256 yes, __m256 no) {
    return _mm256_blendv_ps(no, yes, mask);
}

FV_E031_INLINE __m256 Abs(__m256 value) {
    return _mm256_andnot_ps(_mm256_set1_ps(-0.0f), value);
}

FV_E031_INLINE __m256 Dot(const Vec3x8 &left, const Vec3x8 &right) {
    const __m256 xy = _mm256_add_ps(
            _mm256_mul_ps(left.x, right.x),
            _mm256_mul_ps(left.y, right.y));
    return _mm256_add_ps(xy, _mm256_mul_ps(left.z, right.z));
}

FV_E031_INLINE Vec3x8 Add(const Vec3x8 &left, const Vec3x8 &right) {
    return {
        _mm256_add_ps(left.x, right.x),
        _mm256_add_ps(left.y, right.y),
        _mm256_add_ps(left.z, right.z),
    };
}

FV_E031_INLINE Vec3x8 Subtract(const Vec3x8 &left,
                               const Vec3x8 &right) {
    return {
        _mm256_sub_ps(left.x, right.x),
        _mm256_sub_ps(left.y, right.y),
        _mm256_sub_ps(left.z, right.z),
    };
}

FV_E031_INLINE Vec3x8 Scale(const Vec3x8 &value, __m256 scale) {
    return {
        _mm256_mul_ps(value.x, scale),
        _mm256_mul_ps(value.y, scale),
        _mm256_mul_ps(value.z, scale),
    };
}

FV_E031_INLINE Vec3x8 Negate(const Vec3x8 &value) {
    const __m256 sign = _mm256_set1_ps(-0.0f);
    return {
        _mm256_xor_ps(value.x, sign),
        _mm256_xor_ps(value.y, sign),
        _mm256_xor_ps(value.z, sign),
    };
}

FV_E031_INLINE Vec3x8 Cross(const Vec3x8 &left, const Vec3x8 &right) {
    return {
        _mm256_sub_ps(_mm256_mul_ps(left.y, right.z),
                      _mm256_mul_ps(left.z, right.y)),
        _mm256_sub_ps(_mm256_mul_ps(left.z, right.x),
                      _mm256_mul_ps(left.x, right.z)),
        _mm256_sub_ps(_mm256_mul_ps(left.x, right.y),
                      _mm256_mul_ps(left.y, right.x)),
    };
}

FV_E031_INLINE Vec3x8 Broadcast(const GmVec3 &value) {
    return {
        _mm256_set1_ps(value.x),
        _mm256_set1_ps(value.y),
        _mm256_set1_ps(value.z),
    };
}

FV_E031_INLINE Vec3x8 ZeroVector(void) {
    const __m256 zero = _mm256_setzero_ps();
    return {zero, zero, zero};
}

FV_E031_INLINE Vec3x8 TransformDirection(const Mat3x8 &matrix,
                                         const Vec3x8 &value) {
    return {
        _mm256_add_ps(
                _mm256_add_ps(_mm256_mul_ps(matrix.xx, value.x),
                              _mm256_mul_ps(matrix.xy, value.y)),
                _mm256_mul_ps(matrix.xz, value.z)),
        _mm256_add_ps(
                _mm256_add_ps(_mm256_mul_ps(matrix.yx, value.x),
                              _mm256_mul_ps(matrix.yy, value.y)),
                _mm256_mul_ps(matrix.yz, value.z)),
        _mm256_add_ps(
                _mm256_add_ps(_mm256_mul_ps(matrix.zx, value.x),
                              _mm256_mul_ps(matrix.zy, value.y)),
                _mm256_mul_ps(matrix.zz, value.z)),
    };
}

FV_E031_INLINE Vec3x8 TransformPoint(const Iso3x8 &transform,
                                     const Vec3x8 &value) {
    return Add(TransformDirection(transform.rotation, value),
               transform.translation);
}

FV_E031_INLINE Vec3x8 Normalize(const Vec3x8 &value,
                                __m256 activeMask) {
    const __m256 lengthSquared = Dot(value, value);
    const __m256 normalizeMask = And(
            activeMask,
            _mm256_cmp_ps(
                    _mm256_set1_ps(
                            PhysicsTolerance::SurfaceDirectionLengthSquared),
                    lengthSquared,
                    _CMP_LT_OQ));
    const __m256 safeLengthSquared = Select(
            normalizeMask, lengthSquared, _mm256_set1_ps(1.0f));
    const __m256 inverseLength = _mm256_div_ps(
            _mm256_set1_ps(1.0f), _mm256_sqrt_ps(safeLengthSquared));
    const Vec3x8 normalized = Scale(value, inverseLength);
    return {
        Select(normalizeMask, normalized.x, value.x),
        Select(normalizeMask, normalized.y, value.y),
        Select(normalizeMask, normalized.z, value.z),
    };
}

FV_E031_INLINE Iso3x8 LoadIso(
        const OptimizedCpuPreparedEllipsoidMeshPacket &prepared) {
    return {
        {
            Load(prepared.worldXx.values),
            Load(prepared.worldXy.values),
            Load(prepared.worldXz.values),
            Load(prepared.worldYx.values),
            Load(prepared.worldYy.values),
            Load(prepared.worldYz.values),
            Load(prepared.worldZx.values),
            Load(prepared.worldZy.values),
            Load(prepared.worldZz.values),
        },
        {
            Load(prepared.worldTx.values),
            Load(prepared.worldTy.values),
            Load(prepared.worldTz.values),
        },
    };
}

FV_E031_INLINE void Store(__m256 value,
                          std::array<float, PacketWidth> *output) {
    _mm256_store_ps(output->data(), value);
}

FV_E031_INLINE void Store(const Vec3x8 &value,
                          std::array<float, PacketWidth> *x,
                          std::array<float, PacketWidth> *y,
                          std::array<float, PacketWidth> *z) {
    Store(value.x, x);
    Store(value.y, y);
    Store(value.z, z);
}

FV_E031_INLINE Iso3x8 BroadcastIso(const GmIso4 &transform) {
    return {
        {
            _mm256_set1_ps(transform.rotation.basisX.x),
            _mm256_set1_ps(transform.rotation.basisY.x),
            _mm256_set1_ps(transform.rotation.basisZ.x),
            _mm256_set1_ps(transform.rotation.basisX.y),
            _mm256_set1_ps(transform.rotation.basisY.y),
            _mm256_set1_ps(transform.rotation.basisZ.y),
            _mm256_set1_ps(transform.rotation.basisX.z),
            _mm256_set1_ps(transform.rotation.basisY.z),
            _mm256_set1_ps(transform.rotation.basisZ.z),
        },
        {
            _mm256_set1_ps(transform.translation.x),
            _mm256_set1_ps(transform.translation.y),
            _mm256_set1_ps(transform.translation.z),
        },
    };
}

FV_E031_INLINE Iso3x8 Compose(const Iso3x8 &first,
                              const Iso3x8 &second) {
    const Vec3x8 basisX = TransformDirection(
            second.rotation,
            {first.rotation.xx, first.rotation.yx, first.rotation.zx});
    const Vec3x8 basisY = TransformDirection(
            second.rotation,
            {first.rotation.xy, first.rotation.yy, first.rotation.zy});
    const Vec3x8 basisZ = TransformDirection(
            second.rotation,
            {first.rotation.xz, first.rotation.yz, first.rotation.zz});
    return {
        {
            basisX.x, basisY.x, basisZ.x,
            basisX.y, basisY.y, basisZ.y,
            basisX.z, basisY.z, basisZ.z,
        },
        TransformPoint(second, first.translation),
    };
}

FV_E031_INLINE Iso3x8 Inverse(const Iso3x8 &transform) {
    const Mat3x8 inverseRotation = {
        transform.rotation.xx,
        transform.rotation.yx,
        transform.rotation.zx,
        transform.rotation.xy,
        transform.rotation.yy,
        transform.rotation.zy,
        transform.rotation.xz,
        transform.rotation.yz,
        transform.rotation.zz,
    };
    return {
        inverseRotation,
        TransformDirection(inverseRotation, Negate(transform.translation)),
    };
}

FV_E031_INLINE Iso3x8 DiagonalTransform(const Vec3x8 &scale) {
    const __m256 zero = _mm256_setzero_ps();
    return {
        {
            scale.x, zero, zero,
            zero, scale.y, zero,
            zero, zero, scale.z,
        },
        {zero, zero, zero},
    };
}

FV_E031_INLINE Iso3x8 ScaleRows(const Iso3x8 &transform,
                                const Vec3x8 &rowScale) {
    Iso3x8 result = transform;
    result.rotation.xx = _mm256_mul_ps(rowScale.x, transform.rotation.xx);
    result.rotation.xy = _mm256_mul_ps(transform.rotation.xy, rowScale.x);
    result.rotation.xz = _mm256_mul_ps(rowScale.x, transform.rotation.xz);
    result.translation.x =
            _mm256_mul_ps(transform.translation.x, rowScale.x);

    result.rotation.yx = _mm256_mul_ps(rowScale.y, transform.rotation.yx);
    result.rotation.yy = _mm256_mul_ps(transform.rotation.yy, rowScale.y);
    result.rotation.yz = _mm256_mul_ps(rowScale.y, transform.rotation.yz);
    result.translation.y =
            _mm256_mul_ps(transform.translation.y, rowScale.y);

    result.rotation.zx = _mm256_mul_ps(rowScale.z, transform.rotation.zx);
    result.rotation.zy = _mm256_mul_ps(transform.rotation.zy, rowScale.z);
    result.rotation.zz = _mm256_mul_ps(rowScale.z, transform.rotation.zz);
    result.translation.z =
            _mm256_mul_ps(transform.translation.z, rowScale.z);
    return result;
}

FV_E031_INLINE Boxx8 TransformEllipsoidBox(
        const Iso3x8 &transform,
        const Vec3x8 &radii) {
    const Mat3x8 absoluteRotation = {
        Abs(transform.rotation.xx),
        Abs(transform.rotation.xy),
        Abs(transform.rotation.xz),
        Abs(transform.rotation.yx),
        Abs(transform.rotation.yy),
        Abs(transform.rotation.yz),
        Abs(transform.rotation.zx),
        Abs(transform.rotation.zy),
        Abs(transform.rotation.zz),
    };
    return {
        TransformPoint(transform, ZeroVector()),
        TransformDirection(absoluteRotation, radii),
    };
}

FV_E031_INLINE __m256 BoundsMask(const Boxx8 &queries,
                                 const GmBoxAligned &candidate,
                                 __m256 activeMask) {
    const __m256 rejectZ = _mm256_cmp_ps(
            _mm256_add_ps(_mm256_set1_ps(candidate.halfExtents.z),
                          queries.halfExtents.z),
            Abs(_mm256_sub_ps(_mm256_set1_ps(candidate.center.z),
                              queries.center.z)),
            _CMP_LT_OQ);
    const __m256 rejectY = _mm256_cmp_ps(
            _mm256_add_ps(_mm256_set1_ps(candidate.halfExtents.y),
                          queries.halfExtents.y),
            Abs(_mm256_sub_ps(_mm256_set1_ps(candidate.center.y),
                              queries.center.y)),
            _CMP_LT_OQ);
    const __m256 rejectX = _mm256_cmp_ps(
            _mm256_add_ps(_mm256_set1_ps(candidate.halfExtents.x),
                          queries.halfExtents.x),
            Abs(_mm256_sub_ps(_mm256_set1_ps(candidate.center.x),
                              queries.center.x)),
            _CMP_LT_OQ);
    return AndNot(Or(Or(rejectZ, rejectY), rejectX), activeMask);
}

struct PacketExecution {
    const OptimizedCpuPreparedEllipsoidMeshPacket &setup;
    Iso3x8 meshToUnit;
    Iso3x8 meshToEllipsoid;
    Vec3x8 radii;
    Vec3x8 inverseRadii;
    Iso3x8 meshWorld;
    Boxx8 meshBounds;
    __m256 packetMask;
    Iso3x8 contactToWorld{};
    Iso3x8 normalToWorld{};
    bool outputTransformsReady = false;
    std::uint32_t hitMask = 0u;

    FV_E031_INLINE void EnsureOutputTransforms(void) {
        if (outputTransformsReady) {
            return;
        }
        const Iso3x8 ellipsoidToMesh = Inverse(meshToEllipsoid);
        contactToWorld = Compose(
                Compose(DiagonalTransform(radii), ellipsoidToMesh),
                meshWorld);
        normalToWorld = Compose(
                Compose(DiagonalTransform(inverseRadii), ellipsoidToMesh),
                meshWorld);
        outputTransformsReady = true;
    }

    FV_E031_INLINE void Emit(
            __m256 mask,
            const Vec3x8 &normalUnit,
            const Vec3x8 &separationUnit,
            const Vec3x8 &contactUnit,
            const Vec3x8 &extraUnit,
            GmLocalMaterialIndex triangleMaterial,
            bool primary) {
        const std::uint32_t bits = Bits(mask);
        if (bits == 0u) {
            return;
        }
        EnsureOutputTransforms();

        const Vec3x8 contactWorld =
                TransformPoint(contactToWorld, contactUnit);
        Vec3x8 normalWorld =
                TransformDirection(normalToWorld.rotation, normalUnit);
        normalWorld = Normalize(normalWorld, mask);
        const Vec3x8 separationWorld =
                TransformDirection(contactToWorld.rotation, separationUnit);

        alignas(32) std::array<float, PacketWidth> nx{}, ny{}, nz{};
        alignas(32) std::array<float, PacketWidth> sx{}, sy{}, sz{};
        alignas(32) std::array<float, PacketWidth> cx{}, cy{}, cz{};
        alignas(32) std::array<float, PacketWidth> ex{}, ey{}, ez{};
        Store(normalWorld, &nx, &ny, &nz);
        Store(separationWorld, &sx, &sy, &sz);
        Store(contactWorld, &cx, &cy, &cz);
        Store(extraUnit, &ex, &ey, &ez);

        for (std::size_t lane = 0u; lane < PacketWidth; ++lane) {
            if ((bits & (1u << lane)) == 0u) {
                continue;
            }
            GmCollision &collision = setup.buffers[lane]->AddCollision();
            collision.impulseNormal = {nx[lane], ny[lane], nz[lane]};
            collision.separation = {sx[lane], sy[lane], sz[lane]};
            collision.contactPoint = {cx[lane], cy[lane], cz[lane]};
            collision.localMaterialA = setup.materials[lane];
            collision.localMaterialB = triangleMaterial;
            collision.sphereMergePrimary = primary;
            collision.extraNegated = {ex[lane], ey[lane], ez[lane]};
        }
        hitMask |= bits;
    }

    FV_E031_INLINE void EmitFeature(
            __m256 terminalMask,
            const Vec3x8 &featurePoint,
            __m256 minimumDistanceSquared,
            bool requireRadiusContainment,
            const Vec3x8 &triangleNormal,
            GmLocalMaterialIndex triangleMaterial) {
        if (Bits(terminalMask) == 0u) {
            return;
        }
        const Vec3x8 featureToCenter =
                Subtract(ZeroVector(), featurePoint);
        const __m256 distanceSquared =
                Dot(featureToCenter, featureToCenter);
        __m256 invalid = _mm256_cmp_ps(
                minimumDistanceSquared, distanceSquared, _CMP_NLT_UQ);
        if (requireRadiusContainment) {
            invalid = Or(
                    invalid,
                    _mm256_cmp_ps(
                            _mm256_set1_ps(1.0f),
                            distanceSquared,
                            _CMP_LT_OQ));
        }
        const __m256 valid = AndNot(invalid, terminalMask);
        if (Bits(valid) == 0u) {
            return;
        }

        const __m256 safeDistanceSquared =
                Select(valid, distanceSquared, _mm256_set1_ps(1.0f));
        const __m256 distance = _mm256_sqrt_ps(safeDistanceSquared);
        const __m256 inverseDistance =
                _mm256_div_ps(_mm256_set1_ps(1.0f), distance);
        const Vec3x8 normal = Scale(featureToCenter, inverseDistance);
        const __m256 penetrationScale = _mm256_mul_ps(
                _mm256_sub_ps(distance, _mm256_set1_ps(1.0f)),
                inverseDistance);
        const Vec3x8 penetration =
                Scale(featureToCenter, penetrationScale);
        const __m256 separationAlongNormal =
                Dot(penetration, triangleNormal);
        const Vec3x8 separation =
                Scale(triangleNormal, separationAlongNormal);
        Emit(valid,
             normal,
             separation,
             featurePoint,
             triangleNormal,
             triangleMaterial,
             false);
    }

    FV_E031_INLINE void EmitEndpoint(
            __m256 terminalMask,
            const Vec3x8 &featurePoint,
            __m256 minimumDistance,
            const Vec3x8 &triangleNormal,
            GmLocalMaterialIndex triangleMaterial) {
        if (Bits(terminalMask) == 0u) {
            return;
        }
        const Vec3x8 featureToCenter =
                Subtract(ZeroVector(), featurePoint);
        const __m256 distanceSquared =
                Dot(featureToCenter, featureToCenter);
        const __m256 safeDistanceSquared =
                Select(terminalMask, distanceSquared, _mm256_set1_ps(1.0f));
        const __m256 distance = _mm256_sqrt_ps(safeDistanceSquared);
        const __m256 invalid = Or(
                _mm256_cmp_ps(
                        _mm256_set1_ps(1.0f), distance, _CMP_LT_OQ),
                _mm256_cmp_ps(
                        minimumDistance, distance, _CMP_NLT_UQ));
        const __m256 valid = AndNot(invalid, terminalMask);
        if (Bits(valid) == 0u) {
            return;
        }

        const __m256 safeDistance =
                Select(valid, distance, _mm256_set1_ps(1.0f));
        const __m256 endpointDistance = _mm256_sqrt_ps(safeDistance);
        const __m256 inverseEndpointDistance =
                _mm256_div_ps(_mm256_set1_ps(1.0f), endpointDistance);
        const Vec3x8 normal =
                Scale(featureToCenter, inverseEndpointDistance);
        const __m256 penetrationScale = _mm256_mul_ps(
                _mm256_sub_ps(endpointDistance, _mm256_set1_ps(1.0f)),
                inverseEndpointDistance);
        const Vec3x8 penetration =
                Scale(featureToCenter, penetrationScale);
        const __m256 separationAlongNormal =
                Dot(penetration, triangleNormal);
        const Vec3x8 separation =
                Scale(triangleNormal, separationAlongNormal);
        Emit(valid,
             normal,
             separation,
             featurePoint,
             triangleNormal,
             triangleMaterial,
             false);
    }

    FV_E031_INLINE void CollideTriangle(
            const OptimizedCpuStaticMeshTriangleData &triangle,
            __m256 candidateMask) {
        if (Bits(candidateMask) == 0u) {
            return;
        }

        const Vec3x8 vertex0 = TransformPoint(
                meshToUnit, Broadcast(triangle.vertices[0u]));
        const Vec3x8 vertex1 = TransformPoint(
                meshToUnit, Broadcast(triangle.vertices[1u]));
        const Vec3x8 vertex2 = TransformPoint(
                meshToUnit, Broadcast(triangle.vertices[2u]));
        const Vec3x8 edge01 = Subtract(vertex1, vertex0);
        const Vec3x8 edge02 = Subtract(vertex2, vertex0);

        Vec3x8 triangleNormal = {
            _mm256_sub_ps(_mm256_mul_ps(edge02.z, edge01.y),
                          _mm256_mul_ps(edge02.y, edge01.z)),
            _mm256_sub_ps(_mm256_mul_ps(edge01.z, edge02.x),
                          _mm256_mul_ps(edge02.z, edge01.x)),
            _mm256_sub_ps(_mm256_mul_ps(edge01.x, edge02.y),
                          _mm256_mul_ps(edge02.x, edge01.y)),
        };
        const __m256 normalLengthSquared = _mm256_add_ps(
                _mm256_add_ps(
                        _mm256_mul_ps(triangleNormal.y, triangleNormal.y),
                        _mm256_mul_ps(triangleNormal.x, triangleNormal.x)),
                _mm256_mul_ps(triangleNormal.z, triangleNormal.z));
        const __m256 usableNormal = And(
                candidateMask,
                _mm256_cmp_ps(
                        _mm256_set1_ps(
                                PhysicsTolerance::SurfaceDirectionLengthSquared),
                        normalLengthSquared,
                        _CMP_LT_OQ));
        if (Bits(usableNormal) == 0u) {
            return;
        }
        const __m256 safeNormalLengthSquared = Select(
                usableNormal, normalLengthSquared, _mm256_set1_ps(1.0f));
        const __m256 inverseNormalLength = _mm256_div_ps(
                _mm256_set1_ps(1.0f),
                _mm256_sqrt_ps(safeNormalLengthSquared));
        triangleNormal = Scale(triangleNormal, inverseNormalLength);

        const __m256 planeDistance = Dot(
                Subtract(ZeroVector(), vertex0), triangleNormal);
        const __m256 planeReject = Or(
                _mm256_cmp_ps(_mm256_set1_ps(1.0f),
                              planeDistance,
                              _CMP_LT_OQ),
                _mm256_cmp_ps(planeDistance,
                              _mm256_setzero_ps(),
                              _CMP_LT_OQ));
        __m256 remaining = AndNot(planeReject, usableNormal);
        if (Bits(remaining) == 0u) {
            return;
        }

        const __m256 safeRadicand = Select(
                remaining,
                _mm256_sub_ps(
                        _mm256_set1_ps(1.0f),
                        _mm256_mul_ps(planeDistance, planeDistance)),
                _mm256_setzero_ps());
        const __m256 edgeReach = _mm256_sqrt_ps(safeRadicand);
        const Vec3x8 projectedPoint = Add(
                ZeroVector(),
                Scale(
                        triangleNormal,
                        _mm256_xor_ps(
                                planeDistance,
                                _mm256_set1_ps(-0.0f))));

        const Vec3x8 vertices[3u] = {vertex0, vertex1, vertex2};
        for (std::size_t edgeIndex = 0u; edgeIndex < 3u; ++edgeIndex) {
            if (Bits(remaining) == 0u) {
                break;
            }
            const std::size_t nextIndex = edgeIndex == 2u ? 0u : edgeIndex + 1u;
            const Vec3x8 edgeStart = vertices[edgeIndex];
            const Vec3x8 edgeEnd = vertices[nextIndex];
            Vec3x8 edgeDirection = Subtract(edgeEnd, edgeStart);
            edgeDirection = Normalize(edgeDirection, remaining);
            const Vec3x8 edgeNormal =
                    Cross(edgeDirection, triangleNormal);
            const __m256 edgeDistance = Dot(
                    Subtract(projectedPoint, edgeStart), edgeNormal);

            const __m256 rejected = And(
                    remaining,
                    _mm256_cmp_ps(edgeReach, edgeDistance, _CMP_LT_OQ));
            remaining = AndNot(rejected, remaining);
            const __m256 outside = And(
                    remaining,
                    _mm256_cmp_ps(_mm256_setzero_ps(),
                                  edgeDistance,
                                  _CMP_LT_OQ));
            if (Bits(outside) == 0u) {
                continue;
            }

            const __m256 alongFromStart = Dot(
                    Subtract(projectedPoint, edgeStart), edgeDirection);
            const __m256 startFeature = And(
                    outside,
                    _mm256_cmp_ps(alongFromStart,
                                  _mm256_setzero_ps(),
                                  _CMP_LT_OQ));
            EmitFeature(
                    startFeature,
                    edgeStart,
                    _mm256_set1_ps(
                            PhysicsTolerance::SurfaceDirectionLengthSquared),
                    true,
                    triangleNormal,
                    triangle.material);

            const __m256 afterStart = AndNot(startFeature, outside);
            const __m256 alongFromEnd = Dot(
                    Subtract(projectedPoint, edgeEnd), edgeDirection);
            const __m256 edgeFeature = And(
                    afterStart,
                    _mm256_cmp_ps(_mm256_setzero_ps(),
                                  alongFromEnd,
                                  _CMP_NLT_UQ));
            const Vec3x8 featurePoint = Add(
                    projectedPoint,
                    Scale(
                            edgeNormal,
                            _mm256_xor_ps(
                                    edgeDistance,
                                    _mm256_set1_ps(-0.0f))));
            EmitFeature(
                    edgeFeature,
                    featurePoint,
                    _mm256_set1_ps(PhysicsTolerance::CollisionDistance),
                    false,
                    triangleNormal,
                    triangle.material);

            const __m256 endpoint = AndNot(edgeFeature, afterStart);
            EmitEndpoint(
                    endpoint,
                    edgeEnd,
                    _mm256_set1_ps(
                            PhysicsTolerance::SurfaceDirectionLengthSquared),
                    triangleNormal,
                    triangle.material);
            remaining = AndNot(outside, remaining);
        }

        const __m256 face = And(
                remaining,
                _mm256_cmp_ps(_mm256_setzero_ps(),
                              planeDistance,
                              _CMP_LT_OQ));
        Emit(face,
             triangleNormal,
             Scale(
                     triangleNormal,
                     _mm256_sub_ps(
                             planeDistance, _mm256_set1_ps(1.0f))),
             projectedPoint,
             triangleNormal,
             triangle.material,
             true);
    }
};

FV_E031_AVX2 bool RunPacketAvx2(
        const OptimizedCpuPreparedEllipsoidMeshPacket &setup,
        std::uint32_t activeMask,
        const GmIso4 &meshIso,
        const GmIso4 &meshInverse,
        const OptimizedCpuStaticMeshTriangleSidecar &triangles,
        const OptimizedCpuStaticMeshTriangleHierarchyView &hierarchy,
        std::uint32_t *hitMask) noexcept {
    if (hierarchy.cells == nullptr || hierarchy.postingIndices == nullptr ||
        hierarchy.count == 0u ||
        hierarchy.maximumTraversalDepth > PacketTraversalCapacity) {
        return false;
    }
    const Iso3x8 ellipsoidWorld = LoadIso(setup);
    const Vec3x8 radii = {
        Load(setup.radiiX.values),
        Load(setup.radiiY.values),
        Load(setup.radiiZ.values),
    };
    const Vec3x8 inverseRadii = {
        Load(setup.inverseRadiiX.values),
        Load(setup.inverseRadiiY.values),
        Load(setup.inverseRadiiZ.values),
    };
    const Iso3x8 ellipsoidToMesh =
            Compose(ellipsoidWorld, BroadcastIso(meshInverse));
    const Boxx8 meshBounds =
            TransformEllipsoidBox(ellipsoidToMesh, radii);
    const Iso3x8 meshToEllipsoid = Inverse(ellipsoidToMesh);
    PacketExecution execution{
        setup,
        ScaleRows(meshToEllipsoid, inverseRadii),
        meshToEllipsoid,
        radii,
        inverseRadii,
        BroadcastIso(meshIso),
        meshBounds,
        MaskForBits(activeMask),
        {},
        {},
        false,
        0u,
    };

    struct alignas(32) TraversalMask {
        __m256 value;
    };
    // Entries below traversalDepth are written together before either array
    // is read. Leaving the unused tail uninitialized avoids clearing the
    // fixed traversal storage on every static-mesh packet.
    std::array<u32, PacketTraversalCapacity> traversalEnds;
    std::array<TraversalMask, PacketTraversalCapacity> traversalMasks;
    std::size_t traversalDepth = 0u;

    for (u32 cellIndex = 0u;
         cellIndex < hierarchy.count;) {
        while (traversalDepth != 0u &&
               traversalEnds[traversalDepth - 1u] <= cellIndex) {
            --traversalDepth;
        }
        const GmMeshOctreeCell &cell = hierarchy.cells[cellIndex];
        const __m256 parentMask = traversalDepth == 0u
                ? execution.packetMask
                : traversalMasks[traversalDepth - 1u].value;
        __m256 laneMask = BoundsMask(
                execution.meshBounds,
                cell.Bounds(),
                parentMask);
        std::uint32_t laneBits = Bits(laneMask);
        if (laneBits == 0u) {
            cellIndex += cell.SubtreeEntryCount();
            continue;
        }
        if (!cell.ContainsTriangle()) {
            if (traversalDepth == traversalEnds.size()) {
                return false;
            }
            traversalEnds[traversalDepth] =
                    cellIndex + cell.SubtreeEntryCount();
            traversalMasks[traversalDepth].value = laneMask;
            ++traversalDepth;
            ++cellIndex;
            continue;
        }

        const u32 postingIndex = hierarchy.postingIndices[cellIndex];
        if (postingIndex == std::numeric_limits<u32>::max()) {
            return false;
        }
        ++cellIndex;
        const OptimizedCpuStaticMeshDirectTrianglePosting &posting =
                triangles.DirectTriangleAt(postingIndex);
        const OptimizedCpuStaticMeshTriangleData &triangle =
                triangles.TriangleAt(posting.triangleIndex);
        // BoundsMask and every parent mask use canonical all-zero/all-one
        // lanes, so converting through movemask and rebuilding the vector is
        // redundant. Preserve the exact mask produced by the bounds test.
        execution.CollideTriangle(triangle, laneMask);
    }
    *hitMask = execution.hitMask;
    return true;
}

#undef FV_E031_AVX2
#undef FV_E031_INLINE

#endif

}  // namespace

bool OptimizedCpuEllipsoidMeshPacketAvailable(void) noexcept {
#if FV_E031_HAS_X86_PACKET
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx2");
#else
    return false;
#endif
}

bool PrepareOptimizedCpuEllipsoidMeshPacket(
        const OptimizedCpuEllipsoidMeshPacketLane *lanes,
        std::size_t laneCount,
        std::uint32_t preparedMask,
        OptimizedCpuPreparedEllipsoidMeshPacket *prepared) noexcept {
    if (!OptimizedCpuEllipsoidMeshPacketAvailable()) {
        return false;
    }
    return BuildPreparedPacket(
            lanes, laneCount, preparedMask, prepared);
}

bool GmCollision_PreparedEllipsoidPacket_Mesh_InlineMathOptimizedCpuNativeBinary32WithStaticCache(
        const OptimizedCpuPreparedEllipsoidMeshPacket &prepared,
        std::uint32_t activeMask,
        const LocatedGmSurf &mesh,
        const GmIso4 &meshInverse,
        const OptimizedCpuStaticMeshTriangleSidecar &triangles,
        std::uint32_t *hitMask) noexcept {
    if (hitMask == nullptr || prepared.laneCount < 2u ||
        prepared.laneCount > PacketWidth) {
        return false;
    }
    *hitMask = 0u;
    const std::uint32_t laneMask =
            (1u << static_cast<unsigned int>(prepared.laneCount)) - 1u;
    activeMask &= laneMask;
    if ((activeMask & ~prepared.preparedMask) != 0u ||
        mesh.surf == nullptr || mesh.iso == nullptr ||
        typeid(*mesh.surf) != typeid(GmSurfMesh) ||
        !IsFiniteTransform(meshInverse) ||
        !triangles.IsFor(static_cast<const GmSurfMesh &>(*mesh.surf))) {
        return false;
    }
    if (activeMask == 0u) {
        return true;
    }
#if FV_E031_HAS_X86_PACKET
    OptimizedCpuStaticMeshTriangleHierarchyView hierarchy;
    if (!triangles.TriangleHierarchyView(&hierarchy)) {
        return false;
    }
    return RunPacketAvx2(
            prepared,
            activeMask,
            *mesh.iso,
            meshInverse,
            triangles,
            hierarchy,
            hitMask);
#else
    return false;
#endif
}

bool GmCollision_PreparedEllipsoidPacket_Mesh_InlineMathOptimizedCpuNativeBinary32WithCertifiedStaticMesh(
        const OptimizedCpuPreparedEllipsoidMeshPacket &prepared,
        std::uint32_t activeMask,
        const OptimizedCpuCertifiedStaticMeshPacket &mesh,
        std::uint32_t *hitMask) noexcept {
    if (hitMask == nullptr || prepared.laneCount < 2u ||
        prepared.laneCount > PacketWidth) {
        return false;
    }
    *hitMask = 0u;
    const std::uint32_t laneMask =
            (1u << static_cast<unsigned int>(prepared.laneCount)) - 1u;
    activeMask &= laneMask;
    if ((activeMask & ~prepared.preparedMask) != 0u ||
        !mesh.IsAvailable()) {
        return false;
    }
    if (activeMask == 0u) {
        return true;
    }
#if FV_E031_HAS_X86_PACKET
    return RunPacketAvx2(
            prepared,
            activeMask,
            mesh.meshIso,
            mesh.meshInverse,
            *mesh.triangles,
            mesh.hierarchy,
            hitMask);
#else
    return false;
#endif
}

bool GmCollision_EllipsoidPacket_Mesh_InlineMathOptimizedCpuNativeBinary32WithStaticCache(
        const OptimizedCpuEllipsoidMeshPacketLane *lanes,
        std::size_t laneCount,
        std::uint32_t activeMask,
        const LocatedGmSurf &mesh,
        const GmIso4 &meshInverse,
        const OptimizedCpuStaticMeshTriangleSidecar &triangles,
        std::uint32_t *hitMask) noexcept {
    if (hitMask == nullptr || !OptimizedCpuEllipsoidMeshPacketAvailable()) {
        return false;
    }
    *hitMask = 0u;
    if (mesh.surf == nullptr || typeid(*mesh.surf) != typeid(GmSurfMesh) ||
        !triangles.IsFor(static_cast<const GmSurfMesh &>(*mesh.surf))) {
        return false;
    }
    if (mesh.iso == nullptr || !IsFiniteTransform(meshInverse)) {
        return false;
    }
    OptimizedCpuPreparedEllipsoidMeshPacket prepared;
    if (!BuildPreparedPacket(
                lanes, laneCount, activeMask, &prepared)) {
        return false;
    }
    return GmCollision_PreparedEllipsoidPacket_Mesh_InlineMathOptimizedCpuNativeBinary32WithStaticCache(
            prepared,
            activeMask,
            mesh,
            meshInverse,
            triangles,
            hitMask);
}

#undef FV_E031_HAS_X86_PACKET
