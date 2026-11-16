cmake_minimum_required(VERSION 3.20)
project(ODriveFirmware LANGUAGES C CXX ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_C_COMPILER MATCHES "arm-none-eabi")
    message(FATAL_ERROR "固件需要 ARM GCC；请使用 cmake/arm-none-eabi.cmake 工具链")
endif()
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "固件构建类型" FORCE)
endif()
# 优化和调试选项统一在下面设置，避免 CMake 默认 Release 的 O3/NDEBUG 改变原行为。
foreach(language C CXX ASM)
    foreach(configuration DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set(CMAKE_${language}_FLAGS_${configuration} "")
    endforeach()
endforeach()
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ODRIVE_STRICT "将固件编译警告视为错误" OFF)
option(ODRIVE_USE_LTO "启用链接时优化" OFF)
option(ODRIVE_ENABLE_DISASM "输出反汇编文件" OFF)
include(cmake/boards.cmake)

add_compile_definitions("__weak=__attribute__((weak))"
    "__packed=__attribute__((__packed__))" USE_HAL_DRIVER ${odrive_board_definitions})
add_compile_options(-mthumb -mfloat-abi=hard ${odrive_cpu_options}
    -Wall -Wno-psabi -Wdouble-promotion -Wfloat-conversion -Wno-nonnull
    -fdata-sections -ffunction-sections -g
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-register>
    $<$<CONFIG:Debug>:-gdwarf-2>
    $<$<CONFIG:Debug>:-Og>
    $<$<NOT:$<CONFIG:Debug>>:-O2>)
if(ODRIVE_STRICT)
    add_compile_options(-Werror)
endif()
if(ODRIVE_USE_LTO)
    add_compile_options(-flto)
endif()

# 只使用 SDK 的服务端目标，不在应用中枚举或复制 SDK 源码。
set(FIBRE_BUILD_SERVER ON CACHE BOOL "" FORCE)
set(FIBRE_BUILD_CLIENT OFF CACHE BOOL "" FORCE)
set(FIBRE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(FIBRE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FIBRE_LOG_VERBOSITY 0 CACHE STRING "" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../packages/fibre" fibre)

set(odrive_sources
    syscalls.c
    MotorControl/utils.cpp
    MotorControl/arm_sin_f32.c
    MotorControl/arm_cos_f32.c
    MotorControl/low_level.cpp
    MotorControl/axis.cpp
    MotorControl/motor.cpp
    MotorControl/thermistor.cpp
    MotorControl/encoder.cpp
    MotorControl/encoder_incremental.cpp
    MotorControl/encoder_hall.cpp
    MotorControl/encoder_sincos.cpp
    MotorControl/encoder_spi.cpp
    MotorControl/endstop.cpp
    MotorControl/acim_estimator.c
    MotorControl/mechanical_brake.cpp
    MotorControl/controller.cpp
    MotorControl/foc.cpp
    MotorControl/open_loop_controller.c
    MotorControl/oscilloscope.cpp
    MotorControl/sensorless_estimator.c
    MotorControl/trap_traj.c
    MotorControl/pwm_input.cpp
    MotorControl/main.cpp
    Drivers/STM32/stm32_system.cpp
    Drivers/STM32/stm32_gpio.cpp
    Drivers/STM32/stm32_nvm.c
    Drivers/STM32/stm32_spi_arbiter.cpp
    communication/can/can_simple.cpp
    communication/can/odrive_can.cpp
    communication/communication.cpp
    communication/ascii_protocol.cpp
    communication/interface_uart.cpp
    communication/interface_usb.cpp
    FreeRTOS-openocd.c)
set(_freertos_units croutine event_groups list queue stream_buffer tasks timers)
foreach(unit IN LISTS _freertos_units)
    list(APPEND odrive_sources "${odrive_rtos_dir}/${unit}.c")
endforeach()
list(APPEND odrive_sources
    ${odrive_rtos_dir}/CMSIS_RTOS/cmsis_os.c
    ${odrive_rtos_dir}/portable/MemMang/heap_4.c
    ${odrive_usb_dir}/Core/Src/usbd_core.c
    ${odrive_usb_dir}/Core/Src/usbd_ctlreq.c
    ${odrive_usb_dir}/Core/Src/usbd_ioreq.c)

add_executable(ODriveFirmware ${odrive_sources} ${odrive_board_sources} ${odrive_hal_sources})
set_target_properties(ODriveFirmware PROPERTIES SUFFIX .elf
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
target_include_directories(ODriveFirmware BEFORE PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/MotorControl"
    ${odrive_board_includes}
    "${odrive_rtos_dir}/include" "${odrive_rtos_dir}/CMSIS_RTOS"
    ${odrive_cmsis_includes}
    "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/CMSIS_DSP/Include"
    "${odrive_usb_dir}/Core/Inc")

fibre_generate_interfaces(ODriveFirmware
    DEFINITIONS "${CMAKE_CURRENT_SOURCE_DIR}/odrive-interface.yaml"
    ROOT "${odrive_root_interface}" TYPE_INFO
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/autogen")
add_custom_target(odrive_interfaces DEPENDS
    "${CMAKE_CURRENT_BINARY_DIR}/autogen/interfaces.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/autogen/function_stubs.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/autogen/endpoints.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/autogen/type_info.hpp")
add_dependencies(ODriveFirmware odrive_interfaces)

# 每次构建读取 Git 版本；版本文本没变化时保留 version.c 的时间戳。
set(version_source "${CMAKE_CURRENT_BINARY_DIR}/autogen/version.c")
add_custom_target(odrive_version
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/autogen"
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/../tools/odrive/version.py"
        --output "${version_source}.tmp"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${version_source}.tmp" "${version_source}"
    BYPRODUCTS "${version_source}" "${version_source}.tmp"
    VERBATIM)
set_source_files_properties("${version_source}" PROPERTIES GENERATED TRUE)
target_sources(ODriveFirmware PRIVATE "${version_source}")
add_dependencies(ODriveFirmware odrive_version)

target_link_libraries(ODriveFirmware PRIVATE fibre::server
    "${odrive_dsp_library}"
    c m nosys)
target_link_options(ODriveFirmware PRIVATE
    -mthumb -mfloat-abi=hard ${odrive_cpu_options}
    -specs=nosys.specs -Wl,--undefined=_printf_float -Wl,--undefined=_scanf_float
    -Wl,--cref -Wl,--gc-sections -Wl,--undefined=uxTopUsedPriority
    "-T${odrive_linker_script}"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/ODriveFirmware.map")
if(ODRIVE_USE_LTO)
    target_link_options(ODriveFirmware PRIVATE -flto)
endif()
set_property(TARGET ODriveFirmware APPEND PROPERTY LINK_DEPENDS "${odrive_linker_script}")
add_custom_command(TARGET ODriveFirmware POST_BUILD
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:ODriveFirmware>"
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:ODriveFirmware>"
        "${CMAKE_CURRENT_BINARY_DIR}/ODriveFirmware.hex"
    COMMAND "${CMAKE_OBJCOPY}" -O binary -S "$<TARGET_FILE:ODriveFirmware>"
        "${CMAKE_CURRENT_BINARY_DIR}/ODriveFirmware.bin"
    BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/ODriveFirmware.hex"
        "${CMAKE_CURRENT_BINARY_DIR}/ODriveFirmware.bin"
    VERBATIM)
if(ODRIVE_ENABLE_DISASM)
    add_custom_command(TARGET ODriveFirmware POST_BUILD
        COMMAND ${CMAKE_COMMAND} "-DOBJDUMP=${CMAKE_OBJDUMP}"
            "-DELF=$<TARGET_FILE:ODriveFirmware>"
            "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ODriveFirmware.asm"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Disassemble.cmake"
        BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/ODriveFirmware.asm"
        VERBATIM)
endif()
add_custom_target(firmware DEPENDS ODriveFirmware)
