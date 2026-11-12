set(_odrive_boards
    v3.1 v3.2 v3.3 v3.4-24V v3.4-48V v3.5-24V v3.5-48V
    v3.6-24V v3.6-56V v4.0-56V v4.1-58V)
set(ODRIVE_BOARD_VERSION "v3.6-56V" CACHE STRING "ODrive 硬件版本")
set_property(CACHE ODRIVE_BOARD_VERSION PROPERTY STRINGS ${_odrive_boards})
if(NOT ODRIVE_BOARD_VERSION IN_LIST _odrive_boards)
    message(FATAL_ERROR "不支持的 ODRIVE_BOARD_VERSION=${ODRIVE_BOARD_VERSION}；可选：${_odrive_boards}")
endif()

string(REGEX MATCH "^v([34])[.]([0-9]+)(-([0-9]+)V)?$" _board_match "${ODRIVE_BOARD_VERSION}")
set(odrive_board_major "${CMAKE_MATCH_1}")
set(odrive_board_minor "${CMAKE_MATCH_2}")
set(odrive_board_voltage "${CMAKE_MATCH_4}")
if(NOT odrive_board_voltage)
    set(odrive_board_voltage 24)
endif()
set(odrive_board_definitions
    HW_VERSION_MAJOR=${odrive_board_major}
    HW_VERSION_MINOR=${odrive_board_minor}
    HW_VERSION_VOLTAGE=${odrive_board_voltage})

if(odrive_board_major EQUAL 3)
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
else()
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Private/v4/board.cpp" OR
       NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Private/ThirdParty/STM32F7xx_HAL_Driver")
        message(FATAL_ERROR "${ODRIVE_BOARD_VERSION} 需要未随公开工程提供的 Private/v4 板级源码和 Private/ThirdParty/STM32F7xx_HAL_Driver")
    endif()
    set(odrive_root_interface ODrive4)
    set(odrive_cpu_options -mcpu=cortex-m7 -mfpu=fpv5-sp-d16)
    list(APPEND odrive_board_definitions STM32F722xx ARM_MATH_CM7)
    set(odrive_board_dir "${CMAKE_CURRENT_SOURCE_DIR}/Private/v4")
    set(odrive_hal_dir "${CMAKE_CURRENT_SOURCE_DIR}/Private/ThirdParty/STM32F7xx_HAL_Driver")
    set(odrive_port_dir "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/FreeRTOS/Source/portable/GCC/ARM_CM7/r0p1")
    set(odrive_math_library arm_cortexM7lfsp_math)
    set(odrive_linker_script "${odrive_board_dir}/STM32F722RETx_FLASH.ld")
    set(odrive_board_sources
        ${odrive_board_dir}/startup_stm32f722xx.s
        ${odrive_port_dir}/port.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Private/Drivers/DRV8353/drv8353.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Private/Drivers/status_led.cpp
        ${odrive_board_dir}/board.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Private/ThirdParty/sha-2/sha-256.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Private/ThirdParty/rsa_embedded/rsa.c)
    set(odrive_private_includes
        ${CMAKE_CURRENT_SOURCE_DIR}/Private
        ${CMAKE_CURRENT_SOURCE_DIR}/Private/ThirdParty/sha-2
        ${CMAKE_CURRENT_SOURCE_DIR}/Private/ThirdParty/rsa_embedded)
    set(_board_units main gpio adc can dma freertos spi tim stm32f7xx_it
        stm32f7xx_hal_msp stm32f7xx_hal_timebase_tim system_stm32f7xx
        i2s usart usb_device usbd_conf usbd_desc usbd_cdc_if i2c)
    set(_hal_prefix stm32f7xx_)
    set(_hal_units hal hal_adc hal_adc_ex hal_can hal_cortex hal_dma hal_dma_ex
        hal_exti hal_flash hal_flash_ex hal_gpio hal_i2c hal_i2c_ex hal_i2s
        hal_pcd hal_pcd_ex hal_pwr hal_pwr_ex hal_rcc hal_rcc_ex hal_spi hal_spi_ex
        hal_tim hal_tim_ex hal_uart hal_uart_ex ll_usb)
endif()

foreach(unit IN LISTS _board_units)
    list(APPEND odrive_board_sources "${odrive_board_dir}/Src/${unit}.c")
endforeach()
foreach(unit IN LISTS _hal_units)
    list(APPEND odrive_hal_sources "${odrive_hal_dir}/Src/${_hal_prefix}${unit}.c")
endforeach()
set(odrive_board_includes
    ${odrive_board_dir}/Inc ${odrive_hal_dir}/Inc ${odrive_port_dir}
    ${odrive_private_includes})
