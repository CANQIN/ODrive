保留迁移前的 CMSIS-DSP 1.5.3 头文件和 `libarm_cortexM4lf_math.a`，用于 Cortex-M4 单精度硬浮点。文件按原样从旧 `ThirdParty/CMSIS` 迁入，避免此次 CubeMX/HAL/RTOS 迁移同时改变电机控制数学实现。

CMSIS 内核与 STM32F405 器件头来自 `Firmware/Boards/<板型>/CubeMX/Drivers/CMSIS`。再生成脚本不会覆盖本目录。后续升级 DSP 时应同时更换匹配的头文件与库，并单独验证数值行为。
