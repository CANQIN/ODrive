#include "open_loop_controller.h"

#include <math.h>

static float _open_loop_clamp(float value, float lower, float upper)
{
    return value < lower ? lower : (value > upper ? upper : value);
}

open_loop_state_t open_loop_step(const open_loop_input_t *input, const open_loop_limits_t *limits,
                                 const open_loop_state_t *previous, float dt)
{
    open_loop_state_t next;
    next.current = _open_loop_clamp(input->target_current,
                                    previous->current - limits->max_current_ramp * dt,
                                    previous->current + limits->max_current_ramp * dt);
    next.voltage = _open_loop_clamp(input->target_voltage,
                                    previous->voltage - limits->max_voltage_ramp * dt,
                                    previous->voltage + limits->max_voltage_ramp * dt);
    next.phase_vel = _open_loop_clamp(input->target_phase_vel,
                                      previous->phase_vel - limits->max_phase_vel_ramp * dt,
                                      previous->phase_vel + limits->max_phase_vel_ramp * dt);

    /* 使用更新后的角速度积分。 */
    const float phase = previous->phase + next.phase_vel * dt;
    const float two_pi = 2.0f * 3.14159265358979323846f;
    next.phase = phase - nearbyintf(phase / two_pi) * two_pi;
    next.total_distance = previous->total_distance + next.phase_vel * dt;
    return next;
}
