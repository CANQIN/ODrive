#include "MotorControl/sensorless_estimator.h"

#include <doctest.h>
#include <cmath>
#include <cstddef>
#include <initializer_list>

static_assert(sizeof(sensorless_config_t) == 3 * sizeof(float));
static_assert(offsetof(sensorless_config_t, observer_gain) == 0);
static_assert(offsetof(sensorless_config_t, pll_bandwidth) == sizeof(float));
static_assert(offsetof(sensorless_config_t, pm_flux_linkage) == 2 * sizeof(float));

TEST_SUITE("Sensorless Estimator") {
    TEST_CASE("reset-clears-flux-delay-and-pll") {
        sensorless_state_t state{{1.0f, -2.0f}, {3.0f, -4.0f}, 1.5f, 20.0f};
        sensorless_estimator_reset(&state);
        CHECK(state.flux[0] == 0.0f);
        CHECK(state.flux[1] == 0.0f);
        CHECK(state.voltage_memory[0] == 0.0f);
        CHECK(state.voltage_memory[1] == 0.0f);
        CHECK(state.pll_pos == 0.0f);
        CHECK(state.phase_vel == 0.0f);
    }

    TEST_CASE("invalid-step-clears-state-and-output") {
        sensorless_state_t state{{1.0f, -2.0f}, {3.0f, -4.0f}, 1.5f, 20.0f};
        const sensorless_config_t config{1000.0f, 1000.0f, 1.58e-3f};
        const sensorless_motor_t motor{0.1f, 0.001f, 7};
        sensorless_input_t input{{1.0f, 2.0f}, {3.0f, 4.0f}, false};
        sensorless_output_t output{1.0f, 2.0f, 3.0f, 4.0f};
        float dt = 0.000125f;
        sensorless_status_t expected = SENSORLESS_CURRENT_MISSING;

        SUBCASE("missing-current") {}
        SUBCASE("stability-boundary") {
            input.current_valid = true;
            dt = 0.0005f;
            expected = SENSORLESS_UNSTABLE_GAIN;
        }
        SUBCASE("gain-error-takes-priority") {
            dt = 0.0005f;
            expected = SENSORLESS_UNSTABLE_GAIN;
        }

        CHECK(sensorless_estimator_update(&state, &config, &motor, &input, dt, &output) == expected);
        CHECK(state.flux[0] == 0.0f);
        CHECK(state.flux[1] == 0.0f);
        CHECK(state.voltage_memory[0] == 0.0f);
        CHECK(state.voltage_memory[1] == 0.0f);
        CHECK(state.pll_pos == 0.0f);
        CHECK(state.phase_vel == 0.0f);
        CHECK(output.phase == 0.0f);
        CHECK(output.pll_pos == 0.0f);
        CHECK(output.phase_vel == 0.0f);
        CHECK(output.vel_estimate == 0.0f);
    }

    TEST_CASE("voltage-is-delayed-and-resistance-inductance-are-applied") {
        sensorless_state_t state{};
        const sensorless_config_t config{0.0f, 0.0f, 1.0f};
        const sensorless_motor_t motor{0.5f, 0.25f, 2};
        sensorless_input_t input{{2.0f, -4.0f}, {3.0f, 5.0f}, true};
        sensorless_output_t output{};

        REQUIRE(sensorless_estimator_update(&state, &config, &motor, &input, 0.125f, &output) == SENSORLESS_OK);
        CHECK(state.flux[0] == doctest::Approx(-0.125f));
        CHECK(state.flux[1] == doctest::Approx(0.25f));
        CHECK(state.voltage_memory[0] == 3.0f);
        CHECK(state.voltage_memory[1] == 5.0f);
        CHECK(output.phase == doctest::Approx(std::atan2(1.25f, -0.625f)).epsilon(0.0003));

        input.voltage[0] = -7.0f;
        input.voltage[1] = 11.0f;
        REQUIRE(sensorless_estimator_update(&state, &config, &motor, &input, 0.125f, &output) == SENSORLESS_OK);
        CHECK(state.flux[0] == doctest::Approx(0.125f));
        CHECK(state.flux[1] == doctest::Approx(1.125f));
        CHECK(state.voltage_memory[0] == -7.0f);
        CHECK(state.voltage_memory[1] == 11.0f);
        CHECK(output.phase == doctest::Approx(std::atan2(2.125f, -0.375f)).epsilon(0.0003));
    }

    TEST_CASE("nonlinear-correction-restores-flux-magnitude") {
        sensorless_state_t state{};
        const sensorless_config_t config{4.0f, 0.0f, 1.0f};
        const sensorless_motor_t motor{0.0f, 0.0f, 1};
        const sensorless_input_t input{{0.0f, 0.0f}, {0.0f, 0.0f}, true};
        sensorless_output_t output{};
        float expected_alpha = 0.0f;
        float expected_beta = 0.0f;

        SUBCASE("weak-flux-increases") {
            state.flux[0] = 0.3f;
            state.flux[1] = 0.4f;
            expected_alpha = 0.345f;
            expected_beta = 0.46f;
        }
        SUBCASE("strong-flux-decreases") {
            state.flux[0] = 1.2f;
            state.flux[1] = 1.6f;
            expected_alpha = 0.48f;
            expected_beta = 0.64f;
        }

        REQUIRE(sensorless_estimator_update(&state, &config, &motor, &input, 0.1f, &output) == SENSORLESS_OK);
        CHECK(state.flux[0] == doctest::Approx(expected_alpha));
        CHECK(state.flux[1] == doctest::Approx(expected_beta));
        CHECK(output.phase == doctest::Approx(std::atan2(0.4f, 0.3f)).epsilon(0.0003));
    }

    TEST_CASE("pll-output-is-distinct-from-flux-angle") {
        constexpr float pi = 3.14159265358979323846f;
        sensorless_state_t state{{0.0f, 1.0f}, {0.0f, 0.0f}, 0.0f, 0.0f};
        const sensorless_config_t config{0.0f, 10.0f, 1.0f};
        const sensorless_motor_t motor{0.0f, 0.0f, 4};
        const sensorless_input_t input{{0.0f, 0.0f}, {0.0f, 0.0f}, true};
        sensorless_output_t output{};

        REQUIRE(sensorless_estimator_update(&state, &config, &motor, &input, 0.01f, &output) == SENSORLESS_OK);
        CHECK(output.phase == doctest::Approx(pi / 2.0f));
        CHECK(output.pll_pos == doctest::Approx(pi / 10.0f));
        CHECK(output.phase_vel == doctest::Approx(pi / 2.0f));
        CHECK(output.vel_estimate == doctest::Approx(0.0625f));
        CHECK(state.pll_pos == output.pll_pos);
        CHECK(state.phase_vel == output.phase_vel);
    }

    TEST_CASE("pll-crosses-phase-boundary-by-shortest-path") {
        const sensorless_config_t config{0.0f, 2.0f, 1.0f};
        const sensorless_motor_t motor{0.0f, 0.0f, 1};
        const sensorless_input_t input{{0.0f, 0.0f}, {0.0f, 0.0f}, true};
        for (float direction : {-1.0f, 1.0f}) {
            sensorless_state_t state{};
            state.pll_pos = direction * 3.1f;
            state.flux[0] = std::cos(-direction * 3.1f);
            state.flux[1] = std::sin(-direction * 3.1f);
            sensorless_output_t output{};
            REQUIRE(sensorless_estimator_update(&state, &config, &motor, &input, 0.1f, &output) == SENSORLESS_OK);
            CHECK(direction * output.phase_vel > 0.0f);
            CHECK(std::abs(output.phase_vel) < 0.1f);
            CHECK(direction * output.pll_pos > 3.12f);
            CHECK(std::abs(output.pll_pos) < 3.141593f);
        }
    }

    TEST_CASE("nonpositive-pole-pairs-retain-one-pair-fallback") {
        const sensorless_config_t config{0.0f, 10.0f, 1.0f};
        const sensorless_input_t input{{0.0f, 0.0f}, {0.0f, 0.0f}, true};
        for (int pole_pairs : {-2, 0, 1}) {
            sensorless_state_t state{{0.0f, 1.0f}, {0.0f, 0.0f}, 0.0f, 0.0f};
            const sensorless_motor_t motor{0.0f, 0.0f, pole_pairs};
            sensorless_output_t output{};
            REQUIRE(sensorless_estimator_update(&state, &config, &motor, &input, 0.01f, &output) == SENSORLESS_OK);
            CHECK(output.vel_estimate == doctest::Approx(0.25f));
        }
    }

    TEST_CASE("recovery-matches-a-fresh-instance") {
        sensorless_state_t recovered{{0.3f, -0.2f}, {1.0f, 2.0f}, 1.0f, 2.0f};
        sensorless_state_t fresh{};
        const sensorless_config_t config{1000.0f, 1000.0f, 1.58e-3f};
        const sensorless_motor_t motor{0.1f, 0.001f, 7};
        sensorless_input_t input{{0.0f, 0.0f}, {0.01f, 0.02f}, false};
        sensorless_output_t actual{};
        sensorless_output_t expected{};
        REQUIRE(sensorless_estimator_update(&recovered, &config, &motor, &input, 0.000125f, &actual) == SENSORLESS_CURRENT_MISSING);

        input.current_valid = true;
        for (int i = 0; i < 20; ++i) {
            REQUIRE(sensorless_estimator_update(&recovered, &config, &motor, &input, 0.000125f, &actual) == SENSORLESS_OK);
            REQUIRE(sensorless_estimator_update(&fresh, &config, &motor, &input, 0.000125f, &expected) == SENSORLESS_OK);
            CHECK(actual.phase == expected.phase);
            CHECK(actual.pll_pos == expected.pll_pos);
            CHECK(actual.phase_vel == expected.phase_vel);
            CHECK(actual.vel_estimate == expected.vel_estimate);
        }
    }
}
