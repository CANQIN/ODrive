#include "MotorControl/acim_estimator.h"

#include <doctest.h>
#include <cmath>
#include <initializer_list>

TEST_SUITE("ACIM Estimator") {
    TEST_CASE("flux-follows-d-axis-current-with-explicit-period") {
        const acim_config_t config{2.0f};
        acim_input_t input{0.5f, 10.0f, 2.0f, 0.0f};
        const acim_state_t initial{};

        const auto first = acim_estimator_step(&input, &config, &initial, 0.125f);
        CHECK(first.rotor_flux == 0.5f);
        CHECK(first.slip_vel == 0.0f);
        CHECK(first.stator_phase == input.rotor_phase);
        CHECK(first.stator_phase_vel == input.rotor_phase_vel);

        acim_state_t previous{first.rotor_flux, first.phase_offset};
        const auto second = acim_estimator_step(&input, &config, &previous, 0.25f);
        CHECK(second.rotor_flux == 1.25f);

        input.id = 0.0f;
        previous = {second.rotor_flux, second.phase_offset};
        const auto decay = acim_estimator_step(&input, &config, &previous, 0.125f);
        CHECK(decay.rotor_flux == 0.9375f);
    }

    TEST_CASE("slip-uses-updated-flux-and-preserves-current-direction") {
        const acim_config_t config{2.0f};
        for (float flux_sign : {-1.0f, 1.0f}) {
            for (float torque_sign : {-1.0f, 1.0f}) {
                const acim_input_t input{0.5f, 10.0f, flux_sign * 3.0f, torque_sign * 0.3f};
                const acim_state_t previous{flux_sign, 0.25f};
                const auto output = acim_estimator_step(&input, &config, &previous, 0.125f);
                const float expected_slip = flux_sign * torque_sign * 0.4f;

                CHECK(output.rotor_flux == doctest::Approx(flux_sign * 1.5f));
                CHECK(output.slip_vel == doctest::Approx(expected_slip));
                CHECK(output.stator_phase_vel == doctest::Approx(10.0f + expected_slip));
                CHECK(output.phase_offset == doctest::Approx(0.25f + expected_slip * 0.125f));
                CHECK(output.stator_phase == doctest::Approx(0.75f + expected_slip * 0.125f));
            }
        }
    }

    TEST_CASE("zero-flux-rejects-nan-and-infinite-slip") {
        const acim_config_t config{2.0f};
        const acim_state_t previous{0.0f, 0.25f};
        for (float iq : {-1.0f, 0.0f, 1.0f}) {
            const acim_input_t input{0.5f, 10.0f, 0.0f, iq};
            const auto output = acim_estimator_step(&input, &config, &previous, 0.125f);
            CHECK(output.rotor_flux == 0.0f);
            CHECK(output.slip_vel == 0.0f);
            CHECK(output.phase_offset == 0.25f);
            CHECK(output.stator_phase == 0.75f);
            CHECK(output.stator_phase_vel == 10.0f);
        }
    }

    TEST_CASE("slip-threshold-keeps-boundary-and-rejects-excess") {
        const acim_config_t config{1.0f};
        const acim_state_t previous{1.0f, 0.0f};
        for (float direction : {-1.0f, 1.0f}) {
            acim_input_t input{0.0f, 0.0f, 1.0f, direction * 0.8f};
            const auto boundary = acim_estimator_step(&input, &config, &previous, 0.125f);
            CHECK(boundary.slip_vel == input.iq);
            CHECK(boundary.phase_offset == doctest::Approx(direction * 0.1f));

            input.iq = direction * 0.8001f;
            const auto excess = acim_estimator_step(&input, &config, &previous, 0.125f);
            CHECK(excess.slip_vel == 0.0f);
            CHECK(excess.phase_offset == 0.0f);

            const auto shorter_period = acim_estimator_step(&input, &config, &previous, 0.0625f);
            CHECK(shorter_period.slip_vel == input.iq);
        }
    }

    TEST_CASE("phase-offset-and-stator-phase-wrap-in-both-directions") {
        constexpr float two_pi = 2.0f * 3.14159265358979323846f;
        const acim_config_t config{1.0f};
        for (float direction : {-1.0f, 1.0f}) {
            acim_input_t input{direction * 3.125f, 0.0f, 1.0f, direction * 0.25f};
            acim_state_t previous{1.0f, direction * 3.125f};
            const auto offset_wrap = acim_estimator_step(&input, &config, &previous, 0.125f);
            CHECK(offset_wrap.phase_offset == doctest::Approx(direction * (3.15625f - two_pi)));
            CHECK(offset_wrap.stator_phase == doctest::Approx(direction * (6.28125f - two_pi)));

            input.iq = 0.0f;
            previous.phase_offset = direction * 0.125f;
            const auto stator_wrap = acim_estimator_step(&input, &config, &previous, 0.125f);
            CHECK(stator_wrap.phase_offset == previous.phase_offset);
            CHECK(stator_wrap.stator_phase == doctest::Approx(direction * (3.25f - two_pi)));
        }
    }

    TEST_CASE("instances-are-independent-and-input-state-is-unchanged") {
        const acim_config_t config{2.0f};
        const acim_input_t input{0.5f, 10.0f, 3.0f, 0.3f};
        const acim_state_t previous{1.0f, 0.25f};
        const auto first = acim_estimator_step(&input, &config, &previous, 0.125f);

        const acim_state_t other{2.0f, -0.5f};
        const auto independent = acim_estimator_step(&input, &config, &other, 0.125f);
        CHECK(independent.rotor_flux != first.rotor_flux);
        const auto repeated = acim_estimator_step(&input, &config, &previous, 0.125f);
        CHECK(repeated.rotor_flux == first.rotor_flux);
        CHECK(repeated.phase_offset == first.phase_offset);
        CHECK(repeated.slip_vel == first.slip_vel);
        CHECK(repeated.stator_phase_vel == first.stator_phase_vel);
        CHECK(repeated.stator_phase == first.stator_phase);
        CHECK(previous.rotor_flux == 1.0f);
        CHECK(previous.phase_offset == 0.25f);
    }
}
