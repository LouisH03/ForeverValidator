#include "simulation/backends/cuda/cuda_dynamics_certification.h"

#include <iostream>

int main() {
    const auto result = forevervalidator::simulation::
            CertifyCudaPreCollisionDynamics();
    if (!result.success) {
        std::cerr << result.diagnostic << '\n';
        return 1;
    }
    std::cout << result.diagnostic
              << " checked_states=" << result.checkedStates
              << " checked_fields=" << result.checkedFields << '\n';
    return 0;
}
