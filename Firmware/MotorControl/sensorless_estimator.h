#ifndef __SENSORLESS_ESTIMATOR_H__
#define __SENSORLESS_ESTIMATOR_H__

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sensorless_config
{
    float observer_gain;   /* rad/s */
    float pll_bandwidth;   /* rad/s */
    float pm_flux_linkage; /* 永磁磁链，V·s，须大于 0 */
};
typedef struct sensorless_config sensorless_config_t;

struct sensorless_motor
{
    float phase_resistance; /* Ω */
    float phase_inductance; /* H */
    int32_t pole_pairs;
};
typedef struct sensorless_motor sensorless_motor_t;

/* 数组依次为 α、β 分量；电压为最新反馈值，算法内再延迟一拍。 */
struct sensorless_input
{
    float current[2]; /* A */
    float voltage[2]; /* V */
    bool current_valid;
};
typedef struct sensorless_input sensorless_input_t;

struct sensorless_state
{
    float flux[2];           /* α、β 磁链，V·s */
    float voltage_memory[2]; /* 上一拍反馈电压，V */
    float pll_pos;           /* rad */
    float phase_vel;         /* rad/s */
};
typedef struct sensorless_state sensorless_state_t;

struct sensorless_output
{
    float phase;        /* 磁链电角度，rad */
    float pll_pos;      /* PLL 电角度，rad */
    float phase_vel;    /* 电角速度，rad/s */
    float vel_estimate; /* 机械转速，圈/s */
};
typedef struct sensorless_output sensorless_output_t;

enum sensorless_status
{
    SENSORLESS_OK = 0,
    SENSORLESS_UNSTABLE_GAIN,
    SENSORLESS_CURRENT_MISSING
};
typedef enum sensorless_status sensorless_status_t;

void sensorless_estimator_reset(sensorless_state_t *state);

/* dt 为正的秒数；失败时清零状态和输出，错误锁存由调用方处理。 */
sensorless_status_t sensorless_estimator_update(sensorless_state_t *state,
                                               const sensorless_config_t *config,
                                               const sensorless_motor_t *motor,
                                               const sensorless_input_t *input,
                                               float dt, sensorless_output_t *output);

#ifdef __cplusplus
}
#endif

#endif
