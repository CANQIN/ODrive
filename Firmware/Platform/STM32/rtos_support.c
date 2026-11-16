#include "cmsis_os.h"
#include <stdint.h>
#include "Drivers/STM32/stm32_system.h"

// Threads are created by MotorControl/main.cpp, not the generated default task.
osThreadId defaultTaskHandle;
const uint32_t stack_size_default_task = 2048;

// HAL uses TIM14; SysTick belongs to FreeRTOS and the ODrive IRQ counters.
void SysTick_Handler(void) {
    COUNT_IRQ(SysTick_IRQn);
    osSystickHandler();
}
