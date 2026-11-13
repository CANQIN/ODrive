# MCU UART 示例

`server` 是 STM32F405RG 固件，`client/python` 是电脑 pyserial 客户端。串口是字节流，两端使用 Fibre 帧头、长度和 CRC 完成分帧。

## 硬件与 CubeMX

- 芯片：STM32F405RGT6，8 MHz HSE，主频 168 MHz。
- UART4：PA0 = TX，PA1 = RX，115200 baud，8 数据位、无校验、1 停止位、无硬件流控。
- 使用 3.3 V TTL 串口适配器：电脑适配器 RX 接 PA0、TX 接 PA1，GND 共地。
- `.ioc`：`server/fibre_uart.ioc`，UART4 异步模式并开启中断，PA13/PA14 保留 SWD。

CubeMX 生成外设初始化、中断入口、HAL/CMSIS、启动文件、链接脚本和 CMake。`App/fibre_app.cpp` 是 UART 与 Fibre 的适配：接收中断把字节写入 512 字节环形缓冲区，发送中断记录完成状态，主循环交付 Fibre 回调。示例采用中断收发，未使用 RTOS 或 DMA。

## 构建 MCU

以下命令从 Fibre 包根目录执行，使用 CMake 3.22+、Ninja、Arm GNU Toolchain 和 Python。先将 `arm-none-eabi-gcc` 等工具加入 `PATH`：

```sh
python -m pip install -r codegen/requirements.txt
cmake -S examples/uart/server -B build/examples/uart-server -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake"
cmake --build build/examples/uart-server
```

输出 `build/examples/uart-server/fibre_uart.elf`，可通过 SWD 烧录。CubeMX 重新生成后，`main.c` 的 `USER CODE` 区域保留 `fibre_app_init()` / `fibre_app_poll()` 调用，`App/` 保留业务和适配。

## 构建并运行电脑客户端

```sh
python -m pip install -r examples/uart/client/python/requirements.txt
cmake -S examples/uart/client/python -B build/examples/uart-python -G Ninja -DSERIAL_PORT=COM5
cmake --build build/examples/uart-python
cmake --build build/examples/uart-python --target run
```

把 `COM5` 换成实际串口；Linux 可用 `/dev/ttyUSB0`。客户端构建的是原生 Fibre 动态库，运行目标自动配置绑定路径。Python 的串口适配负责收发字节并转换成逻辑包，业务调用不依赖串口 API。

取消接收只结束当前 Fibre 读请求，HAL 继续使用适配器自有的接收字节；取消发送先中止 HAL 发送，再释放 Fibre 缓冲区。环形缓冲区溢出或 HAL 错误进入 `Error_Handler()`，便于调试定位。

验证范围：已完成固件交叉编译和电脑端构建；实际串口收发需要连接实物验证。
