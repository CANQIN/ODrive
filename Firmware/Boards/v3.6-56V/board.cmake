set(odrive_cpu_options -mcpu=cortex-m4 -mfpu=fpv4-sp-d16)
list(APPEND odrive_board_definitions ARM_MATH_CM4 FPU_FPV4 USER_VECT_TAB_ADDRESS)
set(odrive_generated_units main gpio dma adc can usart spi tim
    stm32f4xx_it stm32f4xx_hal_msp stm32f4xx_hal_timebase_tim system_stm32f4xx)
set(odrive_board_sources
    "${odrive_board_dir}/board.cpp"
    "${odrive_board_dir}/cubemx_hooks.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Drivers/DRV8301/drv8301.cpp")
set(odrive_board_includes "${odrive_board_dir}/Inc")
include("${CMAKE_CURRENT_LIST_DIR}/../../Platform/STM32F4/platform.cmake")
