#ifndef __OPEN_LOOP_CONTROLLER_H__
#define __OPEN_LOOP_CONTROLLER_H__

#ifdef __cplusplus
extern "C" {
#endif

/* 目标值：d 轴电流、电压和电角速度。 */
struct open_loop_input
{
    float target_current;   /* A */
    float target_voltage;   /* V */
    float target_phase_vel; /* rad/s */
};
typedef struct open_loop_input open_loop_input_t;

/* 斜率非负；0 保持原值，正无穷表示不限制斜率。 */
struct open_loop_limits
{
    float max_current_ramp;   /* A/s */
    float max_voltage_ramp;   /* V/s */
    float max_phase_vel_ramp; /* rad/s^2 */
};
typedef struct open_loop_limits open_loop_limits_t;

struct open_loop_state
{
    float current;        /* d 轴电流，A */
    float voltage;        /* d 轴电压，V */
    float phase;          /* 电角度，范围 [-pi, pi]，rad */
    float phase_vel;      /* 电角速度，rad/s */
    float total_distance; /* 累计电角位移，rad */
};
typedef struct open_loop_state open_loop_state_t;

/* 根据上一状态计算下一状态；输入和状态须为有限值，dt 为正的秒数。 */
open_loop_state_t open_loop_step(const open_loop_input_t *input, const open_loop_limits_t *limits,
                                 const open_loop_state_t *previous, float dt);

#ifdef __cplusplus
}
#endif

#endif
