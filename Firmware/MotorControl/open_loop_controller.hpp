#ifndef __OPEN_LOOP_CONTROLLER_HPP
#define __OPEN_LOOP_CONTROLLER_HPP

#include <limits>

class OpenLoopController {
public:
    struct State {
        float current = 0.0f;        // d-axis current [A]
        float voltage = 0.0f;        // d-axis voltage [V]
        float phase = 0.0f;          // electrical phase [rad]
        float phase_vel = 0.0f;      // electrical velocity [rad/s]
        float total_distance = 0.0f; // unwrapped electrical displacement [rad]
    };

    // Advance an explicit previous state by dt seconds (dt > 0).
    // Ramp limits must be nonnegative; +infinity disables the respective limit.
    State update(const State& previous, float dt) const;

    // Config
    float max_current_ramp_ = std::numeric_limits<float>::infinity(); // [A/s]
    float max_voltage_ramp_ = std::numeric_limits<float>::infinity(); // [V/s]
    float max_phase_vel_ramp_ = std::numeric_limits<float>::infinity(); // [rad/s^2]

    // Inputs
    float target_vel_ = 0.0f;
    float target_current_ = 0.0f;
    float target_voltage_ = 0.0f;
};

#endif // __OPEN_LOOP_CONTROLLER_HPP
