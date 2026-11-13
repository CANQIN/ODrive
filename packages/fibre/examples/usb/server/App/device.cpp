#include "device.hpp"
#include "autogen/interfaces.hpp"

// 下位机业务与端点绑定：不依赖 socket、桌面系统或 MCU 驱动。
class ExampleDevice final : public ExampleDeviceIntf {
public:
    void set_value(float value) {
        value_ = value;
        ++setter_count_;
    }

    float add(float delta) final { return value_ + delta; }

    float value_ = 1.25f;
    uint32_t setter_count_ = 0;
};

ExampleDevice device;
ExampleDevice& ep_root = device;
#include "autogen/function_stubs.hpp"
#include "autogen/endpoints.hpp"

const fibre::EndpointServer& example::device_endpoints() {
    return fibre::endpoint_server;
}
