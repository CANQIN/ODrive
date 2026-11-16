#include "sensorless_estimator.h"

#include <float.h>
#include <math.h>

static float _sensorless_wrap_phase(float phase)
{
    const float two_pi = 2.0f * 3.14159265358979323846f;
    return phase - nearbyintf(phase / two_pi) * two_pi;
}

/* 沿用原有 atan2 近似。 */
static float _sensorless_atan2(float y, float x)
{
    const float abs_y = fabsf(y);
    const float abs_x = fabsf(x);
    const float lower = abs_y < abs_x ? abs_y : abs_x;
    const float upper = abs_x < abs_y ? abs_y : abs_x;
    const float a = lower / (upper + FLT_MIN);
    const float s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;

    if (abs_y > abs_x)
    {
        r = 1.57079637f - r;
    }
    if (x < 0.0f)
    {
        r = 3.14159274f - r;
    }
    if (y < 0.0f)
    {
        r = -r;
    }
    return r;
}

void sensorless_estimator_reset(sensorless_state_t *state)
{
    *state = (sensorless_state_t){0};
}

sensorless_status_t sensorless_estimator_update(sensorless_state_t *state,
                                               const sensorless_config_t *config,
                                               const sensorless_motor_t *motor,
                                               const sensorless_input_t *input,
                                               float dt, sensorless_output_t *output)
{
    const float kp = 2.0f * config->pll_bandwidth;
    const float ki = 0.25f * (kp * kp);
    *output = (sensorless_output_t){0};

    if (!(dt * kp < 1.0f))
    {
        sensorless_estimator_reset(state);
        return SENSORLESS_UNSTABLE_GAIN;
    }
    if (!input->current_valid)
    {
        sensorless_estimator_reset(state);
        return SENSORLESS_CURRENT_MISSING;
    }

    /* 电压减去电阻压降，积分得到磁链，再扣除电感磁链。 */
    float eta[2];
    for (int i = 0; i < 2; i++)
    {
        const float voltage = -motor->phase_resistance * input->current[i] + state->voltage_memory[i];
        state->flux[i] += voltage * dt;
        eta[i] = state->flux[i] - motor->phase_inductance * input->current[i];
    }

    /* 非线性校正，使永磁磁链幅值趋近配置值。 */
    const float pm_flux_sqr = config->pm_flux_linkage * config->pm_flux_linkage;
    const float estimated_flux_sqr = eta[0] * eta[0] + eta[1] * eta[1];
    const float bandwidth_factor = 1.0f / pm_flux_sqr;
    const float eta_factor = 0.5f * (config->observer_gain * bandwidth_factor)
                           * (pm_flux_sqr - estimated_flux_sqr);
    for (int i = 0; i < 2; i++)
    {
        const float correction = eta_factor * eta[i];
        state->flux[i] += correction * dt;
        eta[i] = state->flux[i] - motor->phase_inductance * input->current[i];
        state->voltage_memory[i] = input->voltage[i];
    }

    /* PLL 使用上一拍角速度预测，再用磁链角度修正。 */
    const float predicted = _sensorless_wrap_phase(state->pll_pos + dt * state->phase_vel);
    const float phase = _sensorless_atan2(eta[1], eta[0]);
    const float delta = _sensorless_wrap_phase(phase - predicted);
    state->pll_pos = _sensorless_wrap_phase(predicted + dt * kp * delta);
    state->phase_vel += dt * ki * delta;

    output->phase = phase;
    output->pll_pos = state->pll_pos;
    output->phase_vel = state->phase_vel;
    output->vel_estimate = state->phase_vel
                         / (fmaxf((float)motor->pole_pairs, 1.0f) * 2.0f * 3.14159265358979323846f);
    return SENSORLESS_OK;
}
