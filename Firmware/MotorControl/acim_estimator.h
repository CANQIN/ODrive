#ifndef __ACIM_ESTIMATOR_H__
#define __ACIM_ESTIMATOR_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct acim_config
{
    float slip_velocity; /* 转子时间常数倒数，1/s */
} acim_config_t;

typedef struct acim_input
{
    float rotor_phase;     /* 转子电角度，rad */
    float rotor_phase_vel; /* 转子电角速度，rad/s */
    float id;              /* d 轴电流给定，A */
    float iq;              /* q 轴电流给定，A */
} acim_input_t;

typedef struct acim_state
{
    float rotor_flux;   /* 归一化转子磁链，A */
    float phase_offset; /* 磁链相对转子的电角度，rad */
} acim_state_t;

typedef struct acim_output
{
    float rotor_flux;       /* 更新后的归一化转子磁链，A */
    float phase_offset;     /* 更新后的相对电角度，rad */
    float slip_vel;         /* 转差电角速度，rad/s */
    float stator_phase_vel; /* 磁链电角速度，rad/s */
    float stator_phase;     /* 磁链电角度，rad */
} acim_output_t;

/* 输入和状态须为有限值，dt 为正的秒数；启动时由调用方清零状态。 */
acim_output_t acim_estimator_step(const acim_input_t *input, const acim_config_t *config,
                                 const acim_state_t *previous, float dt);

#ifdef __cplusplus
}
#endif

#endif
