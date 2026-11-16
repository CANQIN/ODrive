#include "acim_estimator.h"

#include <math.h>

static float _acim_wrap_phase(float phase)
{
    const float two_pi = 2.0f * 3.14159265358979323846f;
    return phase - nearbyintf(phase / two_pi) * two_pi;
}

acim_output_t acim_estimator_step(const acim_input_t *input, const acim_config_t *config,
                                 const acim_state_t *previous, float dt)
{
    acim_output_t output;

    /* 磁链按转子时间常数跟随 d 轴电流给定，归一化为 A。 */
    const float flux_rate = config->slip_velocity * (input->id - previous->rotor_flux);
    output.rotor_flux = previous->rotor_flux + flux_rate * dt;
    output.slip_vel = config->slip_velocity * (input->iq / output.rotor_flux);

    /* 小磁链可能导致除零或过大转差；保留每周期 0.1 rad 的阈值。 */
    if (isnan(output.slip_vel) || fabsf(output.slip_vel) > 0.1f / dt)
    {
        output.slip_vel = 0.0f;
    }

    output.stator_phase_vel = input->rotor_phase_vel + output.slip_vel;
    output.phase_offset = _acim_wrap_phase(previous->phase_offset + output.slip_vel * dt);
    output.stator_phase = _acim_wrap_phase(input->rotor_phase + output.phase_offset);
    return output;
}
