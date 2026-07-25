#ifndef FOREVERVALIDATOR_SPECULATIVE_TICKING_BACKEND_H
#define FOREVERVALIDATOR_SPECULATIVE_TICKING_BACKEND_H

class ReplaySimulationRuntime;
struct ReplayControlTick;
struct ReplaySimulationStepExecution;

namespace forevervalidator::simulation {

void PrepareSpeculativeTicking(
        ReplaySimulationRuntime &runtime) noexcept;
void CertifySpeculativeTickingForAdvance(
        ReplaySimulationRuntime &runtime) noexcept;
ReplaySimulationStepExecution StepSpeculativeTicking(
        ReplaySimulationRuntime &runtime,
        const ReplayControlTick &tick);

}  // namespace forevervalidator::simulation

#endif
