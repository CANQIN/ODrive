#include "trap_traj.h"

#include <math.h>

static float _trap_traj_max(float a, float b)
{
    return a < b ? b : a;
}

void trap_traj_plan(trap_traj_t *trajectory, const trap_traj_input_t *input)
{
    trap_traj_t plan;
    const float displacement = input->target_position - input->initial_position;
    const float initial_velocity = input->initial_velocity;
    const float stopping_distance = (initial_velocity * initial_velocity) / (2.0f * input->max_deceleration);
    const float stopping_displacement = copysignf(stopping_distance, initial_velocity);
    const float direction = signbit(displacement - stopping_displacement) ? -1.0f : 1.0f;

    plan.acceleration = direction * input->max_acceleration;
    plan.deceleration = -direction * input->max_deceleration;
    plan.cruise_velocity = direction * input->max_velocity;

    /* 初速度超过巡航速度时，先减速。 */
    if (direction * initial_velocity > direction * plan.cruise_velocity)
    {
        plan.acceleration = -direction * input->max_acceleration;
    }
    plan.accel_time = (plan.cruise_velocity - initial_velocity) / plan.acceleration;
    plan.decel_time = -plan.cruise_velocity / plan.deceleration;
    const float min_displacement = 0.5f * plan.accel_time * (plan.cruise_velocity + initial_velocity)
                                + 0.5f * plan.decel_time * plan.cruise_velocity;

    if (direction * displacement < direction * min_displacement)
    {
        /* 位移不足，采用三角形速度曲线。 */
        const float peak_velocity_squared =
            (plan.deceleration * (initial_velocity * initial_velocity)
             + 2.0f * plan.acceleration * plan.deceleration * displacement)
            / (plan.deceleration - plan.acceleration);
        plan.cruise_velocity = direction * sqrtf(_trap_traj_max(peak_velocity_squared, 0.0f));
        plan.accel_time = _trap_traj_max(0.0f, (plan.cruise_velocity - initial_velocity) / plan.acceleration);
        plan.decel_time = _trap_traj_max(0.0f, -plan.cruise_velocity / plan.deceleration);
        plan.cruise_time = 0.0f;
    }
    else
    {
        plan.cruise_time = (displacement - min_displacement) / plan.cruise_velocity;
    }

    plan.duration = plan.accel_time + plan.cruise_time + plan.decel_time;
    plan.initial_position = input->initial_position;
    plan.target_position = input->target_position;
    plan.initial_velocity = initial_velocity;
    plan.accel_end_position = input->initial_position + initial_velocity * plan.accel_time
                           + 0.5f * plan.acceleration * (plan.accel_time * plan.accel_time);
    *trajectory = plan;
}

trap_traj_output_t trap_traj_eval(const trap_traj_t *trajectory, float time)
{
    const trap_traj_t *plan = trajectory;
    trap_traj_output_t output;

    if (time < 0.0f)
    {
        output.position = plan->initial_position;
        output.velocity = plan->initial_velocity;
        output.acceleration = 0.0f;
    }
    else if (time < plan->accel_time)
    {
        output.position = plan->initial_position + plan->initial_velocity * time
                        + 0.5f * plan->acceleration * (time * time);
        output.velocity = plan->initial_velocity + plan->acceleration * time;
        output.acceleration = plan->acceleration;
    }
    else if (time < plan->accel_time + plan->cruise_time)
    {
        output.position = plan->accel_end_position + plan->cruise_velocity * (time - plan->accel_time);
        output.velocity = plan->cruise_velocity;
        output.acceleration = 0.0f;
    }
    else if (time < plan->duration)
    {
        const float remaining = time - plan->duration;
        output.position = plan->target_position + 0.5f * plan->deceleration * (remaining * remaining);
        output.velocity = plan->deceleration * remaining;
        output.acceleration = plan->deceleration;
    }
    else
    {
        output.position = plan->target_position;
        output.velocity = 0.0f;
        output.acceleration = 0.0f;
    }
    return output;
}

float trap_traj_duration(const trap_traj_t *trajectory)
{
    return trajectory->duration;
}
