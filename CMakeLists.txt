cmake_minimum_required(VERSION 3.20)
project(ODrive LANGUAGES NONE)

include(ExternalProject)
find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
find_program(ODRIVE_NINJA_EXECUTABLE NAMES ninja REQUIRED)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "构建类型" FORCE)
endif()
option(ODRIVE_BUILD_FIRMWARE "构建 MCU 固件" ON)
option(ODRIVE_BUILD_HOST "构建供 tools 使用的 Fibre 客户端" ON)
option(ODRIVE_BUILD_TOOLS "打包 Python SDK 和 ODrive 工具" OFF)
option(ODRIVE_BUILD_GUI "启用 GUI 构建目标，默认构建 Web 界面" OFF)
option(ODRIVE_GUI_INSTALL_DEPS "GUI 构建前运行 npm ci" OFF)
option(ODRIVE_BUILD_TESTS "构建已有固件主机测试" OFF)
option(ODRIVE_USE_LTO "固件启用 LTO" OFF)
option(ODRIVE_STRICT "固件将警告视为错误" OFF)
option(ODRIVE_NO_DRM "固件关闭 DRM" OFF)
option(ODRIVE_ENABLE_DISASM "生成固件反汇编" OFF)
set(ODRIVE_BOARD_VERSION v3.6-56V CACHE STRING "固件硬件版本")
set(ARM_TOOLCHAIN_DIR "" CACHE PATH "ARM GCC 安装目录或 bin 目录")
set(ODRIVE_HOST_INSTALL "${CMAKE_BINARY_DIR}/host/install")

# 根项目只调度：PC 和 MCU 在不同构建目录中分别选择编译器。
if(ODRIVE_BUILD_HOST)
    ExternalProject_Add(host
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/native"
        BINARY_DIR "${CMAKE_BINARY_DIR}/host/build"
        CMAKE_GENERATOR Ninja
        CMAKE_ARGS
            "-DCMAKE_MAKE_PROGRAM:FILEPATH=${ODRIVE_NINJA_EXECUTABLE}"
            "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
            "-DCMAKE_INSTALL_PREFIX:PATH=${ODRIVE_HOST_INSTALL}"
            "-DPython3_EXECUTABLE:FILEPATH=${Python3_EXECUTABLE}"
            "-DODRIVE_BUILD_TESTS:BOOL=${ODRIVE_BUILD_TESTS}"
        BUILD_ALWAYS TRUE
        CONFIGURE_HANDLED_BY_BUILD TRUE)
endif()

if(ODRIVE_BUILD_TESTS)
    if(NOT ODRIVE_BUILD_HOST)
        message(FATAL_ERROR "ODRIVE_BUILD_TESTS 需要 ODRIVE_BUILD_HOST")
    endif()
    add_custom_target(tests
        COMMAND ${CMAKE_CTEST_COMMAND} --test-dir "${CMAKE_BINARY_DIR}/host/build" --output-on-failure
        DEPENDS host USES_TERMINAL VERBATIM)
endif()

if(ODRIVE_BUILD_FIRMWARE)
    ExternalProject_Add(firmware
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Firmware"
        BINARY_DIR "${CMAKE_BINARY_DIR}/firmware"
        CMAKE_GENERATOR Ninja
        CMAKE_ARGS
            "-DCMAKE_MAKE_PROGRAM:FILEPATH=${ODRIVE_NINJA_EXECUTABLE}"
            "-DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_CURRENT_SOURCE_DIR}/Firmware/cmake/arm-none-eabi.cmake"
            "-DARM_TOOLCHAIN_DIR:PATH=${ARM_TOOLCHAIN_DIR}"
            "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
            "-DPython3_EXECUTABLE:FILEPATH=${Python3_EXECUTABLE}"
            "-DODRIVE_BOARD_VERSION:STRING=${ODRIVE_BOARD_VERSION}"
            "-DODRIVE_USE_LTO:BOOL=${ODRIVE_USE_LTO}"
            "-DODRIVE_STRICT:BOOL=${ODRIVE_STRICT}"
            "-DODRIVE_NO_DRM:BOOL=${ODRIVE_NO_DRM}"
            "-DODRIVE_ENABLE_DISASM:BOOL=${ODRIVE_ENABLE_DISASM}"
        INSTALL_COMMAND ""
        BUILD_ALWAYS TRUE
        CONFIGURE_HANDLED_BY_BUILD TRUE)
endif()

set(interface_generator "${CMAKE_CURRENT_SOURCE_DIR}/packages/fibre/codegen/interface_generator.py")
set(interface_definitions "${CMAKE_CURRENT_SOURCE_DIR}/Firmware/odrive-interface.yaml")
set(python_enums "${CMAKE_BINARY_DIR}/generated/python/enums.py")
set(arduino_enums "${CMAKE_BINARY_DIR}/generated/arduino/ODriveEnums.h")
add_custom_command(OUTPUT "${python_enums}" "${arduino_enums}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_BINARY_DIR}/generated/python" "${CMAKE_BINARY_DIR}/generated/arduino"
    COMMAND Python3::Interpreter "${interface_generator}" --definitions "${interface_definitions}"
        --template "${CMAKE_CURRENT_SOURCE_DIR}/tools/enums_template.j2" --output "${python_enums}"
    COMMAND Python3::Interpreter "${interface_generator}" --definitions "${interface_definitions}"
        --template "${CMAKE_CURRENT_SOURCE_DIR}/tools/arduino_enums_template.j2" --output "${arduino_enums}"
    DEPENDS "${interface_generator}" "${interface_definitions}"
        tools/enums_template.j2 tools/arduino_enums_template.j2
    VERBATIM)
add_custom_target(interfaces ALL DEPENDS "${python_enums}" "${arduino_enums}")

if(ODRIVE_BUILD_TOOLS)
    if(NOT ODRIVE_BUILD_HOST)
        message(FATAL_ERROR "ODRIVE_BUILD_TOOLS 需要 ODRIVE_BUILD_HOST，以打包本轮新客户端")
    endif()
    add_custom_target(tools ALL
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_tools.py"
            --repo "${CMAKE_CURRENT_SOURCE_DIR}" --build "${CMAKE_BINARY_DIR}"
            --native-prefix "${ODRIVE_HOST_INSTALL}" --enums "${python_enums}"
        DEPENDS host interfaces
        COMMENT "打包新 Fibre Python 绑定与 ODrive 工具"
        VERBATIM)
endif()

if(ODRIVE_BUILD_GUI)
    find_program(ODRIVE_NPM_EXECUTABLE NAMES npm.cmd npm REQUIRED)
    add_custom_target(gui_dependencies
        COMMAND "${ODRIVE_NPM_EXECUTABLE}" ci
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/GUI"
        USES_TERMINAL VERBATIM)
    add_custom_target(gui_web ALL
        COMMAND ${CMAKE_COMMAND} -E env "ODRIVE_ENUMS_PATH=${python_enums}"
            "${ODRIVE_NPM_EXECUTABLE}" run enumGenerate
        COMMAND "${ODRIVE_NPM_EXECUTABLE}" run build:web
        DEPENDS interfaces
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/GUI"
        USES_TERMINAL VERBATIM)
    add_custom_target(gui_package
        COMMAND ${CMAKE_COMMAND} -E env "ODRIVE_ENUMS_PATH=${python_enums}"
            "${ODRIVE_NPM_EXECUTABLE}" run electron:build
        DEPENDS interfaces
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/GUI"
        USES_TERMINAL VERBATIM)
    if(ODRIVE_GUI_INSTALL_DEPS)
        add_dependencies(gui_web gui_dependencies)
        add_dependencies(gui_package gui_dependencies)
    endif()
endif()
