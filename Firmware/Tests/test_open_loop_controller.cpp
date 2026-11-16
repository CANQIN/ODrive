#include "MotorControl/open_loop_controller.hpp"

#include <doctest.h>
#include <initializer_list>

TEST_SUITE("Open Loop Controller") {
    TEST_CASE("unlimited-ramps-and-explicit-state") {
        OpenLoopController controller;
        controller.target_current_ = -3.0f;
        controller.target_voltage_ = 5.0f;
        controller.target_vel_ = 2.0f;
        const auto& algorithm = controller;
        const OpenLoopController::State previous{1.0f, -1.0f, 0.25f, -2.0f, 10.0f};

        const auto next = algorithm.update(previous, 0.125f);
        CHECK(next.current == doctest::Approx(-3.0f));
        CHECK(next.voltage == doctest::Approx(5.0f));
        CHECK(next.phase_vel == doctest::Approx(2.0f));
        CHECK(next.phase == doctest::Approx(0.5f));
        CHECK(next.total_distance == doctest::Approx(10.25f));

        // Another caller's state must not affect evaluation of the original state.
        const auto other = algorithm.update({}, 0.25f);
        CHECK(other.total_distance == doctest::Approx(0.5f));
        const auto repeated = algorithm.update(previous, 0.125f);
        CHECK(repeated.current == next.current);
        CHECK(repeated.voltage == next.voltage);
        CHECK(repeated.phase == next.phase);
        CHECK(repeated.phase_vel == next.phase_vel);
        CHECK(repeated.total_distance == next.total_distance);
    }

    TEST_CASE("finite-ramps-stop-at-targets") {
        OpenLoopController controller;
        controller.max_current_ramp_ = 2.0f;
        controller.max_voltage_ramp_ = 4.0f;
        controller.max_phase_vel_ramp_ = 8.0f;
        controller.target_current_ = 3.0f;
        controller.target_voltage_ = 5.0f;
        controller.target_vel_ = 6.0f;

        const auto next = controller.update({1.0f, -1.0f, 0.25f, 2.0f, 10.0f}, 0.25f);
        CHECK(next.current == doctest::Approx(1.5f));
        CHECK(next.voltage == doctest::Approx(0.0f));
        CHECK(next.phase_vel == doctest::Approx(4.0f));
        CHECK(next.phase == doctest::Approx(1.25f));
        CHECK(next.total_distance == doctest::Approx(11.0f));

        controller.target_current_ = 1.75f;
        controller.target_voltage_ = -0.25f;
        controller.target_vel_ = 3.5f;
        const auto reached = controller.update(next, 0.25f);
        CHECK(reached.current == doctest::Approx(1.75f));
        CHECK(reached.voltage == doctest::Approx(-0.25f));
        CHECK(reached.phase_vel == doctest::Approx(3.5f));
        CHECK(reached.phase == doctest::Approx(2.125f));
        CHECK(reached.total_distance == doctest::Approx(11.875f));
    }

    TEST_CASE("ramps-reverse-direction") {
        OpenLoopController controller;
        controller.max_current_ramp_ = 2.0f;
        controller.max_voltage_ramp_ = 4.0f;
        controller.max_phase_vel_ramp_ = 8.0f;
        controller.target_current_ = -5.0f;
        controller.target_voltage_ = -8.0f;
        controller.target_vel_ = -6.0f;

        const auto next = controller.update({1.0f, 2.0f, 1.0f, 1.0f, 4.0f}, 0.5f);
        CHECK(next.current == doctest::Approx(0.0f));
        CHECK(next.voltage == doctest::Approx(0.0f));
        CHECK(next.phase_vel == doctest::Approx(-3.0f));
        CHECK(next.phase == doctest::Approx(-0.5f));
        CHECK(next.total_distance == doctest::Approx(2.5f));
    }

    TEST_CASE("zero-ramp-limits-hold-values-while-phase-advances") {
        OpenLoopController controller;
        controller.max_current_ramp_ = 0.0f;
        controller.max_voltage_ramp_ = 0.0f;
        controller.max_phase_vel_ramp_ = 0.0f;
        controller.target_current_ = 100.0f;
        controller.target_voltage_ = 100.0f;
        controller.target_vel_ = 100.0f;

        const auto next = controller.update({2.0f, 3.0f, 1.0f, -2.0f, 7.0f}, 0.25f);
        CHECK(next.current == doctest::Approx(2.0f));
        CHECK(next.voltage == doctest::Approx(3.0f));
        CHECK(next.phase_vel == doctest::Approx(-2.0f));
        CHECK(next.phase == doctest::Approx(0.5f));
        CHECK(next.total_distance == doctest::Approx(6.5f));
    }

    TEST_CASE("phase-wraps-in-both-directions-but-distance-does-not") {
        constexpr float pi = 3.14159265358979323846f;
        OpenLoopController controller;
        for (float direction : {-1.0f, 1.0f}) {
            controller.target_vel_ = direction * 2.0f;
            OpenLoopController::State previous;
            previous.phase = direction * (pi - 0.25f);
            previous.total_distance = direction * 20.0f;
            const auto next = controller.update(previous, 0.25f);
            CHECK(next.phase == doctest::Approx(direction * (-pi + 0.25f)));
            CHECK(next.total_distance == doctest::Approx(direction * 20.5f));

            controller.target_vel_ = direction * (10.0f * pi + 0.25f);
            previous.phase = direction * 0.5f;
            const auto multiple_turns = controller.update(previous, 1.0f);
            CHECK(multiple_turns.phase == doctest::Approx(direction * 0.75f));
            CHECK(multiple_turns.total_distance == doctest::Approx(direction * (20.25f + 10.0f * pi)));
        }
    }

    TEST_CASE("variable-dt-integrates-the-updated-velocity") {
        OpenLoopController controller;
        controller.max_current_ramp_ = 2.0f;
        controller.max_voltage_ramp_ = 3.0f;
        controller.max_phase_vel_ramp_ = 4.0f;
        controller.target_current_ = 100.0f;
        controller.target_voltage_ = 100.0f;
        controller.target_vel_ = 100.0f;

        const auto first = controller.update({}, 0.125f);
        CHECK(first.current == doctest::Approx(0.25f));
        CHECK(first.voltage == doctest::Approx(0.375f));
        CHECK(first.phase_vel == doctest::Approx(0.5f));
        CHECK(first.phase == doctest::Approx(0.0625f));
        CHECK(first.total_distance == doctest::Approx(0.0625f));
        const auto second = controller.update(first, 0.25f);
        CHECK(second.current == doctest::Approx(0.75f));
        CHECK(second.voltage == doctest::Approx(1.125f));
        CHECK(second.phase_vel == doctest::Approx(1.5f));
        CHECK(second.phase == doctest::Approx(0.4375f));
        CHECK(second.total_distance == doctest::Approx(0.4375f));
    }
}
