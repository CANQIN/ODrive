# Fibre 通信示例

示例先按通信方式分组，每组再分 `client` 和 `server`。每一端都有自己的适配代码及 CMake 工程，不引用其他示例的业务或驱动文件。

| 示例 | server 下位机 | client 上位机 | Fibre 通道 |
| --- | --- | --- | --- |
| [udp](udp/) | 电脑模拟设备 | C++、Python | 整包，一个 UDP 数据报一个逻辑包 |
| [tcp](tcp/) | 电脑模拟设备 | C++、Python | 字节流，由 Fibre 分帧 |
| [usb](usb/README.md) | STM32F405RG，CubeMX 工程 | Python / PyUSB | 自定义 USB Bulk，整包 |
| [uart](uart/README.md) | STM32F405RG，CubeMX 工程 | Python / pyserial | 串口字节流，由 Fibre 分帧 |

```text
examples/
  udp/
    server/                  业务、桌面适配、独立 CMake 工程
    client/cpp/              C++ 应用及自己的 UDP 适配
    client/python/           Python 应用、UDP 适配及原生库构建
  tcp/
    server/                  业务、桌面适配、独立 CMake 工程
    client/cpp/
    client/python/
  usb/
    server/
      fibre_usb.ioc          CubeMX USB FS 外设配置
      Core/ Drivers/ cmake/  CubeMX 生成的工程和驱动
      ThirdParty/            ST USB Device Core（应用管理）
      App/                   业务、Fibre 适配、自定义 Bulk 类
    client/python/           PyUSB 整包收发
  uart/
    server/
      fibre_uart.ioc         CubeMX UART4 配置
      Core/ Drivers/ cmake/  CubeMX 生成的工程和驱动
      App/                   业务、Fibre 适配、UART 中断收发
    client/python/           pyserial 收发和流分帧
```

所有示例暴露相同业务：可写浮点属性 `value`、只读 `setter_count` 和 `add(delta)`。客户端通过端点 0 读取接口 JSON，运行时发现端点编号，不需要服务端 YAML 或生成头文件。

## 本机 UDP / TCP

下面的命令从 Fibre 包根目录执行，需要 CMake、Ninja、C++17 编译器和 Python。服务端构建时安装接口生成器依赖；C++ 客户端不需要 Python：

```sh
python -m pip install -r codegen/requirements.txt
cmake -S examples/udp/server -B build/examples/udp-server -G Ninja
cmake --build build/examples/udp-server
cmake -S examples/udp/client/cpp -B build/examples/udp-cpp -G Ninja
cmake --build build/examples/udp-cpp
cmake -S examples/udp/client/python -B build/examples/udp-python -G Ninja
cmake --build build/examples/udp-python
```

先在一个终端运行服务端，再在另一个终端运行任意客户端：

```sh
./build/examples/udp-server/fibre_server
./build/examples/udp-cpp/fibre_client
# 或运行 Python 客户端
cmake --build build/examples/udp-python --target run
```

Windows 的程序名可加 `.exe`。TCP 的构建、运行方式相同，将路径中的 `udp` 换成 `tcp`。协议已由工程目录确定，程序不再接收 `udp|tcp` 选择参数。

两组都使用本机 `127.0.0.1:39000`，不需要 MCU。一次服务一个客户端；客户端运行结束可换另一种语言再连接，TCP 服务端会重新接受连接。服务端按 `Ctrl+C` 退出。首次运行客户端会把 `value` 从 `1.25` 改为 `3.5`，检查 setter 计数增加一次，并验证 `add(2)` 返回 `5.5`。

Python 工程的 CMake 构建原生动态库，`run` 自动设置 `PYTHONPATH` 和 `FIBRE_LIBRARY_PATH`，无需安装包内 Python 绑定。多 Python 环境时，可在配置命令中添加 `-DPython3_EXECUTABLE=<Python完整路径>`。UDP 没有额外重传机制；网络示例的调用或收发等待上限为 5 秒。

## MCU 与电脑的边界

USB、UART 的 `server` 是实际 MCU 工程，不是桌面模拟。它们由 CubeMX 6.18.1 和 STM32CubeF4 1.28.3 生成，包含启动文件、链接脚本、所需 HAL/CMSIS 源码和 `.ioc`，构建不依赖 ODrive 固件目录或本机 CubeMX 安装目录。

CubeMX 管理时钟、引脚、外设、驱动和工程基础；`App/` 管理 Fibre 业务与适配。业务定义只在服务端，电脑端分别通过 PyUSB 和 pyserial 连接设备。USB 使用自定义 Bulk 类，CubeMX 只生成 USB FS PCD 外设层；USB Device Core、描述符和类注册由应用接入。USB Device Core 放在 `ThirdParty/`，避免 CubeMX 重新生成时清理其管理的 `Middlewares/` 目录。

| 层 | 文件 / 职责 |
| --- | --- |
| 业务 | `device.yaml`、`device.cpp`、`device.hpp`，提供 `device_endpoints()` |
| Fibre 协议 | USB / UDP 用 `LegacyProtocolPacketBased`；UART / TCP 用 `LegacyProtocolStreamBased` |
| MCU 适配 | `App/fibre_app.cpp` 实现 `AsyncStreamSource` / `AsyncStreamSink` |
| 硬件 | `Core/` 外设初始化、中断入口以及 `Drivers/` HAL |
| 调度 | 中断记录数据和完成事件，`fibre_app_poll()` 在主循环调用 Fibre 回调 |

适配时需要实现接收、发送、取消，并报告实际完成长度或断开/错误状态。同一方向只保留一个请求；缓冲区及回调上下文在完成前保持有效，通知前清除回调，允许协议立即提交下一次请求。USB 接收缓冲区未消费时不重新挂接 OUT 端点，由 USB NAK 提供反压；UART 使用自有接收环形缓冲区，溢出或 HAL 错误进入 `Error_Handler()`。

重新生成时只修改 `.ioc`，应用调用放在 `main.c` 的 `USER CODE` 区域，业务和适配放在 `App/`。顶层 CMake 是 CubeMX 仅首次生成的文件，保留 `include(App/fibre.cmake)`；不要把应用代码写入会被覆盖的 `cmake/stm32cubemx/CMakeLists.txt`。

`App/fibre.cmake` 为端点类型查询启用 RTTI，并在构建目录生成兼容 GCC 9 的链接脚本副本，原始 CubeMX 链接脚本保留。核心库仍然不依赖 STM32 或操作系统；平台代码及第三方驱动只属于可选示例。ST HAL、CMSIS、USB Device Core 使用各自目录内的许可证。

复制任意工程时，带上该端目录，再用 `-DFIBRE_SOURCE_DIR=<Fibre包目录>` 指向 SDK。MCU 接线、构建和电脑端运行分别见 USB、UART 说明。
