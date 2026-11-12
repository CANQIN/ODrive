# Fibre 两端独立工程

示例按“MCU 下位机业务”和“电脑上位机应用”组织。`fibre_server` 的业务代码可移植到 MCU，附带的 `desktop/` 用于没有 MCU 时在电脑上模拟下位机；`fibre_client` 提供 C++ 和 Python 上位机。各工程独立构建，可以通过本地 UDP 或 TCP 通信。

## 目录与职责

```text
examples/
  fibre_server/
    CMakeLists.txt           构建下位机业务及桌面模拟程序
    device.yaml              设备属性和函数定义，只属于服务端
    device.hpp               向平台通信代码提供 device_endpoints() 入口
    device.cpp               可移植业务对象和生成端点绑定
    desktop/                 无 MCU 时的模拟平台，移植 MCU 时替换
      main.cpp               监听、协议启动和桌面主循环
      socket.hpp             模拟平台的系统适配
      udp_transport.hpp      接收请求，向请求方回复
      tcp_transport.hpp      监听并接收客户端连接
  fibre_client/
    cpp/
      CMakeLists.txt         构建 C++ 客户端
      main.cpp               传输选择、对象发现、属性读写和函数调用
      transports/            电脑上位机自己的适配
        socket.hpp           上位机的系统适配
        udp_transport.hpp    向设备发送请求，接收设备响应
        tcp_transport.hpp    主动连接设备，收发字节流
    python/
      CMakeLists.txt         构建 C ABI 动态库，提供 run_udp / run_tcp
      main.py                Python 对象发现和业务操作
      transports.py          Python UDP、TCP 收发与流帧适配
```

| 内容 | 服务端 | C++ 客户端 | Python 客户端 |
| --- | --- | --- | --- |
| Fibre 依赖 | `fibre::server` | `fibre::client` | Python 绑定 + `fibre::c` 动态库 |
| 业务定义 | `device.yaml`、`device.cpp`、`device.hpp` | 属性名、函数名、参数类型 | 属性名、函数名、参数类型 |
| 生成文件 | 构建目录内的 `interfaces.hpp`、`function_stubs.hpp`、`endpoints.hpp` | 无 | 无 |
| 接口生成器 | 构建时需要 Python 及 codegen 依赖 | 不需要 | 不需要 |
| 传输适配 | 自身 `desktop/`，迁移 MCU 时替换 | 自身 `transports/` | 自身 `transports.py` |

两端只共同依赖 Fibre 包，没有跨工程的适配代码引用。示例保留少量相似实现，让每一端的文件和职责完整可见：服务端处理请求与监听，客户端发起请求与连接。它们以后可以分别接入 MCU 网络栈和电脑系统，不必维护一套混合平台适配。

两种客户端都从服务端端点 0 读取 JSON 接口描述，运行时获得端点编号，不读取服务端 YAML，不包含服务端生成头文件。客户端业务与传输各自独立：增加一种传输时，不用改设备属性和函数调用。

## 分别构建

以下命令从 Fibre 包根目录执行，终端中需能使用 CMake 3.20+、Ninja、C++17 编译器和 Python。服务端构建前安装接口生成器依赖：

```sh
python -m pip install -r codegen/requirements.txt

cmake -S examples/fibre_server -B build/server -G Ninja
cmake --build build/server

cmake -S examples/fibre_client/cpp -B build/client-cpp -G Ninja
cmake --build build/client-cpp

cmake -S examples/fibre_client/python -B build/client-python -G Ninja
cmake --build build/client-python
```

只使用一种客户端时，只需构建对应工程。C++ 客户端不需要 Python 环境；Python 工程中的 CMake 编译的是原生库，Python 源码直接运行。多个 Python 环境并存时，给相应配置命令添加 `-DPython3_EXECUTABLE=<Python完整路径>`。

每个工程各自编译所需的 Fibre 库，不依赖另一个工程的构建产物。示例不随包核心或 ODrive 默认构建；示例统一通过各自的 CMake 工程构建。

## 运行四种组合

先在一个终端启动所选传输的服务端，再在另一个终端运行客户端：

| 组合 | 服务端命令 | 客户端命令 |
| --- | --- | --- |
| UDP + C++ | `./build/server/fibre_server udp` | `./build/client-cpp/fibre_client udp` |
| UDP + Python | `./build/server/fibre_server udp` | `cmake --build build/client-python --target run_udp` |
| TCP + C++ | `./build/server/fibre_server tcp` | `./build/client-cpp/fibre_client tcp` |
| TCP + Python | `./build/server/fibre_server tcp` | `cmake --build build/client-python --target run_tcp` |

Windows 的程序名可加 `.exe`。Python 的两个运行目标自动设置包内绑定的 `PYTHONPATH` 和本轮构建的 `FIBRE_LIBRARY_PATH`，无需安装 Python 发行包或手工定位 DLL。

两种传输都监听本机 `127.0.0.1:39000`。服务端按 `Ctrl+C` 退出，一次处理一个客户端；客户端退出后可以换另一种语言再次连接。TCP 服务端在连接断开后重新等待连接，业务对象保持当前属性值和累计 setter 次数。

新启动服务端后的首次客户端输出如下（Python / C++ 的业务操作一致）：

```text
已通过 UDP 发现设备：127.0.0.1:39000
属性写入：旧值=1.25，新值=3.50，setter 次数=0 -> 1
函数调用：add(2.0)=5.50
```

客户端会检查属性值、函数结果和 setter 计数，失败退出码非零。C++ 的发现及每次调用最多等待 5 秒；Python 的 socket 收发等待上限为 5 秒。UDP 示例没有额外重传机制。

## UDP 与 TCP 展示的适配边界

```text
服务端业务对象 ← Fibre 端点处理 ← 协议实例 ← 收发适配 ← UDP / TCP
客户端业务调用 → Fibre 发现与调用 → 协议实例 → 收发适配 → UDP / TCP
```

| 传输类型 | C++ 协议实例 | 适配器负责的内容 |
| --- | --- | --- |
| UDP：有包边界 | `LegacyProtocolPacketBased` | 一个数据报对应一个完整 Fibre 逻辑包 |
| TCP：字节流 | `LegacyProtocolStreamBased` | 交付实际收到/发出的字节数，允许短读、短写 |

TCP 中的帧头、长度、CRC 和流分帧由 Fibre 的 `PacketWrapper` / `PacketUnwrapper` 处理，不能把一次 `recv()` 当成一个 Fibre 包。

Python 的 `Domain(transport=...)` 接收的是包通道，因此 `transports.py` 的 TCP 实现会复用包内流解析器进行解帧，发送时补上同样的帧头和 CRC；UDP 实现直接交付数据报。两种实现对业务都只暴露 `mtu`、`read_packet()`、`write_packet()`、`close()`。

平台代码分别放在服务端 `desktop/`、C++ 客户端 `transports/` 和 Python 客户端 `transports.py`。C++ 核心不包含 socket、Windows 或 POSIX 后端，这些示例适配不会安装进核心库。

## 如何迁移到 MCU

保留服务端根目录的 `device.yaml`、`device.cpp`、`device.hpp`，继续生成接口并链接 `fibre::server`。`ExampleDevice`、`ep_root` 和生成端点绑定全部封装在 `device.cpp` 中，不需要搬运 `desktop/` 或客户端的任何文件。

MCU 工程自行提供驱动适配、启动文件、链接脚本和交叉工具链，在通信任务中启动协议。例如字节流通道：

```cpp
#include "device.hpp"

// rx、tx 由 MCU 驱动适配提供，在协议运行期间持续有效。
fibre::LegacyProtocolStreamBased protocol(&rx, &tx, &example::device_endpoints());
protocol.start({});
```

整包通道则使用 `LegacyProtocolPacketBased`，传入同一个 `device_endpoints()` 和通道 MTU。桌面模拟入口 `desktop/main.cpp` 展示了两种协议的初始化；其 `poll()` 循环在 MCU 上替换为主循环或通信任务中的驱动事件处理。

适配类实现 `AsyncStreamSource` 和 `AsyncStreamSink`，具体职责如下：

| 接口 / 时机 | MCU 应用需要实现什么 |
| --- | --- |
| `start_read(buffer, handle, completer)` | 保存接收缓冲区和完成回调，启动驱动接收 |
| 接收完成 | 调用回调，返回状态及实际写入末尾地址 |
| `start_write(buffer, handle, completer)` | 保存发送缓冲区和完成回调，启动驱动发送 |
| 发送完成 | 调用回调，返回状态及实际发送末尾地址 |
| `cancel_read()` / `cancel_write()` | 停止对应操作，完成回调返回 `kStreamCancelled` |
| 断开或错误 | 完成回调返回 `kStreamClosed` 或 `kStreamError` |
| 主循环 / 通信任务 | 驱动协议处理；驱动中断只投递事件，在同一执行上下文调用 Fibre |

同一方向同时只保留一个请求。缓冲区和回调上下文在完成回调之前必须有效，完成或取消只通知一次；通知前清除保存的回调，允许协议在回调内立即提交下一次请求。服务端的 `desktop/tcp_transport.hpp` 展示了挂起请求、完成、短读写和取消的实现。

MCU 使用 UDP 时接入网络栈的整包收发，继续用包协议；使用 TCP、UART、USB CDC 等字节流时，接入字节收发并用流协议。串口 DMA 完成事件与这里 socket 就绪事件的来源不同，交给 Fibre 的缓冲区和回调契约相同，因此无需另设 UART 占位模板。

复制示例到其他项目时，只需复制所需工程目录（`fibre_server`、`fibre_client/cpp` 或 `fibre_client/python`），再用 `-DFIBRE_SOURCE_DIR=<Fibre包目录>` 指向独立 SDK，不需要复制另一端或公共示例目录。
