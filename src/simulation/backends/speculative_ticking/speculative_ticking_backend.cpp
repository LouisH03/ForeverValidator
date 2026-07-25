#include "simulation/backends/speculative_ticking/speculative_ticking_backend.h"

#include "simulation/backends/optimized_cpu/optimized_cpu_binary32_math.h"
#include "simulation/runtime/replay_simulation_runtime.h"

namespace forevervalidator::simulation {

void PrepareSpeculativeTicking(
        ReplaySimulationRuntime &runtime) noexcept {
    runtime.PrepareOptimizedCpuStaticTransforms();
}

void CertifySpeculativeTickingForAdvance(
        ReplaySimulationRuntime &runtime) noexcept {
    runtime.CertifyOptimizedCpuStaticTransformsForAdvance();
}

ReplaySimulationStepExecution StepSpeculativeTicking(
        ReplaySimulationRuntime &runtime,
        const ReplayControlTick &tick) {
    const OptimizedCpuBinary32MathPath mathPath =
            SelectOptimizedCpuBinary32MathPathForActiveExecution();
    if (mathPath == OptimizedCpuBinary32MathPath::X86Sse2) {
        return runtime.StepOptimizedCpuNativeBinary32(tick);
    }
    return runtime.StepOptimizedCpu(tick);
}

}  // namespace forevervalidator::simulation
