#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "engine/core/binary32_math.h"
#include "engine/core/func_keys_real.h"
#include "simulation/backends/optimized_cpu/optimized_cpu_compiled_tuning_curve.h"
#include "simulation/backends/optimized_cpu/optimized_cpu_vehicle_forces.h"
#include "simulation/runtime/replay_deterministic_execution.h"

namespace {

constexpr std::size_t InputCount = 4096u;
constexpr std::size_t Repetitions = 4096u;

volatile std::uint64_t benchmarkSink = 0u;

std::uint32_t Bits(float value) noexcept {
    std::uint32_t bits = 0u;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

std::array<float, InputCount> BuildInputs(void) {
    std::array<float, InputCount> inputs{};
    std::uint32_t state = 0x6d2b79f5u;
    for (float &input : inputs) {
        state ^= state << 13u;
        state ^= state >> 17u;
        state ^= state << 5u;
        const float unit = static_cast<float>(state & 0xffffu) / 65535.0f;
        input = unit * 180.0f - 45.0f;
    }
    return inputs;
}

float EvaluateReference(const CFuncKeysReal &curve, float speed) {
    const float input = Binary32::FromDouble(
            static_cast<double>(speed) * static_cast<double>(3.6f));
    unsigned long keyIndex = 0ul;
    float output = 0.0f;
    curve.GetValue(input, output, keyIndex);
    return output;
}

template<typename Evaluate>
std::uint64_t Measure(
        const std::array<float, InputCount> &inputs,
        Evaluate evaluate) {
    std::uint64_t checksum = 0u;
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t repetition = 0u;
         repetition < Repetitions;
         ++repetition) {
        for (float input : inputs) {
            checksum += Bits(evaluate(input));
        }
    }
    const auto stop = std::chrono::steady_clock::now();
    benchmarkSink ^= checksum;
    return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                    stop - start).count());
}

}  // namespace

int main(int argc, char **argv) {
    std::size_t keyCount = 12u;
    if (argc == 2) {
        try {
            std::size_t consumed = 0u;
            const unsigned long parsed = std::stoul(argv[1], &consumed);
            if (argv[1][consumed] != '\0' || parsed == 0u || parsed > 12u) {
                std::fprintf(stderr, "key count must be in [1, 12]\n");
                return 1;
            }
            keyCount = static_cast<std::size_t>(parsed);
        } catch (...) {
            std::fprintf(stderr, "key count must be in [1, 12]\n");
            return 1;
        }
    } else if (argc != 1) {
        std::fprintf(stderr, "usage: %s [KEY_COUNT]\n", argv[0]);
        return 1;
    }

    tmnf::simulation::DeterministicExecutionScope deterministicScope;
    if (!deterministicScope.Established()) {
        std::fprintf(stderr, "could not establish deterministic execution\n");
        return 1;
    }

    CFuncKeysReal curve;
    std::vector<CFuncKeysReal::Key> keys = {
                {-160.0f, 0.22f},
                {-80.0f, 0.29f},
                {-30.0f, 0.41f},
                {0.0f, 0.58f},
                {35.0f, 0.81f},
                {70.0f, 1.05f},
                {110.0f, 1.18f},
                {160.0f, 1.12f},
                {220.0f, 0.96f},
                {300.0f, 0.74f},
                {400.0f, 0.51f},
                {520.0f, 0.34f},
            };
    keys.resize(keyCount);
    curve.SetKeys(std::move(keys), CFuncKeysReal::Linear);

    forevervalidator::simulation::OptimizedCpuCompiledTuningCurve compiled;
    if (!compiled.TryBuild(curve)) {
        std::fprintf(stderr, "could not compile tuning curve\n");
        return 1;
    }

    const auto inputs = BuildInputs();
    for (float input : inputs) {
        const float reference = EvaluateReference(curve, input);
        const float native =
                forevervalidator::simulation::
                        OptimizedCpuEvaluateVehicleCurveForDifferential(
                                curve,
                                input,
                                true,
                                false,
                                forevervalidator::simulation::
                                        OptimizedCpuBinary32MathPath::X86Sse2);
        const float optimized = compiled.EvaluateSpeed(input);
        if (Bits(reference) != Bits(native) ||
            Bits(reference) != Bits(optimized)) {
            std::fprintf(
                    stderr,
                    "curve mismatch input=%08x reference=%08x native=%08x compiled=%08x\n",
                    Bits(input),
                    Bits(reference),
                    Bits(native),
                    Bits(optimized));
            return 1;
        }
    }

    for (unsigned warmup = 0u; warmup < 3u; ++warmup) {
        Measure(inputs, [&](float input) {
            return compiled.EvaluateSpeed(input);
        });
    }

    const std::uint64_t referenceNanoseconds = Measure(
            inputs,
            [&](float input) { return EvaluateReference(curve, input); });
    const std::uint64_t nativeNanoseconds = Measure(
            inputs,
            [&](float input) {
                return forevervalidator::simulation::
                        OptimizedCpuEvaluateVehicleCurveForDifferential(
                                curve,
                                input,
                                true,
                                false,
                                forevervalidator::simulation::
                                        OptimizedCpuBinary32MathPath::X86Sse2);
            });
    const std::uint64_t compiledNanoseconds = Measure(
            inputs,
            [&](float input) { return compiled.EvaluateSpeed(input); });

    const double calls = static_cast<double>(InputCount) * Repetitions;
    const double referencePerCall = referenceNanoseconds / calls;
    const double nativePerCall = nativeNanoseconds / calls;
    const double compiledPerCall = compiledNanoseconds / calls;
    std::printf(
            "key_count=%zu calls=%zu reference_ns_per_call=%.3f "
            "native_ns_per_call=%.3f "
            "compiled_ns_per_call=%.3f speedup_vs_reference=%.3fx "
            "speedup_vs_native=%.3fx sink=%llu result=identical\n",
            keyCount,
            InputCount * Repetitions,
            referencePerCall,
            nativePerCall,
            compiledPerCall,
            referencePerCall / compiledPerCall,
            nativePerCall / compiledPerCall,
            static_cast<unsigned long long>(benchmarkSink));
    return 0;
}
