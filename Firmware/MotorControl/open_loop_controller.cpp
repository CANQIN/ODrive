#include "open_loop_controller.hpp"

#include <algorithm>
#include <cmath>

OpenLoopController::State OpenLoopController::update(const State& previous, float dt) const {
    State next;
    next.current = std::clamp(target_current_,
                             previous.current - max_current_ramp_ * dt,
                             previous.current + max_current_ramp_ * dt);
    next.voltage = std::clamp(target_voltage_,
                             previous.voltage - max_voltage_ramp_ * dt,
                             previous.voltage + max_voltage_ramp_ * dt);
    next.phase_vel = std::clamp(target_vel_,
                               previous.phase_vel - max_phase_vel_ramp_ * dt,
                               previous.phase_vel + max_phase_vel_ramp_ * dt);

    // Preserve the existing integration order: use the updated velocity.
    const float phase = previous.phase + next.phase_vel * dt;
    constexpr float two_pi = 2.0f * 3.14159265358979323846f;
    next.phase = phase - std::nearbyint(phase / two_pi) * two_pi;
    next.total_distance = previous.total_distance + next.phase_vel * dt;
    return next;
}
