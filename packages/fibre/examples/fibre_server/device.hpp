#pragma once

#include <fibre/legacy_protocol.hpp>

namespace example {

// 业务层提供端点处理入口，由桌面或 MCU 的通信代码注入协议实例。
const fibre::EndpointServer& device_endpoints();

} // namespace example
