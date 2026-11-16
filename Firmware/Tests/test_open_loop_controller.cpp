#include "MotorControl/open_loop_controller.h"

#include <doctest.h>
#include <cmath>
#include <initializer_list>

TEST_SUITE("Open Loop Controller") {
    TEST_CASE("unlimited-ramps-and-explicit-state") {
        open_loop_input_t input{};
        open_loop_limits_t limits{INFINITY, INFINITY, INFINITY};
        input.target_current = -3.0f;
        input.target_voltage = 5.0f;
        input.target_phase_vel = 2.0f;
        const open_loop_state_t previous{1.0f, -1.0f, 0.25f, -2.0f, 10.0f};

        const auto next = open_loop_step(&input, &limits, &previous, 0.125f);
        CHECK(next.current == doctest::Approx(-3.0f));
        CHECK(next.voltage == doctest::Approx(5.0f));
        CHECK(next.phase_vel == doctest::Approx(2.0f));
        CHECK(next.phase == doctest::Approx(0.5f));
        CHECK(next.total_distance == doctest::Approx(10.25f));

        // 不同调用方的状态互不影响。
        const open_loop_state_t reset{};
        const auto other = open_loop_step(&input, &limits, &reset, 0.25f);
        CHECK(other.total_distance == doctest::Approx(0.5f));
        const auto repeated = open_loop_step(&input, &limits, &previous, 0.125f);
        CHECK(repeated.current == next.current);
        CHECK(repeated.voltage == next.voltage);
        CHECK(repeated.phase == next.phase);
        CHECK(repeated.phase_vel == next.phase_vel);
        CHECK(repeated.total_distance == next.total_distance);
    }

    TEST_CASE("finite-ramps-stop-at-targets") {
        open_loop_input_t input{};
        open_loop_limits_t limits{INFINITY, INFINITY, INFINITY};
        limits.max_current_ramp = 2.0f;
        limits.max_voltage_ramp = 4.0f;
        limits.max_phase_vel_ramp = 8.0f;
        input.target_current = 3.0f;
        input.target_voltage = 5.0f;
        input.target_phase_vel = 6.0f;

        const open_loop_state_t previous{1.0f, -1.0f, 0.25f, 2.0f, 10.0f};
        const auto next = open_loop_step(&input, &limits, &previous, 0.25f);
        CHECK(next.current == doctest::Approx(1.5f));
        CHECK(next.voltage == doctest::Approx(0.0f));
        CHECK(next.phase_vel == doctest::Approx(4.0f));
        CHECK(next.phase == doctest::Approx(1.25f));
        CHECK(next.total_distance == doctest::Approx(11.0f));

        input.target_current = 1.75f;
        input.target_voltage = -0.25f;
        input.target_phase_vel = 3.5f;
        const auto reached = open_loop_step(&input, &limits, &next, 0.25f);
        CHECK(reached.current == doctest::Approx(1.75f));
        CHECK(reached.voltage == doctest::Approx(-0.25f));
        CHECK(reached.phase_vel == doctest::Approx(3.5f));
        CHECK(reached.phase == doctest::Approx(2.125f));
        CHECK(reached.total_distance == doctest::Approx(11.875f));
    }

    TEST_CASE("ramps-reverse-direction") {
        open_loop_input_t input{};
        open_loop_limits_t limits{INFINITY, INFINITY, INFINITY};
        limits.max_current_ramp = 2.0f;
        limits.max_voltage_ramp = 4.0f;
        limits.max_phase_vel_ramp = 8.0f;
        input.target_current = -5.0f;
        input.target_voltage = -8.0f;
        input.target_phase_vel = -6.0f;

        const open_loop_state_t previous{1.0f, 2.0f, 1.0f, 1.0f, 4.0f};
        const auto next = open_loop_step(&input, &limits, &previous, 0.5f);
        CHECK(next.current == doctest::Approx(0.0f));
        CHECK(next.voltage == doctest::Approx(0.0f));
        CHECK(next.phase_vel == doctest::Approx(-3.0f));
        CHECK(next.phase == doctest::Approx(-0.5f));
        CHECK(next.total_distance == doctest::Approx(2.5f));
    }

    TEST_CASE("zero-ramp-limits-hold-values-while-phase-advances") {
        open_loop_input_t input{};
        open_loop_limits_t limits{INFINITY, INFINITY, INFINITY};
        limits.max_current_ramp = 0.0f;
        limits.max_voltage_ramp = 0.0f;
        limits.max_phase_vel_ramp = 0.0f;
        input.target_current = 100.0f;
        input.target_voltage = 100.0f;
        input.target_phase_vel = 100.0f;

        const open_loop_state_t previous{2.0f, 3.0f, 1.0f, -2.0f, 7.0f};
        const auto next = open_loop_step(&input, &limits, &previous, 0.25f);
        CHECK(next.current == doctest::Approx(2.0f));
        CHECK(next.voltage == doctest::Approx(3.0f));
        CHECK(next.phase_vel == doctest::Approx(-2.0f));
        CHECK(next.phase == doctest::Approx(0.5f));
        CHECK(next.total_distance == doctest::Approx(6.5f));
    }

    TEST_CASE("phase-wraps-in-both-directions-but-distance-does-not") {
        constexpr float pi = 3.14159265358979323846f;
        open_loop_input_t input{};
        open_loop_limits_t limits{INFINITY, INFINITY, INFINITY};
        for (float direction : {-1.0f, 1.0f}) {
            input.target_phase_vel = direction * 2.0f;
            open_loop_state_t previous{};
            previous.phase = direction * (pi - 0.25f);
            previous.total_distance = direction * 20.0f;
            const auto next = open_loop_step(&input, &limits, &previous, 0.25f);
            CHECK(next.phase == doctest::Approx(direction * (-pi + 0.25f)));
            CHECK(next.total_distance == doctest::Approx(direction * 20.5f));

            input.target_phase_vel = direction * (10.0f * pi + 0.25f);
            previous.phase = direction * 0.5f;
            const auto multiple_turns = open_loop_step(&input, &limits, &previous, 1.0f);
            CHECK(multiple_turns.phase == doctest::Approx(direction * 0.75f));
            CHECK(multiple_turns.total_distance == doctest::Approx(direction * (20.25f + 10.0f * pi)));
        }
    }

    TEST_CASE("variable-dt-integrates-the-updated-velocity") {
        open_loop_input_t input{};
        open_loop_limits_t limits{INFINITY, INFINITY, INFINITY};
        limits.max_current_ramp = 2.0f;
        limits.max_voltage_ramp = 3.0f;
        limits.max_phase_vel_ramp = 4.0f;
        input.target_current = 100.0f;
        input.target_voltage = 100.0f;
        input.target_phase_vel = 100.0f;

        const open_loop_state_t previous{};
        const auto first = open_loop_step(&input, &limits, &previous, 0.125f);
        CHECK(first.current == doctest::Approx(0.25f));
        CHECK(first.voltage == doctest::Approx(0.375f));
        CHECK(first.phase_vel == doctest::Approx(0.5f));
        CHECK(first.phase == doctest::Approx(0.0625f));
        CHECK(first.total_distance == doctest::Approx(0.0625f));
        const auto second = open_loop_step(&input, &limits, &first, 0.25f);
        CHECK(second.current == doctest::Approx(0.75f));
        CHECK(second.voltage == doctest::Approx(1.125f));
        CHECK(second.phase_vel == doctest::Approx(1.5f));
        CHECK(second.phase == doctest::Approx(0.4375f));
        CHECK(second.total_distance == doctest::Approx(0.4375f));
    }
}
