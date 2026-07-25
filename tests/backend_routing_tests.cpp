#include "simulation/backends/simulation_backend.h"

#include <forevervalidator/validation.h>

#include <iostream>

int main() {
    using forevervalidator::SimulationBackend;
    using forevervalidator::simulation::ResolveLeafBackend;

    if (ResolveLeafBackend(SimulationBackend::Reference) !=
        SimulationBackend::Reference) {
        std::cerr << "Reference did not resolve to the reference backend\n";
        return 1;
    }
    if (ResolveLeafBackend(SimulationBackend::OptimizedCpu) !=
        SimulationBackend::OptimizedCpu) {
        std::cerr << "OptimizedCpu silently resolved to Reference\n";
        return 1;
    }
    return 0;
}
