#include "simulation/backends/cuda/cuda_collision_certification.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

int main() {
    using namespace forevervalidator::simulation;
    using cuda::collision::CudaCollision;
    constexpr std::array<std::uint32_t, 12u> order = {
            7u, 2u, 11u, 0u, 5u, 9u,
            1u, 10u, 4u, 8u, 3u, 6u};
    std::vector<CudaCollision> collisions;
    for (std::uint32_t id : order) {
        CudaCollision collision;
        collision.contactPoint = {
                static_cast<float>(id),
                static_cast<float>(11u - id),
                static_cast<float>(id & 1u)};
        collision.impulseNormal = {
                0.01f * static_cast<float>(id),
                1.0f,
                -0.02f * static_cast<float>(id)};
        collision.materialA = id;
        collisions.push_back(collision);
    }
    const CudaCollisionOrderingExecution sorted =
            ExecuteCudaCollisionOrderingForCertification(collisions);
    if (!sorted.success ||
        sorted.collisions.size() != collisions.size()) {
        std::cerr << sorted.diagnostic << '\n';
        return 1;
    }
    for (std::uint32_t index = 0u;
         index < sorted.collisions.size(); ++index) {
        const std::uint32_t expected =
                static_cast<std::uint32_t>(
                        sorted.collisions.size() - 1u - index);
        if (sorted.collisions[index].materialA != expected) {
            std::cerr << "CUDA collision response ordering diverged at "
                      << index << " actual="
                      << sorted.collisions[index].materialA << '\n';
            return 1;
        }
    }
    const auto overflow =
            ExecuteCudaCollisionOrderingForCertification(
                    std::vector<CudaCollision>(
                            cuda::collision::CollisionCapacity + 1u));
    if (overflow.success ||
        overflow.diagnostic.find("capacity") ==
                std::string::npos) {
        std::cerr << "CUDA collision ordering overflow was not explicit\n";
        return 1;
    }
    return 0;
}
