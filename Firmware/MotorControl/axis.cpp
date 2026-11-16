#include <stdlib.h>
#include <functional>
#include <bitset>
#include "gpio.h"

#include "odrive_main.h"
#include "utils.hpp"
#include "communication/interface_can.hpp"

Axis::Axis(int axis_num,
           uint16_t default_step_gpio_pin,
           uint16_t default_dir_gpio_pin,
           osPriority thread_priority,
           Encoder& encoder,
           SensorlessEstimator& sensorless_estimator,
           Controller& controller,
           Motor& motor,
           TrapezoidalTrajectory& trap,
           Endstop& min_endstop,
           Endstop& max_endstop,
           MechanicalBrake& mechanical_brake)
    : axis_num_(axis_num),
      default_step_gpio_pin_(default_step_gpio_pin),
      default_dir_gpio_pin_(default_dir_gpio_pin),
      thread_priority_(thread_priority),
      encoder_(encoder),
      sensorless_estimator_(sensorless_estimator),
      controller_(controller),
      motor_(motor),
      trap_traj_(trap),
      min_endstop_(min_endstop),
      max_endstop_(max_endstop),
      mechanical_brake_(mechanical_brake)
{
    encoder_.set_event_handler([](void* context, Encoder::Event event) {
        auto& axis = *static_cast<Axis*>(context);
        if (event == Encoder::Event::error) {
            axis.error_ |= ERROR_ENCODER_FAILED;
        } else if (event == Encoder::Event::reference_acquired) {
            if (axis.controller_.config_.anticogging.pre_calibrated) {
                axis.controller_.anticogging_valid_ = true;
            }
        } else if (event == Encoder::Event::configuration_failed) {
            odrv.misconfigured_ = true;
        }
    }, this);
    controller_.axis_ = this;
    motor_.axis_ = this;
    trap_traj_.axis_ = this;
    min_endstop_.axis_ = this;
    max_endstop_.axis_ = this;
    mechanical_brake_.axis_ = this;
}

Axis::LockinConfig_t Axis::default_calibration() {
    Axis::LockinConfig_t config;
    config.current = 10.0f;           // [A]
    config.ramp_time = 0.4f;          // [s]
    config.ramp_distance = 1 * M_PI;  // [rad]
    config.accel = 20.0f;     // [rad/s^2]
    config.vel = 40.0f; // [rad/s]
    config.finish_distance = 100.0f * 2.0f * M_PI;  // [rad]
    config.finish_on_vel = false;
    config.finish_on_distance = true;
    config.finish_on_enc_idx = true;
    return config;
}

Axis::LockinConfig_t Axis::default_sensorless() {
    Axis::LockinConfig_t config;
    config.current = 10.0f;           // [A]
    config.ramp_time = 0.4f;          // [s]
    config.ramp_distance = 1 * M_PI;  // [rad]
    config.accel = 200.0f;     // [rad/s^2]
    config.vel = 400.0f; // [rad/s]
    config.finish_distance = 100.0f;  // [rad]
    config.finish_on_vel = true;
    config.finish_on_distance = false;
    config.finish_on_enc_idx = false;
    return config;
}

static void step_cb_wrapper(void* ctx) {
    reinterpret_cast<Axis*>(ctx)->step_cb();
}

bool Axis::apply_config() {
    config_.parent = this;
    decode_step_dir_pins();
    watchdog_feed();
    return true;
}

void Axis::clear_config() {
    config_ = {};
    config_.step_gpio_pin = default_step_gpio_pin_;
    config_.dir_gpio_pin = default_dir_gpio_pin_;
    config_.can.node_id = axis_num_;
}

static void run_state_machine_loop_wrapper(const void* ctx) {
    auto* axis = static_cast<Axis*>(const_cast<void*>(ctx));
    axis->run_state_machine_loop();
    axis->thread_id_valid_ = false;
}

// @brief Starts run_state_machine_loop in a new thread
void Axis::start_thread() {
    static char thread_name[] = "thread_def";
    const osThreadDef_t thread_def = {
        thread_name, run_state_machine_loop_wrapper, thread_priority_, 0,
        stack_size_ / sizeof(StackType_t)};
    thread_id_ = osThreadCreate(&thread_def, this);
    thread_id_valid_ = true;
}

/**
 * @brief Blocks until at least one complete control loop has been executed.
 */
bool Axis::wait_for_control_iteration() {
    osSignalWait(0x0001, osWaitForever); // this might return instantly
    osSignalWait(0x0001, osWaitForever); // this might be triggered at the
                                         // end of a control loop iteration
                                         // which was started before we entered
                                         // this function
    osSignalWait(0x0001, osWaitForever);
    return true;
}

// step/direction interface
void Axis::step_cb() {
    if (step_dir_active_) {
        dir_gpio_.read() ? ++steps_ : --steps_;
        controller_.input_pos_updated();
    }
}

void Axis::decode_step_dir_pins() {
    step_gpio_ = get_gpio(config_.step_gpio_pin);
    dir_gpio_ = get_gpio(config_.dir_gpio_pin);
}

// @brief (de)activates step/dir input
void Axis::set_step_dir_active(bool active) {
    if (active) {
        // Subscribe to rising edges of the step GPIO
        if (!step_gpio_.subscribe(true, false, step_cb_wrapper, this)) {
            odrv.misconfigured_ = true;
        }

        step_dir_active_ = true;
    } else {
        step_dir_active_ = false;

        // Unsubscribe from step GPIO
        // TODO: if we change the GPIO while the subscription is active and then
        // unsubscribe then the unsubscribe is for the wrong pin.
        step_gpio_.unsubscribe();
    }
}

// @brief Do axis level checks and call subcomponent do_checks
// Returns true if everything is ok.
bool Axis::do_checks(uint32_t timestamp) {
    // Sub-components should use set_error which will propegate to this error_
    motor_.effective_current_lim();
    motor_.do_checks(timestamp);

    // Check for endstop presses
    if (min_endstop_.config_.enabled && min_endstop_.rose() && !(current_state_ == AXIS_STATE_HOMING)) {
        error_ |= ERROR_MIN_ENDSTOP_PRESSED;
    } else if (max_endstop_.config_.enabled && max_endstop_.rose() && !(current_state_ == AXIS_STATE_HOMING)) {
        error_ |= ERROR_MAX_ENDSTOP_PRESSED;
    }

    return check_for_errors();
}

// @brief Feed the watchdog to prevent watchdog timeouts.
void Axis::watchdog_feed() {
    watchdog_current_value_ = get_watchdog_reset();
}

// @brief Check the watchdog timer for expiration. Also sets the watchdog error bit if expired.
bool Axis::watchdog_check() {
    if (!config_.enable_watchdog) return true;

    // explicit check here to ensure that we don't underflow back to UINT32_MAX
    if (watchdog_current_value_ > 0) {
        watchdog_current_value_--;
        return true;
    } else {
        error_ |= ERROR_WATCHDOG_TIMER_EXPIRED;
        return false;
    }
}

bool Axis::run_lockin_spin(const LockinConfig_t &lockin_config, bool remain_armed,
        std::function<bool(bool)> loop_cb) {
    CRITICAL_SECTION() {
        // Reset state variables
        open_loop_controller_.Idq_setpoint_ = {0.0f, 0.0f};
        open_loop_controller_.Vdq_setpoint_ = {0.0f, 0.0f};
        open_loop_controller_.phase_ = 0.0f;
        open_loop_controller_.phase_vel_ = 0.0f;

        open_loop_controller_.max_current_ramp_ = lockin_config.current / lockin_config.ramp_time;
        open_loop_controller_.max_voltage_ramp_ = lockin_config.current / lockin_config.ramp_time;
        open_loop_controller_.max_phase_vel_ramp_ = lockin_config.accel;
        open_loop_controller_.target_current_ = motor_.config_.motor_type != Motor::MOTOR_TYPE_GIMBAL ? lockin_config.current : 0.0f;
        open_loop_controller_.target_voltage_ = motor_.config_.motor_type != Motor::MOTOR_TYPE_GIMBAL ? 0.0f : lockin_config.current;
        open_loop_controller_.target_vel_ = lockin_config.vel;
        open_loop_controller_.total_distance_ = 0.0f;

        motor_.current_control_.enable_current_control_src_ = motor_.config_.motor_type != Motor::MOTOR_TYPE_GIMBAL;
        motor_.current_control_.Idq_setpoint_src_.connect_to(&open_loop_controller_.Idq_setpoint_);
        motor_.current_control_.Vdq_setpoint_src_.connect_to(&open_loop_controller_.Vdq_setpoint_);

        motor_.current_control_.phase_src_.connect_to(&open_loop_controller_.phase_);
        acim_estimator_.rotor_phase_src_.connect_to(&open_loop_controller_.phase_);
        
        motor_.phase_vel_src_.connect_to(&open_loop_controller_.phase_vel_);
        motor_.current_control_.phase_vel_src_.connect_to(&open_loop_controller_.phase_vel_);
        acim_estimator_.rotor_phase_vel_src_.connect_to(&open_loop_controller_.phase_vel_);
    }
    wait_for_control_iteration();

    motor_.arm(&motor_.current_control_);

    bool subscribed_to_idx_once = false;
    bool success = false;
    float dir = lockin_config.vel >= 0.0f ? 1.0f : -1.0f;

    while ((requested_state_ == AXIS_STATE_UNDEFINED) && motor_.is_armed_) {
        bool reached_target_vel = std::abs(open_loop_controller_.phase_vel_.any().value_or(0.0f) - lockin_config.vel) <= std::numeric_limits<float>::epsilon();
        bool reached_target_dist = open_loop_controller_.total_distance_.any().value_or(0.0f) * dir >= lockin_config.finish_distance * dir;

        // Check if terminal condition is reached
        bool terminal_condition = (reached_target_vel && lockin_config.finish_on_vel)
                               || (reached_target_dist && lockin_config.finish_on_distance)
                               || (encoder_.index_found_ && lockin_config.finish_on_enc_idx);
        if (terminal_condition) {
            success = true;
            break;
        }

        // Activate index pin as soon as target velocity was reached. This is
        // to avoid hitting the index from the wrong direction.
        if (reached_target_vel && !encoder_.index_found_ && !subscribed_to_idx_once) {
            encoder_.set_idx_subscribe(true);
            subscribed_to_idx_once = true;
        }

        if (loop_cb)
            if (!loop_cb(reached_target_vel))
                break;

        // TODO: use new sync function instead
        asm volatile ("" ::: "memory");
        osDelay(1);
    }

    if (!success || !remain_armed) {
        motor_.disarm();
    }

    return success;
}


bool Axis::start_closed_loop_control() {
    bool sensorless_mode = config_.enable_sensorless_mode;

    if (sensorless_mode) {
        // TODO: restart if desired
        if (!run_lockin_spin(config_.sensorless_ramp, true)) {
            return false;
        }
    }

    // Hook up the data paths between the components
    CRITICAL_SECTION() {
        if (sensorless_mode) {
            controller_.pos_estimate_linear_src_.disconnect();
            controller_.pos_estimate_circular_src_.disconnect();
            controller_.pos_wrap_src_.disconnect();
            controller_.vel_estimate_src_.connect_to(&sensorless_estimator_.vel_estimate_);
        } else if (controller_.config_.load_encoder_axis < AXIS_COUNT) {
            Axis* ax = &axes[controller_.config_.load_encoder_axis];
            controller_.pos_estimate_circular_src_.connect_to(&ax->encoder_.pos_circular_);
            controller_.pos_wrap_src_.connect_to(&controller_.config_.circular_setpoint_range);
            controller_.pos_estimate_linear_src_.connect_to(&ax->encoder_.pos_estimate_);
            controller_.vel_estimate_src_.connect_to(&ax->encoder_.vel_estimate_);
        } else {
            controller_.pos_estimate_circular_src_.disconnect();
            controller_.pos_estimate_linear_src_.disconnect();
            controller_.pos_wrap_src_.disconnect();
            controller_.vel_estimate_src_.disconnect();
            controller_.set_error(Controller::ERROR_INVALID_LOAD_ENCODER);
            return false;
        }

        // To avoid any transient on startup, we intialize the setpoint to be the current position
        controller_.control_mode_updated();
        controller_.input_pos_updated();

        // Avoid integrator windup issues
        controller_.vel_integrator_torque_ = 0.0f;

        motor_.torque_setpoint_src_.connect_to(&controller_.torque_output_);
        motor_.direction_ = sensorless_mode ? 1.0f : encoder_.config_.direction;

        motor_.current_control_.enable_current_control_src_ = motor_.config_.motor_type != Motor::MOTOR_TYPE_GIMBAL;
        motor_.current_control_.Idq_setpoint_src_.connect_to(&motor_.Idq_setpoint_);
        motor_.current_control_.Vdq_setpoint_src_.connect_to(&motor_.Vdq_setpoint_);

        bool is_acim = motor_.config_.motor_type == Motor::MOTOR_TYPE_ACIM;
        // phase
        OutputPort<float>* phase_src = sensorless_mode ? &sensorless_estimator_.phase_ : &encoder_.phase_;
        acim_estimator_.rotor_phase_src_.connect_to(phase_src);
        OutputPort<float>* stator_phase_src = is_acim ? &acim_estimator_.stator_phase_ : phase_src;
        motor_.current_control_.phase_src_.connect_to(stator_phase_src);
        // phase vel
        OutputPort<float>* phase_vel_src = sensorless_mode ? &sensorless_estimator_.phase_vel_ : &encoder_.phase_vel_;
        acim_estimator_.rotor_phase_vel_src_.connect_to(phase_vel_src);
        OutputPort<float>* stator_phase_vel_src = is_acim ? &acim_estimator_.stator_phase_vel_ : phase_vel_src;
        motor_.phase_vel_src_.connect_to(stator_phase_vel_src);
        motor_.current_control_.phase_vel_src_.connect_to(stator_phase_vel_src);
        
        if (sensorless_mode) {
            // Make the final velocity of the loĉk-in spin the setpoint of the
            // closed loop controller to allow for smooth transition.
            float vel = config_.sensorless_ramp.vel / (2.0f * M_PI * motor_.config_.pole_pairs);
            controller_.input_vel_ = vel;
            controller_.vel_setpoint_ = vel;
        }
    }

    // In sensorless mode the motor is already armed.
    if (!motor_.is_armed_) {
        wait_for_control_iteration();
        motor_.arm(&motor_.current_control_);
    }

    return true;
}

bool Axis::stop_closed_loop_control() {
    motor_.disarm();
    return check_for_errors();
}

bool Axis::run_closed_loop_control_loop() {
    start_closed_loop_control();
    set_step_dir_active(config_.enable_step_dir);

    while ((requested_state_ == AXIS_STATE_UNDEFINED) && motor_.is_armed_) {
        osDelay(1);
    }

    set_step_dir_active(config_.enable_step_dir && config_.step_dir_always_on);
    stop_closed_loop_control();

    return check_for_errors();
}


// Slowly drive in the negative direction at homing_speed until the min endstop is pressed
// When pressed, set the linear count to the offset (default 0), and then go to position 0
bool Axis::run_homing() {
    // TODO: theoretically this check should be inside the update loop,
    // otherwise someone could disable the endstop while homing is in progress.
    if (!min_endstop_.config_.enabled) {
        return error_ |= ERROR_HOMING_WITHOUT_ENDSTOP, false;
    }

    controller_.config_.control_mode = Controller::CONTROL_MODE_VELOCITY_CONTROL;
    controller_.config_.input_mode = Controller::INPUT_MODE_VEL_RAMP;

    controller_.input_pos_ = 0.0f;
    controller_.input_pos_updated();
    controller_.input_vel_ = -controller_.config_.homing_speed;
    controller_.input_torque_ = 0.0f;

    homing_.is_homed = false;

    error_ &= ~ERROR_MIN_ENDSTOP_PRESSED;

    bool done = false;

    start_closed_loop_control();

    // Driving toward the endstop
    while ((requested_state_ == AXIS_STATE_UNDEFINED) && motor_.is_armed_ && !(done = min_endstop_.get_state())) {
        osDelay(1);
    }

    stop_closed_loop_control();
    
    controller_.input_vel_ = 0.0f;

    if (!done) {
        return false;
    }

    error_ &= ~ERROR_MIN_ENDSTOP_PRESSED; // clear this error since we deliberately drove into the endstop

    std::optional<float> pos_estimate_local = encoder_.pos_estimate_.any();
    if (pos_estimate_local == std::nullopt || !pos_estimate_local.has_value()){
        return error_ |= ERROR_UNKNOWN_POSITION, false;
    }
    
    controller_.config_.control_mode = Controller::CONTROL_MODE_POSITION_CONTROL;
    controller_.config_.input_mode = Controller::INPUT_MODE_TRAP_TRAJ;

    // Initialize closed loop control, and then set the desired location.
    start_closed_loop_control();
    
    controller_.input_pos_ = pos_estimate_local.value() + min_endstop_.config_.offset;
    controller_.pos_setpoint_ = pos_estimate_local.value();
    controller_.vel_setpoint_ = 0.0f;
    controller_.input_pos_updated();

    // Synchronization issue.  Ensure trajectory_done is false prior to the while loop, so that
    // the controller has time to run move_to_pos() on the next update()
    controller_.trajectory_done_ = false; 
    
    while ((requested_state_ == AXIS_STATE_UNDEFINED) && motor_.is_armed_ && !(done = controller_.trajectory_done_)) {
        osDelay(1);
    }

    stop_closed_loop_control();

    if (!done) {
        return false;
    }

    // Set the current position to 0, the target to zero, and make sure we're path planning from 0 to 0
    encoder_.set_linear_count(0); 
    const auto load_encoder_axis = controller_.config_.load_encoder_axis;
    if(load_encoder_axis != axis_num_ && load_encoder_axis < AXIS_COUNT) {
        axes[load_encoder_axis].encoder_.set_linear_count(0);
    }
    controller_.input_pos_ = 0.0f;
    controller_.pos_setpoint_ = 0.0f;
    controller_.vel_setpoint_ = 0.0f;
    controller_.input_pos_updated();

    // Force encoder estimate to update
    osDelay(1);

    homing_.is_homed = true;

    return check_for_errors();
}

bool Axis::run_idle_loop() {
    last_drv_fault_ = motor_.gate_driver_.get_error();
    mechanical_brake_.engage();
    set_step_dir_active(config_.enable_step_dir && config_.step_dir_always_on);
    while (requested_state_ == AXIS_STATE_UNDEFINED) {
        motor_.setup();
        osDelay(1);
    }
    return check_for_errors();
}

// Infinite loop that does calibration and enters main control loop as appropriate
void Axis::run_state_machine_loop() {
    for (;;) {
        // Load the task chain if a specific request is pending
        if (requested_state_ != AXIS_STATE_UNDEFINED) {
            size_t pos = 0;
            if (requested_state_ == AXIS_STATE_STARTUP_SEQUENCE) {
                if (config_.startup_motor_calibration)
                    task_chain_[pos++] = AXIS_STATE_MOTOR_CALIBRATION;
                if (config_.startup_encoder_index_search && encoder_.config_.use_index)
                    task_chain_[pos++] = AXIS_STATE_ENCODER_INDEX_SEARCH;
                if (config_.startup_encoder_offset_calibration)
                    task_chain_[pos++] = AXIS_STATE_ENCODER_OFFSET_CALIBRATION;
                if (config_.startup_homing)
                    task_chain_[pos++] = AXIS_STATE_HOMING;
                if (config_.startup_closed_loop_control)
                    task_chain_[pos++] = AXIS_STATE_CLOSED_LOOP_CONTROL;
                task_chain_[pos++] = AXIS_STATE_IDLE;
            } else if (requested_state_ == AXIS_STATE_FULL_CALIBRATION_SEQUENCE) {
                task_chain_[pos++] = AXIS_STATE_MOTOR_CALIBRATION;
                if (encoder_.config_.mode == ODriveIntf::EncoderIntf::MODE_HALL)
                    task_chain_[pos++] = AXIS_STATE_ENCODER_HALL_POLARITY_CALIBRATION;
                if (encoder_.config_.use_index)
                    task_chain_[pos++] = AXIS_STATE_ENCODER_INDEX_SEARCH;
                task_chain_[pos++] = AXIS_STATE_ENCODER_OFFSET_CALIBRATION;
                task_chain_[pos++] = AXIS_STATE_IDLE;
            } else if (requested_state_ != AXIS_STATE_UNDEFINED) {
                task_chain_[pos++] = requested_state_;
                task_chain_[pos++] = AXIS_STATE_IDLE;
            }
            task_chain_[pos++] = AXIS_STATE_UNDEFINED;  // TODO: bounds checking
            requested_state_ = AXIS_STATE_UNDEFINED;
            // Auto-clear any invalid state error
            error_ &= ~ERROR_INVALID_STATE;
        }

        // Note that current_state is a reference to task_chain_[0]

        // Run the specified state
        // Handlers should exit if requested_state != AXIS_STATE_UNDEFINED
        bool status;
        switch (current_state_) {
            case AXIS_STATE_MOTOR_CALIBRATION: {
                // These error checks are a hacky way to force legacy behavior
                // when an error is raised. TODO: remove this when we overhaul
                // the error architecture
                // (https://github.com/madcowswe/ODrive/issues/526).
                //if (odrv.any_error())
                //    goto invalid_state_label;
                status = motor_.run_calibration();
            } break;

            case AXIS_STATE_ENCODER_INDEX_SEARCH: {
                //if (odrv.any_error())
                //    goto invalid_state_label;
                if (!motor_.is_calibrated_)
                    goto invalid_state_label;

                status = run_index_search();
            } break;

            case AXIS_STATE_ENCODER_DIR_FIND: {
                //if (odrv.any_error())
                //    goto invalid_state_label;
                if (!motor_.is_calibrated_)
                    goto invalid_state_label;

                status = run_direction_find();
                // Help facilitate encoder.is_ready without reboot
                if (status)
                    encoder_.apply_config(motor_.config_.motor_type);
            } break;

            case AXIS_STATE_ENCODER_HALL_POLARITY_CALIBRATION: {
                if (!motor_.is_calibrated_)
                    goto invalid_state_label;

                status = run_hall_polarity_calibration();
            } break;

            case AXIS_STATE_ENCODER_HALL_PHASE_CALIBRATION: {
                if (!motor_.is_calibrated_)
                    goto invalid_state_label;

                if (!encoder_.config_.hall_polarity_calibrated) {
                    encoder_.set_error(ODriveIntf::EncoderIntf::ERROR_HALL_NOT_CALIBRATED_YET);
                    goto invalid_state_label;
                }

                status = run_hall_phase_calibration();
            } break;

            case AXIS_STATE_HOMING: {
                Controller::ControlMode stored_control_mode = controller_.config_.control_mode;
                Controller::InputMode stored_input_mode = controller_.config_.input_mode;
                
                status = run_homing();

                controller_.config_.control_mode = stored_control_mode;
                controller_.config_.input_mode = stored_input_mode;
            } break;

            case AXIS_STATE_ENCODER_OFFSET_CALIBRATION: {
                //if (odrv.any_error())
                //    goto invalid_state_label;
                if (!motor_.is_calibrated_)
                    goto invalid_state_label;
                status = run_offset_calibration();
            } break;

            case AXIS_STATE_LOCKIN_SPIN: {
                //if (odrv.any_error())
                //    goto invalid_state_label;
                if (!motor_.is_calibrated_ || encoder_.config_.direction==0)
                    goto invalid_state_label;
                status = run_lockin_spin(config_.general_lockin, false);
            } break;

            case AXIS_STATE_CLOSED_LOOP_CONTROL: {
                //if (odrv.any_error())
                //    goto invalid_state_label;
                if (!motor_.is_calibrated_ || (encoder_.config_.direction==0 && !config_.enable_sensorless_mode))
                    goto invalid_state_label;
                watchdog_feed();
                status = run_closed_loop_control_loop();
            } break;

            case AXIS_STATE_IDLE: {
                run_idle_loop();
                status = true;
            } break;

            default:
            invalid_state_label:
                error_ |= ERROR_INVALID_STATE;
                status = false;  // this will set the state to idle
                break;
        }

        // If the state failed, go to idle, else advance task chain
        if (!status) {
            std::fill(task_chain_.begin(), task_chain_.end(), AXIS_STATE_UNDEFINED);
            current_state_ = AXIS_STATE_IDLE;
        } else {
            std::rotate(task_chain_.begin(), task_chain_.begin() + 1, task_chain_.end());
            task_chain_.back() = AXIS_STATE_UNDEFINED;
        }
    }
}


// 编码器校准由轴编排，类型模块只提供采样与解码结果。
bool Axis::run_index_search() {
    encoder_.config_.use_index = true;
    encoder_.index_found_ = false;
    encoder_.set_idx_subscribe();

    bool success = run_lockin_spin(config_.calibration_lockin, false);
    return success;
}

bool Axis::run_direction_find() {
    int32_t init_enc_val = encoder_.shadow_count_;

    Axis::LockinConfig_t lockin_config = config_.calibration_lockin;
    lockin_config.finish_distance = lockin_config.vel * 3.0f; // run for 3 seconds
    lockin_config.finish_on_distance = true;
    lockin_config.finish_on_enc_idx = false;
    lockin_config.finish_on_vel = false;
    bool success = run_lockin_spin(lockin_config, false);

    if (success) {
        // Check response and direction
        if (encoder_.shadow_count_ > init_enc_val + 8) {
            // motor same dir as encoder
            encoder_.config_.direction = 1;
        } else if (encoder_.shadow_count_ < init_enc_val - 8) {
            // motor opposite dir as encoder
            encoder_.config_.direction = -1;
        } else {
            encoder_.config_.direction = 0;
        }
    }

    return success;
}

bool Axis::run_hall_polarity_calibration() {
    Axis::LockinConfig_t lockin_config = config_.calibration_lockin;
    lockin_config.finish_distance = lockin_config.vel * 3.0f; // run for 3 seconds
    lockin_config.finish_on_distance = true;
    lockin_config.finish_on_enc_idx = false;
    lockin_config.finish_on_vel = false;

    auto loop_cb = [this](bool const_vel) {
        if (const_vel)
            sample_hall_states_ = true;
        // No need to cancel early
        return true;
    };

    encoder_.config_.hall_polarity_calibrated = false;
    states_seen_count_.fill(0);
    bool success = run_lockin_spin(lockin_config, false, loop_cb);
    sample_hall_states_ = false;

    if (success) {
        std::bitset<8> state_seen;
        std::bitset<8> state_confirmed;
        for (int i = 0; i < 8; i++) {
            if (states_seen_count_[i] > 0)
                state_seen[i] = true;
            if (states_seen_count_[i] > 50)
                state_confirmed[i] = true;
        }
        if (!(state_seen == state_confirmed)) {
            encoder_.set_error(Encoder::ERROR_ILLEGAL_HALL_STATE);
            return false;
        }

        // Hall effect sensors can be arranged at 60 or 120 electrical degrees.
        // Out of 8 possible states, 120 and 60 deg arrangements each miss 2 states.
        // ODrive assumes 120 deg separation - if a 60 deg setup is used, it can
        // be converted to 120 deg states by flipping the polarity of one sensor.
        uint8_t states = state_seen.to_ulong();
        uint8_t hall_polarity = 0;
        auto flip_detect = [](uint8_t states, unsigned int idx)->bool {
            return (~states & 0xFF) == (1<<(0+idx) | 1<<(7-idx));
        };
        if (flip_detect(states, 0)) {
            hall_polarity = 0b000;
        } else if (flip_detect(states, 1)) {
            hall_polarity = 0b001;
        } else if (flip_detect(states, 2)) {
            hall_polarity = 0b010;
        } else if (flip_detect(states, 3)) {
            hall_polarity = 0b100;
        } else {
            encoder_.set_error(Encoder::ERROR_ILLEGAL_HALL_STATE);
            return false;
        }
        encoder_.config_.hall_polarity = hall_polarity;
        encoder_.config_.hall_polarity_calibrated = true;
    }

    return success;
}

bool Axis::run_hall_phase_calibration() {
    Axis::LockinConfig_t lockin_config = config_.calibration_lockin;
    lockin_config.finish_distance = lockin_config.vel * 30.0f; // run for 30 seconds
    lockin_config.finish_on_distance = true;
    lockin_config.finish_on_enc_idx = false;
    lockin_config.finish_on_vel = false;

    auto loop_cb = [this](bool const_vel) {
        if (const_vel)
            sample_hall_phase_ = true;
        // No need to cancel early
        return true;
    };

    // TODO: There is a race condition here with the execution in Axis::update_encoder.
    // We should evaluate making thread execution synchronous with the control loops
    // at least optionally.
    // Perhaps the new loop_sync feature will give a loose timing guarantee that may be sufficient
    calibrate_hall_phase_ = true;
    encoder_.config_.hall_edge_phcnt.fill(0.0f);
    hall_phase_calib_seen_count_.fill(0);
    bool success = run_lockin_spin(lockin_config, false, loop_cb);
    if (encoder_.error_ & Encoder::ERROR_ILLEGAL_HALL_STATE)
        success = false;

    if (success) {
        // Check deltas to dicern rotation direction
        float delta_phase = 0.0f;
        for (int i = 0; i < 6; i++) {
            int next_i = (i == 5) ? 0 : i+1;
            delta_phase += wrap_pm_pi(encoder_.config_.hall_edge_phcnt[next_i] - encoder_.config_.hall_edge_phcnt[i]);
        }
        // Correct reverse rotation
        if (delta_phase < 0.0f) {
            encoder_.config_.direction = -1;
            for (int i = 0; i < 6; i++)
                encoder_.config_.hall_edge_phcnt[i] = wrap_pm_pi(-encoder_.config_.hall_edge_phcnt[i]);
        } else {
            encoder_.config_.direction = 1;
        }
        // Normalize edge timing to 1st edge in sequence, and change units to counts
        float offset = encoder_.config_.hall_edge_phcnt[0];
        for (int i = 0; i < 6; i++) {
            float& phcnt = encoder_.config_.hall_edge_phcnt[i];
            phcnt = fmodf_pos((6.0f / (2.0f * M_PI)) * (phcnt - offset), 6.0f);
        }
    } else {
        encoder_.config_.hall_edge_phcnt = Encoder::hall_edge_defaults;
    }

    calibrate_hall_phase_ = false;
    return success;
}

bool Axis::run_offset_calibration() {
    const float start_lock_duration = 1.0f;

    // Require index found if enabled
    if (encoder_.config_.use_index && !encoder_.index_found_) {
        encoder_.set_error(Encoder::ERROR_INDEX_NOT_FOUND_YET);
        return false;
    }

    if (encoder_.config_.mode == Encoder::MODE_HALL && !encoder_.config_.hall_polarity_calibrated) {
        encoder_.set_error(Encoder::ERROR_HALL_NOT_CALIBRATED_YET);
        return false;
    }

    // We use encoder_.shadow_count_ to do the calibration, but the offset is used by encoder_.count_in_cpr_
    // Therefore we have to sync them for calibration
    encoder_.shadow_count_ = encoder_.count_in_cpr_;

    CRITICAL_SECTION() {
        // Reset state variables
        open_loop_controller_.Idq_setpoint_ = {0.0f, 0.0f};
        open_loop_controller_.Vdq_setpoint_ = {0.0f, 0.0f};
        open_loop_controller_.phase_ = 0.0f;
        open_loop_controller_.phase_vel_ = 0.0f;

        float max_current_ramp = motor_.config_.calibration_current / start_lock_duration * 2.0f;
        open_loop_controller_.max_current_ramp_ = max_current_ramp;
        open_loop_controller_.max_voltage_ramp_ = max_current_ramp;
        open_loop_controller_.max_phase_vel_ramp_ = INFINITY;
        open_loop_controller_.target_current_ = motor_.config_.motor_type != Motor::MOTOR_TYPE_GIMBAL ? motor_.config_.calibration_current : 0.0f;
        open_loop_controller_.target_voltage_ = motor_.config_.motor_type != Motor::MOTOR_TYPE_GIMBAL ? 0.0f : motor_.config_.calibration_current;
        open_loop_controller_.target_vel_ = 0.0f;
        open_loop_controller_.total_distance_ = 0.0f;
        open_loop_controller_.phase_ = open_loop_controller_.initial_phase_ = wrap_pm_pi(0 - encoder_.config_.calib_scan_distance / 2.0f);

        motor_.current_control_.enable_current_control_src_ = (motor_.config_.motor_type != Motor::MOTOR_TYPE_GIMBAL);
        motor_.current_control_.Idq_setpoint_src_.connect_to(&open_loop_controller_.Idq_setpoint_);
        motor_.current_control_.Vdq_setpoint_src_.connect_to(&open_loop_controller_.Vdq_setpoint_);

        motor_.current_control_.phase_src_.connect_to(&open_loop_controller_.phase_);
        acim_estimator_.rotor_phase_src_.connect_to(&open_loop_controller_.phase_);

        motor_.phase_vel_src_.connect_to(&open_loop_controller_.phase_vel_);
        motor_.current_control_.phase_vel_src_.connect_to(&open_loop_controller_.phase_vel_);
        acim_estimator_.rotor_phase_vel_src_.connect_to(&open_loop_controller_.phase_vel_);
    }
    wait_for_control_iteration();

    motor_.arm(&motor_.current_control_);

    // go to start position of forward scan for start_lock_duration to get ready to scan
    for (size_t i = 0; i < (size_t)(start_lock_duration * 1000.0f); ++i) {
        if (!motor_.is_armed_) {
            return false; // TODO: return "disarmed" error code
        }
        if (requested_state_ != Axis::AXIS_STATE_UNDEFINED) {
            motor_.disarm();
            return false; // TODO: return "aborted" error code
        }
        osDelay(1);
    }


    int32_t init_enc_val = encoder_.shadow_count_;
    uint32_t num_steps = 0;
    int64_t encvaluesum = 0;

    CRITICAL_SECTION() {
        open_loop_controller_.target_vel_ = encoder_.config_.calib_scan_omega;
        open_loop_controller_.total_distance_ = 0.0f;
    }

    // scan forward
    while ((requested_state_ == Axis::AXIS_STATE_UNDEFINED) && motor_.is_armed_) {
        bool reached_target_dist = open_loop_controller_.total_distance_.any().value_or(-INFINITY) >= encoder_.config_.calib_scan_distance;
        if (reached_target_dist) {
            break;
        }
        encvaluesum += encoder_.shadow_count_;
        num_steps++;
        osDelay(1);
    }

    // Check response and direction
    if (encoder_.shadow_count_ > init_enc_val + 8) {
        // motor same dir as encoder
        encoder_.config_.direction = 1;
    } else if (encoder_.shadow_count_ < init_enc_val - 8) {
        // motor opposite dir as encoder
        encoder_.config_.direction = -1;
    } else {
        // Encoder response error
        encoder_.set_error(Encoder::ERROR_NO_RESPONSE);
        motor_.disarm();
        return false;
    }

    // Check CPR
    float elec_rad_per_enc = motor_.config_.pole_pairs * 2 * M_PI * (1.0f / (float)(encoder_.config_.cpr));
    float expected_encoder_delta = encoder_.config_.calib_scan_distance / elec_rad_per_enc;
    encoder_.calib_scan_response_ = std::abs(encoder_.shadow_count_ - init_enc_val);
    if (std::abs(encoder_.calib_scan_response_ - expected_encoder_delta) / expected_encoder_delta > encoder_.config_.calib_range) {
        encoder_.set_error(Encoder::ERROR_CPR_POLEPAIRS_MISMATCH);
        motor_.disarm();
        return false;
    }

    CRITICAL_SECTION() {
        open_loop_controller_.target_vel_ = -encoder_.config_.calib_scan_omega;
    }

    // scan backwards
    while ((requested_state_ == Axis::AXIS_STATE_UNDEFINED) && motor_.is_armed_) {
        bool reached_target_dist = open_loop_controller_.total_distance_.any().value_or(INFINITY) <= 0.0f;
        if (reached_target_dist) {
            break;
        }
        encvaluesum += encoder_.shadow_count_;
        num_steps++;
        osDelay(1);
    }

    // Motor disarmed because of an error
    if (!motor_.is_armed_) {
        return false;
    }

    motor_.disarm();

    encoder_.config_.phase_offset = encvaluesum / num_steps;
    int32_t residual = encvaluesum - ((int64_t)encoder_.config_.phase_offset * (int64_t)num_steps);
    encoder_.config_.phase_offset_float = (float)residual / (float)num_steps + 0.5f;  // add 0.5 to center-align state to phase

    encoder_.is_ready_ = true;
    return true;
}

bool Axis::update_encoder() {
    const Encoder::RuntimeContext context{
        motor_.config_.pole_pairs,
        controller_.config_.circular_setpoint_range,
        motor_.config_.motor_type,
        calibrate_hall_phase_};
    bool success = encoder_.update(context);
    if (encoder_.mode_ == Encoder::MODE_HALL) {
        if (sample_hall_states_) {
            states_seen_count_[encoder_.hall_state_]++;
        }
        if (calibrate_hall_phase_ && encoder_.hall_count_.has_value()) {
            int32_t hall_cnt = encoder_.hall_count_.value();
            if (sample_hall_phase_ && last_hall_cnt_.has_value()) {
                int mod_hall_cnt = mod(hall_cnt - last_hall_cnt_.value(), 6);
                size_t edge_idx;
                if (mod_hall_cnt == 0) { goto skip; }
                else if (mod_hall_cnt == 1) {
                    edge_idx = hall_cnt;
                } else if (mod_hall_cnt == 5) {
                    edge_idx = last_hall_cnt_.value();
                } else {
                    encoder_.set_error(Encoder::ERROR_ILLEGAL_HALL_STATE);
                    return false;
                }

                auto maybe_phase = open_loop_controller_.phase_.any();
                if (maybe_phase) {
                    float phase = maybe_phase.value();
                    hall_phase_calib_seen_count_[edge_idx]++;
                    float& edge_phase = encoder_.config_.hall_edge_phcnt[edge_idx];
                    if (hall_phase_calib_seen_count_[edge_idx] == 1)
                        edge_phase = phase;
                    else {
                        edge_phase += (phase - edge_phase) / hall_phase_calib_seen_count_[edge_idx];
                        edge_phase = wrap_pm_pi(edge_phase);
                    }
                }
            }
        skip:
            last_hall_cnt_ = hall_cnt;
            return true;
        }
    }
    return success;
}
