# Fibre C++ 核心

本目录提供平台无关的协议、对象客户端、序列化与日志接口。服务端注入业务端点，两端均由应用提供收发；客户端的事件循环和通道发现也由应用提供。

构建与接入见 [包说明](../README.md)，示例见 [examples](../examples/README.md)。公共头位于 [include/fibre](../include/fibre/)，CMake 提供 `fibre::server`、`fibre::client` 和 `fibre::c`。
