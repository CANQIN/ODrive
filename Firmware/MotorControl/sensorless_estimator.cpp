
#include "sensorless_estimator.hpp"
#include "pll.hpp"
#include "utils.hpp"

void SensorlessEstimator::reset() {
    pll_pos_ = 0.0f;
    vel_estimate_ = 0.0f;
    V_alpha_beta_memory_[0] = 0.0f;
    V_alpha_beta_memory_[1] = 0.0f;
    flux_state_[0] = 0.0f;
    flux_state_[1] = 0.0f;
}

bool SensorlessEstimator::update(const Input& input, const MotorModel& motor, float dt) {
    // Algorithm based on paper: Sensorless Control of Surface-Mount Permanent-Magnet Synchronous Motors Based on a Nonlinear Observer
    // http://cas.ensmp.fr/~praly/Telechargement/Journaux/2010-IEEE_TPEL-Lee-Hong-Nam-Ortega-Praly-Astolfi.pdf
    // In particular, equation 8 (and by extension eqn 4 and 6).

    // The V_alpha_beta applied immedietly prior to the current measurement associated with this cycle
    // is the one computed two cycles ago. To get the correct measurement, it was stored twice:
    // once by the FOC voltage reporting (input.voltage_feedback), and once by V_alpha_beta_memory.

    // PLL
    // Pll gains as a function of bandwidth
    const PllGains gains = pll_gains_from_bandwidth(config_.pll_bandwidth);

    // Check that we don't get problems with discrete time approximation
    if (!(dt * gains.kp < 1.0f)) {
        error_ |= ERROR_UNSTABLE_GAIN;
        reset(); // Reset state for when the next valid current measurement comes in.
        return false;
    }

    if (!input.current.has_value()) {
        error_ |= ERROR_UNKNOWN_CURRENT_MEASUREMENT;
        reset(); // Reset state for when the next valid current measurement comes in.
        return false;
    }

    float I_alpha_beta[2] = {
        input.current->alpha,
        input.current->beta};

    // alpha-beta vector operations
    float eta[2];
    for (int i = 0; i <= 1; ++i) {
        // y is the total flux-driving voltage (see paper eqn 4)
        float y = -motor.phase_resistance * I_alpha_beta[i] + V_alpha_beta_memory_[i];
        // flux dynamics (prediction)
        float x_dot = y;
        // integrate prediction to current timestep
        flux_state_[i] += x_dot * dt;

        // eta is the estimated permanent magnet flux (see paper eqn 6)
        eta[i] = flux_state_[i] - motor.phase_inductance * I_alpha_beta[i];
    }

    // Non-linear observer (see paper eqn 8):
    float pm_flux_sqr = config_.pm_flux_linkage * config_.pm_flux_linkage;
    float est_pm_flux_sqr = eta[0] * eta[0] + eta[1] * eta[1];
    float bandwidth_factor = 1.0f / pm_flux_sqr;
    float eta_factor = 0.5f * (config_.observer_gain * bandwidth_factor) * (pm_flux_sqr - est_pm_flux_sqr);

    // alpha-beta vector operations
    for (int i = 0; i <= 1; ++i) {
        // add observer action to flux estimate dynamics
        float x_dot = eta_factor * eta[i];
        // convert action to discrete-time
        flux_state_[i] += x_dot * dt;
        // update new eta
        eta[i] = flux_state_[i] - motor.phase_inductance * I_alpha_beta[i];
    }

    // Flux state estimation done, store V_alpha_beta for next timestep
    V_alpha_beta_memory_[0] = input.voltage_feedback.alpha;
    V_alpha_beta_memory_[1] = input.voltage_feedback.beta;

    // predict PLL phase with velocity
    PllState predicted = pll_predict(
        {pll_pos_, phase_vel_.previous().value_or(0.0f)}, dt);
    predicted.position = wrap_pm_pi(predicted.position);
    // update PLL phase with observer permanent magnet phase
    float phase = fast_atan2(eta[1], eta[0]);
    float delta_phase = wrap_pm_pi(phase - predicted.position);
    const PllState result = pll_correct(predicted, delta_phase, gains, dt);
    pll_pos_ = wrap_pm_pi(result.position);

    // set outputs
    phase_ = phase;
    phase_vel_ = result.velocity;
    vel_estimate_ = result.velocity / (std::max((float)motor.pole_pairs, 1.0f) * 2.0f * M_PI);

    return true;
};
