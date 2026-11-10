// 增量编码器：定时器采样与计数差分。
#include "encoder.hpp"
#include <board.h>

void Encoder::sample_incremental() {
    tim_cnt_sample_ = (int16_t)timer_->Instance->CNT;
}

void Encoder::update_incremental(int32_t& delta_enc) {
    //TODO: use count_in_cpr_ instead as shadow_count_ can overflow
    //or use 64 bit
    int16_t delta_enc_16 = (int16_t)tim_cnt_sample_ - (int16_t)shadow_count_;
    delta_enc = (int32_t)delta_enc_16; //sign extend
}
