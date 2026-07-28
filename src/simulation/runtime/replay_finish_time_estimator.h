#ifndef TMNF_REPLAY_FINISH_TIME_ESTIMATOR_H
#define TMNF_REPLAY_FINISH_TIME_ESTIMATOR_H

#include <cstdint>
#include <functional>
#include <optional>

#include <forevervalidator/finish_time.h>

struct ReplayFinishSubstep {
    std::uint32_t tickTimeMs = 0u;
    std::uint32_t tickPeriodMs = 0u;
    double substepStartSeconds = 0.0;
    float substepDurationSeconds = 0.0f;
};

using ReplayFinishProbe = std::function<bool(float)>;

std::optional<forevervalidator::FinishTimeEstimate>
RefineReplayFinishTime(const ReplayFinishSubstep &substep,
                       const ReplayFinishProbe &probe);

#endif
