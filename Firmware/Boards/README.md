# 板型配置与 CubeMX

当前仅实现 `v3.6-56V`。每个板型单独拥有 CubeMX 工程和板级适配，CMake 从 `*/board.json` 与 `board.cmake` 自动发现可用板型，无需修改中央版本判断。

```text
Boards/<板型>/
  board.json          MCU、工具版本、生成入口、硬件版本
  board.cmake         CPU 编译选项、生成源清单、平台选择
  board.cpp / Inc/    电机、编码器、通信句柄和运行时引脚映射
  cubemx_hooks.c      生成代码的少量 USER CODE 回调
  verify.py          本板时钟、PWM、DMA、中断、存储布局约束
  *.ld / *.svd       链接布局、调试描述
  CubeMX/            本板 .ioc 与实际生成的基础代码、HAL、RTOS、USB Core
Platform/STM32/      共享 RTOS 支持、ODrive USB 协议类和描述符
Platform/STM32F4/    Cortex-M4F 平台构建与故障处理
```

TIM5 生成计时基准，PWM 输入通道由运行时映射配置。

静态时钟、引脚、外设参数、MSP、DMA/NVIC、普通中断转发由 CubeMX 生成。`board.cpp` 负责组合应用对象、调用生成的初始化、应用运行时参数及电机控制专用中断，不复制 CAN/UART 的初始化实现。生成文件仅允许在 `USER CODE` 区域添加钩子，厂商库保持原样。

USB Device 初始化、PCD/MSP、中断和回调使用生成代码。共享层保留 ODrive 的 CDC + native 类、WinUSB 描述符及数据接口；底层初始化包装器调用生成实现后，通过板型钩子补充分配 EP3 FIFO。生成器未提供 FIFO USER CODE 区域，因此这里保留小包装器。

## 重新生成与构建

在仓库根目录执行（CubeMX 6.18.1、STM32CubeF4 1.28.3）：

```powershell
./scripts/regenerate-cubemx.ps1 -Board v3.6-56V -CubeMXPath D:/STM32CubeMX/STM32CubeMX.exe
cmake -S . -B build -DODRIVE_BOARD_VERSION=v3.6-56V -DODRIVE_BUILD_TESTS=ON -DODRIVE_STRICT=ON
cmake --build build --target firmware tests --parallel 8
```

也可用 `STM32CUBEMX_PATH` 指定 CubeMX 安装目录或程序。脚本生成所选板型工程，保留 USER CODE，裁掉不用的厂商内容和默认应用骨架，验证生成结果；日志位于 `build/cubemx/<板型>`。普通构建使用已纳入版本管理的生成源码，无需安装 CubeMX。

## 新增板型

1. 创建 `Boards/<新板型>`；配置 `board.json` 与 `board.cmake`。同系列可参考 v3.6，但应核对每一处引脚、定时器、模拟采样和功率级参数。
2. 使用 CubeMX 创建本板 `.ioc`，固定工具/固件包版本，启用分离的外设 `.c/.h`、Keep User Code，复制完整库。设置时钟、外设、DMA 与 NVIC。
3. 在 `board.cmake` 列出 `odrive_generated_units`、`odrive_board_sources`、`odrive_board_includes`、`odrive_board_definitions`、`odrive_cpu_options`，并包含适用的平台 CMake。平台提供 HAL/RTOS/USB 路径、CMSIS include、DSP 库和平台源文件。
4. 实现 `board.h`/`board.cpp` 的应用硬件接口（电机/编码器、GPIO 功能、通信句柄、同步采样与控制中断），提供链接脚本、`Inc/nvm_config.h`（擦除扇区布局）和 `verify.py`。常量来自本板 IOC；不要在共享目录按硬件版本堆叠条件分支。
5. 用 `-Board <新板型>` 生成，使用独立构建目录 `-B build/<新板型>` 并指定 `-DODRIVE_BOARD_VERSION=<新板型>`。运行 `python scripts/check-cubemx.py --all`、严格编译与主机测试，然后上板验证。

Windows 构建入口也支持 `./build.ps1 -BoardVersion <新板型> -BuildDirectory build/<新板型>`，板型合法性统一由 CMake 检查。VSCode IntelliSense 读取编译数据库；使用独立构建目录时同步调整 `compileCommands`，调试配置中的 ELF 路径与 SVD 也应匹配所选板型。

现有共享平台针对 STM32F4/Cortex-M4F。跨 MCU 系列、ADC/PWM 架构或门驱动的板子仍需移植平台与电机采样路径，不能仅更换 `.ioc`。主机测试不覆盖真实电气时序；部署前需验证 USB/UART/CAN、编码器 SPI、PWM/ADC 同步、配置保存重启及电机运行。

配置存储格式已升级为版本 2：旧配置因字段布局变化而不再加载，更新固件后需重新配置并保存。GPIO 模式值 8 留空，其他已有模式值保持不变。
