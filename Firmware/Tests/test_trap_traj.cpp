#include "MotorControl/trap_traj.h"

#include <doctest.h>
#include <algorithm>
#include <climits>
#include <cmath>

static_assert(sizeof(float) * CHAR_BIT == 32);


void run_trajectory_test(float goal, float position, float velocity, float Vmax, float Amax, float Dmax) {
    float dt = 0.000125f;
    int replan_interval = 10; // must be > 2 (see note below)
    float t = 0.0f;
    float Vmax_test = std::max(Vmax, std::abs(velocity));

    trap_traj_t traj{};

    int replan_counter = 0;

    do {
        if (replan_counter <= 0) {
            const trap_traj_input_t input{position, velocity, goal, Vmax, Amax, Dmax};
            trap_traj_plan(&traj, &input);
            t = 0.0f;
            replan_counter = replan_interval;
        } else {
            replan_counter--;
        }

        const trap_traj_output_t step = trap_traj_eval(&traj, t);
        t += dt;

        //std::cerr << "vel: " << step.velocity << ", pos: " << step.position << "\n";
        
        // Check if acceleration within bounds
        if (velocity >= 0.0f) {
            CHECK(step.acceleration <= Amax);
            CHECK(step.acceleration >= -Dmax);
            CHECK((step.velocity - velocity) / dt <= Amax * 1.002f);
            CHECK((step.velocity - velocity) / dt >= -Dmax * 1.002f);
        } else {
            CHECK(step.acceleration <= Dmax);
            CHECK(step.acceleration >= -Amax);
            CHECK((step.velocity - velocity) / dt <= Dmax * 1.002f);
            CHECK((step.velocity - velocity) / dt >= -Amax * 1.002f);
        }

        // Check if velocity within bounds
        CHECK(step.velocity >= -Vmax_test);
        CHECK(step.velocity <= Vmax_test);
        CHECK((step.position - position) / dt >= -Vmax_test * 1.002f);
        CHECK((step.position - position) / dt <= Vmax_test * 1.002f);
        velocity = step.velocity;

        // Check if position is making progress
        // TODO: the trajectory planner currently needs three "warm-up" iterations
        // until its position makes progress. This should probably be revisited.
        // TODO: this is disabled currently because there are legitimate trajectories
        // where the position first moves in the wrong direction.
        //if ((replan_counter < replan_interval - 2) && (t <= trap_traj_duration(&traj))) {
        //    CHECK(std::abs(step.position - goal) < std::abs(position - goal));
        //}
        position = step.position;

    } while (t <= trap_traj_duration(&traj));

    CHECK(position >= goal - 1.0f);
    CHECK(position <= goal + 1.0f);
    CHECK(velocity >= -Dmax * dt);
    CHECK(velocity <= Dmax * dt);
}


TEST_SUITE("Trajectory Planner") {
    TEST_CASE("plans-can-be-copied-and-replanned-independently") {
        trap_traj_t trajectory{};
        const trap_traj_input_t forward{0.0f, 0.0f, 10.0f, 2.0f, 1.0f, 1.0f};
        trap_traj_plan(&trajectory, &forward);
        const trap_traj_t saved = trajectory;

        const trap_traj_input_t reverse{0.0f, 0.0f, -1.0f, 2.0f, 1.0f, 1.0f};
        trap_traj_plan(&trajectory, &reverse);
        CHECK(trap_traj_duration(&saved) == doctest::Approx(7.0f));
        CHECK(trap_traj_duration(&trajectory) == doctest::Approx(2.0f));
        CHECK(trap_traj_eval(&saved, 1.0f).position == doctest::Approx(0.5f));
        CHECK(trap_traj_eval(&trajectory, 1.0f).position == doctest::Approx(-0.5f));
    }

    TEST_CASE("stationary-plan-has-zero-duration") {
        trap_traj_t trajectory{};
        const trap_traj_input_t input{3.5f, 0.0f, 3.5f, 2.0f, 1.0f, 1.0f};
        trap_traj_plan(&trajectory, &input);
        CHECK(trap_traj_duration(&trajectory) == doctest::Approx(0.0f));
        const trap_traj_output_t output = trap_traj_eval(&trajectory, 0.0f);
        CHECK(output.position == doctest::Approx(3.5f));
        CHECK(output.velocity == doctest::Approx(0.0f));
        CHECK(output.acceleration == doctest::Approx(0.0f));
    }

    TEST_CASE("trapezoid-phase-boundaries-and-independent-evaluation") {
        trap_traj_t traj{};
        const trap_traj_input_t input{0.0f, 0.0f, 10.0f, 2.0f, 1.0f, 1.0f};
        trap_traj_plan(&traj, &input);
        CHECK(trap_traj_duration(&traj) == doctest::Approx(7.0f));

        const trap_traj_t& planned = traj;
        const auto check_step = [&planned](float t, float pos, float vel, float accel) {
            const auto step = trap_traj_eval(&planned, t);
            CHECK(step.position == doctest::Approx(pos));
            CHECK(step.velocity == doctest::Approx(vel));
            CHECK(step.acceleration == doctest::Approx(accel));
        };
        // 乱序和重复求值不改变轨迹。
        check_step(7.0f, 10.0f, 0.0f, 0.0f);
        check_step(1.0f, 0.5f, 1.0f, 1.0f);
        check_step(-1.0f, 0.0f, 0.0f, 0.0f);
        check_step(0.0f, 0.0f, 0.0f, 1.0f);
        check_step(2.0f, 2.0f, 2.0f, 0.0f);
        check_step(5.0f, 8.0f, 2.0f, -1.0f);
        check_step(6.0f, 9.5f, 1.0f, -1.0f);
        check_step(8.0f, 10.0f, 0.0f, 0.0f);
        check_step(1.0f, 0.5f, 1.0f, 1.0f);
    }

    TEST_CASE("triangle-phase-boundaries") {
        trap_traj_t traj{};
        const trap_traj_input_t input{0.0f, 0.0f, 1.0f, 2.0f, 1.0f, 1.0f};
        trap_traj_plan(&traj, &input);
        CHECK(trap_traj_duration(&traj) == doctest::Approx(2.0f));
        const auto peak = trap_traj_eval(&traj, 1.0f);
        CHECK(peak.position == doctest::Approx(0.5f));
        CHECK(peak.velocity == doctest::Approx(1.0f));
        CHECK(peak.acceleration == doctest::Approx(-1.0f));
        const auto end = trap_traj_eval(&traj, 2.0f);
        CHECK(end.position == doctest::Approx(1.0f));
        CHECK(end.velocity == doctest::Approx(0.0f));
        CHECK(end.acceleration == doctest::Approx(0.0f));
    }

    // these form a triangle trajectory because 2*v^2/(2*a) = 2 * 27712^2 / (2*22288) = 34456 > 16384
    TEST_CASE("neg-dir-triangle") {
        run_trajectory_test(-8192.0f, 8192.0f, 0.0f, 27712.0f, 22288.0f, 22288.0f);
    }
    TEST_CASE("pos-dir-triangle") {
        run_trajectory_test(8192.0f, -8192.0f, 0.0f, 27712.0f, 22288.0f, 22288.0f);
    }

    // these form a trapezoid trajectory because 2*v^2/(2*a) = 2 * 27712^2 / (2*22288) = 34456 < 16384
    TEST_CASE("neg-dir-trapezoid") {
        run_trajectory_test(-25000.0f, 25000.0f, 0.0f, 27712.0f, 22288.0f, 22288.0f);
    }
    TEST_CASE("pos-dir-trapezoid") {
        run_trajectory_test(25000.0f, -25000.0f, 0.0f, 27712.0f, 22288.0f, 22288.0f);
    }

    // for the following tests note that v^2/(2*a) = 27712^2 / (2*22288) = 17227 > 16384
    TEST_CASE("neg-dir-not-enough-braking-distance") {
        run_trajectory_test(-8192.0f, 8192.0f, -27712.0f, 27712.0f, 22288.0f, 22288.0f);
    }
    TEST_CASE("pos-dir-not-enough-braking-distance") {
        run_trajectory_test(8192.0f, -8192.0f, 27712.0f, 27712.0f, 22288.0f, 22288.0f);
    }

    TEST_CASE("neg-dir-over-speed") {
        run_trajectory_test(-8192.0f, 8192.0f, -40000.0f, 27712.0f, 22288.0f, 22288.0f);
    }
    TEST_CASE("pos-dir-over-speed") {
        run_trajectory_test(8192.0f, -8192.0f, 40000.0f, 27712.0f, 22288.0f, 22288.0f);
    }
}
