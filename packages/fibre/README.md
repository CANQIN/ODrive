# Fibre 平台无关包

包含 MCU 服务端、上位机客户端、C ABI、Python 绑定和接口生成器，保持 Fibre v0.1 线协议。整个目录可以复制到其他工程独立构建。

核心只提供协议与抽象接口，不包含 USB、串口、TCP 或操作系统事件循环实现。`examples/` 按 UDP、TCP、USB、UART 分组，包含桌面示例和 STM32 CubeMX 示例，不参与核心库构建或安装。包内没有预编译库；交叉构建配置与硬件驱动只存在于 MCU 示例中。C++ 核心依赖标准 C++17；Python 绑定依赖标准库。动态库格式和符号导出由调用方工具链及 CMake 生成，协议代码不选择操作系统。

## 需要应用实现什么

| 能力 | MCU / C++ 应用 | Python 应用 |
| --- | --- | --- |
| 收发 | `AsyncStreamSource` / `AsyncStreamSink` | `transport.read_packet()` / `write_packet()` |
| 关闭或取消 | 完成回调返回取消/断开状态 | 取消收发协程，`transport.close()` 释放资源 |
| 调度 | 主循环、任务队列或外部 `EventLoop` | 绑定使用标准 asyncio 调度，应用回调在该线程执行 |
| 设备发现 | 直接交付通道，或外部 `ChannelDiscoverer` | 把已连接的包通道传给 `Domain` |
| 业务对象 | YAML + 对应 C++ 对象，注入 `EndpointServer` | 从设备接口描述发现属性和函数 |
| 日志输出 | 可选 `set_log_sink()` 回调 | 通用 Logger 或应用自己的日志 |

使用哪种通信方式，就在应用侧实现对应收发；核心不加载后端，也不识别设备型号和系统类型。字节流 MCU 通道可使用 `LegacyProtocolStreamBased`；交给客户端 Domain 的通道需要保留逻辑包边界。

## 目录

```text
fibre/
  CMakeLists.txt        两端库、C ABI 和安装
  cmake/               包导出与接口生成函数
  include/fibre/       公共接口、编解码和日志回调
  src/                 协议、客户端、C ABI 实现及私有辅助头
  codegen/             Python 接口生成器及依赖
  codegen/templates/   接口生成模板
  bindings/python/     Python 绑定包
  examples/udp/        UDP 整包示例：server、client/cpp、client/python
  examples/tcp/        TCP 字节流示例：server、client/cpp、client/python
  examples/usb/        MCU USB Bulk 示例：CubeMX server、PyUSB client
  examples/uart/       MCU 串口示例：CubeMX server、pyserial client
```

## 构建

需要 CMake 3.20、C++17 编译器。需要生成接口时，安装 `codegen/requirements.txt` 中的 Python 依赖。

```sh
python -m pip install -r codegen/requirements.txt
cmake -S . -B build
cmake --build build
cmake --install build --prefix install
```

按[示例说明](examples/README.md)选择通信方式，再分别构建该组的服务端和客户端。UDP、TCP 可在本机演示；USB、UART 提供实际 MCU 工程。所有示例演示相同的对象发现、属性读写、setter 副作用和函数调用，各自构建，不随核心库默认构建。

| CMake 目标 | 内容 |
| --- | --- |
| `fibre::server` | MCU 服务端静态库，默认无堆分配 |
| `fibre::client` | C++ 客户端静态库，允许堆分配 |
| `fibre::c` | 供其他语言调用的 C ABI 动态库 |

两种角色宏会影响内部类型布局，服务端和客户端程序分别链接相应目标，不把不同角色配置的两套库链接到同一进程。

## MCU 接入

```cmake
set(FIBRE_BUILD_CLIENT OFF CACHE BOOL "" FORCE)
add_subdirectory(path/to/fibre fibre-build)
target_link_libraries(firmware PRIVATE fibre::server)
fibre_generate_interfaces(firmware
    DEFINITIONS "${CMAKE_CURRENT_SOURCE_DIR}/device.yaml"
    ROOT Device)
```

也可安装后使用 `find_package(fibre CONFIG REQUIRED)`。在一个翻译单元包含生成接口、实现业务对象、绑定 `ep_root`，再包含 `function_stubs.hpp` 和 `endpoints.hpp`。

```cpp
fibre::LegacyProtocolStreamBased protocol(&rx, &tx, &fibre::endpoint_server);
protocol.start({});
```

`rx`、`tx` 由应用实现；接口实现和缓冲区在异步完成前须有效。`examples/usb/server/App/` 和 `examples/uart/server/App/` 提供完整 MCU 接入，业务定义、协议适配和 CubeMX 生成代码分开管理。适配契约见[示例说明](examples/README.md#mcu-与电脑的边界)。

## C/C++ 客户端接入

应用实现 `EventLoop`，调用 `fibre::open()` 并创建 Domain；直接调用 `Domain::add_channels()`，或通过 `Context::register_backend()` 注册自己管理的通道发现器。C ABI 对应入口是 `libfibre_open()`、`libfibre_open_domain()`、`libfibre_add_channels()` 和 `libfibre_register_backend()`。

注册的 C++ 适配对象由应用管理生命周期。先停止发现和收发，再关闭 Domain 和上下文。事件源标识、事件位和外部调度行为由适配器定义，核心不操作系统文件描述符。

日志默认关闭。需要日志时用 `FIBRE_LOG_VERBOSITY` 设置编译级别，并调用 `<fibre/logging.hpp>` 的 `set_log_sink(sink, context)`；格式化使用标准字符串流，资源受限的 MCU 可保持级别 0。

## Python 接入

```sh
python -m pip install ./bindings/python
```

应用先设置 `FIBRE_LIBRARY_PATH` 为自行构建的 C ABI 动态库完整路径，再导入绑定。运行库搜索目录等工具链部署工作由应用负责。

```python
import fibre

with fibre.Domain(transport=my_packets) as domain:
    device = domain.discover_one()
    device.value = 3.5
    print(device.value)
    print(device.add(2.0))
```

`my_packets` 提供 `mtu`、`async read_packet(capacity)`、`async write_packet(packet)` 和 `async close()`。协程在 Fibre 的 asyncio 线程中运行。各组 `examples/<通信方式>/client/python/` 提供实际收发实现，具体 I/O 和第三方传输依赖留在应用中。

详见 [Python 说明](bindings/python/README.md) 与 [示例说明](examples/README.md)。

## ODrive 应用边界

ODrive 的 STM32 驱动和 RTOS 调度位于包外 `Firmware/communication/`。原生库选择、PyUSB 适配和交互终端位于包外 `tools/odrive/pyfibre/`。本包的构建和安装不读取这些应用目录。

ODrive 通过 CMake 的 `fibre::server` 引入 MCU 核心，并使用自己的 `odrive-interface.yaml`。接口生成直接调用本包的 `codegen/interface_generator.py`。
