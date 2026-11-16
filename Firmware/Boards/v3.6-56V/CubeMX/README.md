此目录由 STM32CubeMX 6.18.1 + STM32CubeF4 1.28.3 实际生成，目标为 ODrive v3.6-56V / STM32F405RGT6。

配置入口为 `CubeMX.ioc`。从仓库根目录执行 `./scripts/regenerate-cubemx.ps1 -Board v3.6-56V`，生成本板的基础工程。生成器保留 USER CODE，裁掉不用的厂商内容及默认任务、USB 类/描述符骨架，运行本板配置校验。普通构建使用已纳入版本管理的源码，无需安装 CubeMX。

时钟、外设初始化、MSP、普通中断转发、HAL tick、USB Device/PCD、启动文件和 HAL/RTOS/USB Core 均使用生成实现。`main.c` 的入口通过编译宏重命名，实际启动顺序和运行时配置由板级 `board.cpp` 与应用管理。只在 USER CODE 区域添加钩子，不修改厂商库。

完整目录边界、新板接入流程及共享引脚说明见 `Firmware/Boards/README.md`；配置约束见本板 `verify.py`。不要用生成的 Makefile 或默认链接脚本烧录：固件必须由仓库 CMake 构建，以保留 NVM/CCM 布局。
