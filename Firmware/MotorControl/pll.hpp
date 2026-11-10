#pragma once

// 位置单位由调用方确定，速度单位须为同一位置单位每秒。
struct PllState {
    float position;
    float velocity;
};

struct PllGains {
    float kp;
    float ki;
};

// 输入带宽，返回对应的比例和积分增益。
inline PllGains pll_gains_from_bandwidth(float bandwidth) {
    const float kp = 2.0f * bandwidth;
    const float ki = 0.25f * (kp * kp);
    return {kp, ki};
}

// 输入上一状态和以秒为单位的 dt，返回预测状态。
inline PllState pll_predict(const PllState& previous, float dt) {
    return {previous.position + dt * previous.velocity, previous.velocity};
}

// 输入预测状态、同位置单位的测量误差、增益和秒数 dt，返回校正状态。
inline PllState pll_correct(const PllState& predicted, float measurement_error,
                            const PllGains& gains, float dt) {
    return {predicted.position + dt * gains.kp * measurement_error,
            predicted.velocity + dt * gains.ki * measurement_error};
}
