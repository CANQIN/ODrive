#ifndef __TRAP_TRAJ_H__
#define __TRAP_TRAJ_H__

#ifdef __cplusplus
extern "C" {
#endif

/* 位置单位为圈，时间单位为秒；输入须为有限值，三个限值须大于 0。 */
struct trap_traj_input
{
    float initial_position;
    float initial_velocity;
    float target_position;
    float max_velocity;
    float max_acceleration;
    float max_deceleration;
};
typedef struct trap_traj_input trap_traj_input_t;

struct trap_traj_output
{
    float position;     /* 圈 */
    float velocity;     /* 圈/s */
    float acceleration; /* 圈/s^2 */
};
typedef struct trap_traj_output trap_traj_output_t;

struct trap_traj
{
    float initial_position;
    float target_position;
    float initial_velocity;
    float acceleration;
    float cruise_velocity;
    float deceleration;
    float accel_time;
    float cruise_time;
    float decel_time;
    float duration;
    float accel_end_position;
};
typedef struct trap_traj trap_traj_t;

void trap_traj_plan(trap_traj_t *trajectory, const trap_traj_input_t *input);

/* 规划后按秒求值；负时间返回起点，结束后保持终点。 */
trap_traj_output_t trap_traj_eval(const trap_traj_t *trajectory, float time);

/* 返回规划时长，单位为秒。 */
float trap_traj_duration(const trap_traj_t *trajectory);

#ifdef __cplusplus
}
#endif

#endif
