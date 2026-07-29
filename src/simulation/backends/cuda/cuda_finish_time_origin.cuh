#ifndef FOREVERVALIDATOR_CUDA_FINISH_TIME_ORIGIN_CUH
#define FOREVERVALIDATOR_CUDA_FINISH_TIME_ORIGIN_CUH

#include <cstdint>

namespace forevervalidator::simulation::cuda::finish {

__host__ __device__ constexpr std::uint64_t TickStartNanoseconds(
        std::uint32_t tickTimeMs) noexcept {
    return static_cast<std::uint64_t>(tickTimeMs) * 1000000u;
}

}  // namespace forevervalidator::simulation::cuda::finish

#endif
