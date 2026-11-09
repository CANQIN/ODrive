#ifndef __SENSORLESS_ESTIMATOR_HPP
#define __SENSORLESS_ESTIMATOR_HPP

#include "component.hpp"
#include <autogen/interfaces.hpp>

// TODO：对外暴露配置接口显式配置；输出数据也显式暴露接口

class SensorlessEstimator : public ODriveIntf::SensorlessEstimatorIntf {
public:
    struct Config_t {
        float observer_gain = 1000.0f; // [rad/s]
        float pll_bandwidth = 1000.0f;  // [rad/s]
        float pm_flux_linkage = 1.58e-3f; // [V / (rad/s)]  { 5.51328895422 / (<pole pairs> * <rpm/v>) }
    };

    struct AlphaBeta {
        float alpha = 0.0f;
        float beta = 0.0f;
    };

    struct Input {
        // Missing current is an error; the caller supplies zero when disarmed.
        std::optional<AlphaBeta> current; // [A]
        // Latest FOC voltage feedback, before this estimator's extra delay.
        AlphaBeta voltage_feedback; // [V]
    };

    struct MotorModel {
        float phase_resistance; // [ohm]
        float phase_inductance; // [H]
        int32_t pole_pairs;
    };

    void reset();
    // dt is in seconds. On failure, latch the local error and reset the
    // observer state; the caller is responsible for system-level fault handling.
    bool update(const Input& input, const MotorModel& motor, float dt);

    Config_t config_;

    // TODO: expose on protocol
    Error error_ = ERROR_NONE;
    float pll_pos_ = 0.0f;                      // [rad]
    float flux_state_[2] = {0.0f, 0.0f};        // [Vs]
    float V_alpha_beta_memory_[2] = {0.0f, 0.0f}; // [V]

    OutputPort<float> phase_ = 0.0f;                   // [rad]
    OutputPort<float> phase_vel_ = 0.0f;               // [rad/s]
    OutputPort<float> vel_estimate_ = 0.0f;            // [turns/s]
};

#endif /* __SENSORLESS_ESTIMATOR_HPP */
