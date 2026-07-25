#include "simulation/backends/simulation_backend.h"

namespace forevervalidator::simulation {

bool IsSimulationBackendSupported(SimulationBackend backend) noexcept {
    switch (backend) {
    case SimulationBackend::Reference:
    case SimulationBackend::OptimizedCpu:
    case SimulationBackend::SpeculativeTicking:
    case SimulationBackend::Batched:
        return true;
    }
    return false;
}

}  // namespace forevervalidator::simulation
