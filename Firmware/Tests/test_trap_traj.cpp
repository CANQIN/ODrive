#include "MotorControl/trapTraj.hpp"

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

    TrapezoidalTrajectory traj{};

    int replan_counter = 0;

    do {
        if (replan_counter <= 0) {
            CHECK(traj.planTrapezoidal(goal, position, velocity, Vmax, Amax, Dmax));
            t = 0.0f;
            replan_counter = replan_interval;
        } else {
            replan_counter--;
        }

        TrapezoidalTrajectory::Step_t step = traj.eval(t);
        t += dt;

        //std::cerr << "vel: " << step.Yd << ", pos: " << step.Y << "\n";
        
        // Check if acceleration within bounds
        if (velocity >= 0.0f) {
            CHECK(step.Ydd <= Amax);
            CHECK(step.Ydd >= -Dmax);
            CHECK((step.Yd - velocity) / dt <= Amax * 1.002f);
            CHECK((step.Yd - velocity) / dt >= -Dmax * 1.002f);
        } else {
            CHECK(step.Ydd <= Dmax);
            CHECK(step.Ydd >= -Amax);
            CHECK((step.Yd - velocity) / dt <= Dmax * 1.002f);
            CHECK((step.Yd - velocity) / dt >= -Amax * 1.002f);
        }

        // Check if velocity within bounds
        CHECK(step.Yd >= -Vmax_test);
        CHECK(step.Yd <= Vmax_test);
        CHECK((step.Y - position) / dt >= -Vmax_test * 1.002f);
        CHECK((step.Y - position) / dt <= Vmax_test * 1.002f);
        velocity = step.Yd;

        // Check if position is making progress
        // TODO: the trajectory planner currently needs three "warm-up" iterations
        // until its position makes progress. This should probably be revisited.
        // TODO: this is disabled currently because there are legitimate trajectories
        // where the position first moves in the wrong direction.
        //if ((replan_counter < replan_interval - 2) && (t <= traj.Tf_)) {
        //    CHECK(std::abs(step.Y - goal) < std::abs(position - goal));
        //}
        position = step.Y;

    } while (t <= traj.Tf_);

    CHECK(position >= goal - 1.0f);
    CHECK(position <= goal + 1.0f);
    CHECK(velocity >= -Dmax * dt);
    CHECK(velocity <= Dmax * dt);
}


TEST_SUITE("Trajectory Planner") {
    TEST_CASE("trapezoid-phase-boundaries-and-independent-evaluation") {
        TrapezoidalTrajectory traj{};
        REQUIRE(traj.planTrapezoidal(10.0f, 0.0f, 0.0f, 2.0f, 1.0f, 1.0f));
        CHECK(traj.Ta_ == doctest::Approx(2.0f));
        CHECK(traj.Tv_ == doctest::Approx(3.0f));
        CHECK(traj.Td_ == doctest::Approx(2.0f));
        CHECK(traj.Tf_ == doctest::Approx(7.0f));

        const auto& planned = traj;
        const auto check_step = [&planned](float t, float pos, float vel, float accel) {
            const auto step = planned.eval(t);
            CHECK(step.Y == doctest::Approx(pos));
            CHECK(step.Yd == doctest::Approx(vel));
            CHECK(step.Ydd == doctest::Approx(accel));
        };
        // Query out of order and repeat: evaluation must not change playback state.
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
        TrapezoidalTrajectory traj{};
        REQUIRE(traj.planTrapezoidal(1.0f, 0.0f, 0.0f, 2.0f, 1.0f, 1.0f));
        CHECK(traj.Ta_ == doctest::Approx(1.0f));
        CHECK(traj.Tv_ == doctest::Approx(0.0f));
        CHECK(traj.Td_ == doctest::Approx(1.0f));
        CHECK(traj.Tf_ == doctest::Approx(2.0f));
        const auto peak = traj.eval(1.0f);
        CHECK(peak.Y == doctest::Approx(0.5f));
        CHECK(peak.Yd == doctest::Approx(1.0f));
        CHECK(peak.Ydd == doctest::Approx(-1.0f));
        const auto end = traj.eval(2.0f);
        CHECK(end.Y == doctest::Approx(1.0f));
        CHECK(end.Yd == doctest::Approx(0.0f));
        CHECK(end.Ydd == doctest::Approx(0.0f));
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
