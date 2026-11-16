include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board-version.cmake")

set(odrive_board_definitions
    HW_VERSION_MAJOR=3
    HW_VERSION_MINOR=6
    HW_VERSION_VOLTAGE=56)

set(odrive_root_interface ODrive3)
set(odrive_cpu_options -mcpu=cortex-m4 -mfpu=fpv4-sp-d16)
list(APPEND odrive_board_definitions STM32F405xx ARM_MATH_CM4 FPU_FPV4)
set(odrive_board_dir "${CMAKE_CURRENT_SOURCE_DIR}/Board/v3")
set(odrive_hal_dir "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/STM32F4xx_HAL_Driver")
set(odrive_port_dir "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/FreeRTOS/Source/portable/GCC/ARM_CM4F")
set(odrive_math_library arm_cortexM4lf_math)
set(odrive_linker_script "${odrive_board_dir}/STM32F405RGTx_FLASH.ld")
set(odrive_board_sources
    ${odrive_board_dir}/startup_stm32f405xx.s
    ${odrive_port_dir}/port.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/DRV8301/drv8301.cpp
    ${odrive_board_dir}/board.cpp)
set(_board_units stm32f4xx_hal_timebase_TIM tim dma freertos main usbd_conf
    spi usart usbd_cdc_if adc stm32f4xx_hal_msp usbd_desc stm32f4xx_it
    usb_device can system_stm32f4xx gpio i2c)
set(_hal_prefix stm32f4xx_)
set(_hal_units hal hal_adc hal_adc_ex hal_can hal_cortex hal_dma hal_dma_ex
    hal_flash hal_flash_ex hal_flash_ramfunc hal_gpio hal_i2c hal_i2c_ex
    hal_pcd hal_pcd_ex hal_pwr hal_pwr_ex hal_rcc hal_rcc_ex hal_spi hal_tim
    hal_tim_ex hal_uart ll_usb)

foreach(unit IN LISTS _board_units)
    list(APPEND odrive_board_sources "${odrive_board_dir}/Src/${unit}.c")
endforeach()
foreach(unit IN LISTS _hal_units)
    list(APPEND odrive_hal_sources "${odrive_hal_dir}/Src/${_hal_prefix}${unit}.c")
endforeach()
set(odrive_board_includes
    ${odrive_board_dir}/Inc ${odrive_hal_dir}/Inc ${odrive_port_dir})
