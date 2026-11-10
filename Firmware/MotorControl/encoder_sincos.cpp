// SinCos 编码器：模拟采样与相位解码。
#include "encoder.hpp"
#include <board.h>
#include "low_level.h"

void Encoder::sample_sincos() {
    sincos_sample_s_ = get_adc_relative_voltage(get_gpio(config_.sincos_gpio_pin_sin)) - 0.5f;
    sincos_sample_c_ = get_adc_relative_voltage(get_gpio(config_.sincos_gpio_pin_cos)) - 0.5f;
}

void Encoder::update_sincos(int32_t& delta_enc) {
    float phase = fast_atan2(sincos_sample_s_, sincos_sample_c_);
    int fake_count = (int)(1000.0f * phase);
    //CPR = 6283 = 2pi * 1k

    delta_enc = fake_count - count_in_cpr_;
    delta_enc = mod(delta_enc, 6283);
    if (delta_enc > 6283/2)
        delta_enc -= 6283;
}
