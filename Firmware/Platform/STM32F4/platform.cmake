# STM32F4 + Cortex-M4F platform. A different MCU family supplies its own platform
# port; board profiles select clocks, pins and peripherals through CubeMX.
set(odrive_hal_dir "${odrive_cube_dir}/Drivers/STM32F4xx_HAL_Driver")
set(odrive_rtos_dir "${odrive_cube_dir}/Middlewares/Third_Party/FreeRTOS/Source")
set(odrive_usb_dir "${odrive_cube_dir}/Middlewares/ST/STM32_USB_Device_Library")
set(odrive_port_dir "${odrive_rtos_dir}/portable/GCC/ARM_CM4F")
set(odrive_platform_dir "${CMAKE_CURRENT_LIST_DIR}/../STM32")
set(odrive_dsp_library "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/CMSIS_DSP/Lib/libarm_cortexM4lf_math.a")
set(odrive_cmsis_includes "${odrive_cube_dir}/Drivers/CMSIS/Include"
    "${odrive_cube_dir}/Drivers/CMSIS/Device/ST/${odrive_cmsis_device}/Include")
list(APPEND odrive_board_sources
    "${odrive_cube_dir}/${odrive_startup}"
    "${odrive_port_dir}/port.c"
    "${odrive_platform_dir}/rtos_support.c"
    "${CMAKE_CURRENT_LIST_DIR}/faults.c"
    "${odrive_platform_dir}/USB/Src/usb_ll_adapter.c"
    "${odrive_cube_dir}/USB_DEVICE/App/usb_device.c"
    "${odrive_cube_dir}/USB_DEVICE/Target/usbd_conf.c")
foreach(unit IN LISTS odrive_generated_units)
    list(APPEND odrive_board_sources "${odrive_cube_dir}/Core/Src/${unit}.c")
endforeach()
foreach(unit usbd_desc usbd_cdc_if usbd_cdc)
    list(APPEND odrive_board_sources "${odrive_platform_dir}/USB/Src/${unit}.c")
endforeach()
set(_hal_units hal hal_adc hal_adc_ex hal_can hal_cortex hal_dma hal_dma_ex
    hal_flash hal_flash_ex hal_flash_ramfunc hal_gpio
    hal_pcd hal_pcd_ex hal_pwr hal_pwr_ex hal_rcc hal_rcc_ex hal_spi hal_tim
    hal_tim_ex hal_uart ll_usb)
foreach(unit IN LISTS _hal_units)
    list(APPEND odrive_hal_sources "${odrive_hal_dir}/Src/stm32f4xx_${unit}.c")
endforeach()
list(APPEND odrive_board_includes "${odrive_platform_dir}/USB/Inc"
    "${odrive_cube_dir}/Core/Inc" "${odrive_cube_dir}/USB_DEVICE/App"
    "${odrive_cube_dir}/USB_DEVICE/Target" "${odrive_hal_dir}/Inc" "${odrive_port_dir}")
# F4 CubeMX has no FIFO USER section. Wrap its LL init to allocate the native
# endpoint before the generated device startup connects to the USB host.
set_source_files_properties("${odrive_cube_dir}/USB_DEVICE/Target/usbd_conf.c" PROPERTIES
    COMPILE_DEFINITIONS "USBD_LL_Init=odrive_cubemx_usb_ll_init")
