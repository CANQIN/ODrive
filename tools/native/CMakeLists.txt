cmake_minimum_required(VERSION 3.20)
project(ODriveHost LANGUAGES C CXX)

if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(FIBRE_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(FIBRE_BUILD_CLIENT ON CACHE BOOL "" FORCE)
set(FIBRE_BUILD_SHARED ON CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../packages/fibre" fibre)

# 运行库部署属于 ODrive 应用，独立 SDK 不携带平台链接选项。
if(MINGW)
    target_link_options(libfibre PRIVATE -static)
endif()

option(ODRIVE_BUILD_TESTS "构建已有固件主机测试" OFF)
if(ODRIVE_BUILD_TESTS)
    include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board-version.cmake")
    enable_testing()
    set(firmware "${CMAKE_CURRENT_LIST_DIR}/../../Firmware")
    add_executable(odrive_tests
        "${firmware}/Tests/test_runner.cpp"
        "${firmware}/Tests/test_can.cpp"
        "${firmware}/Tests/test_nvm_config.cpp"
        "${firmware}/Tests/test_rotate.cpp"
        "${firmware}/Tests/test_timer.cpp"
        "${firmware}/Tests/test_trap_traj.cpp")
    target_compile_features(odrive_tests PRIVATE cxx_std_17)
    target_include_directories(odrive_tests PRIVATE "${firmware}"
        "${firmware}/MotorControl" "${CMAKE_CURRENT_LIST_DIR}/../../packages/fibre/include"
        "${firmware}/Drivers/DRV8301" "${firmware}/doctest")
    add_test(NAME odrive_tests COMMAND odrive_tests)

    # Exercise the actual ODrive USB class against the regenerated ST USB core.
    # Only the MCU HAL and RTOS queue are mocked; USB descriptors/setup/data
    # dispatch execute the same C sources as the firmware.
    set(usb_core "${odrive_cube_dir}/Middlewares/ST/STM32_USB_Device_Library/Core")
    add_executable(odrive_usb_tests
        "${firmware}/Tests/test_usb.cpp"
        "${firmware}/Platform/STM32/USB/Src/usbd_cdc.c"
        "${firmware}/Platform/STM32/USB/Src/usbd_desc.c"
        "${firmware}/Platform/STM32/USB/Src/usb_ll_adapter.c"
        "${usb_core}/Src/usbd_core.c"
        "${usb_core}/Src/usbd_ctlreq.c"
        "${usb_core}/Src/usbd_ioreq.c")
    target_compile_features(odrive_usb_tests PRIVATE cxx_std_17)
    foreach(_field major minor voltage)
        string(JSON _value GET "${odrive_board_manifest}" hardware ${_field})
        string(TOUPPER "${_field}" _upper)
        target_compile_definitions(odrive_usb_tests PRIVATE "HW_VERSION_${_upper}=${_value}")
    endforeach()
    target_include_directories(odrive_usb_tests PRIVATE
        "${firmware}/Tests/usb_mocks" "${firmware}/Platform/STM32/USB/Inc"
        "${odrive_cube_dir}/USB_DEVICE/Target" "${odrive_cube_dir}/Core/Inc"
        "${usb_core}/Inc" "${firmware}" "${firmware}/doctest")
    add_test(NAME odrive_usb_tests COMMAND odrive_usb_tests)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME cubemx_configuration
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_LIST_DIR}/../../scripts/check-cubemx.py"
            --board "${ODRIVE_BOARD_VERSION}")
    add_test(NAME board_profiles
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_LIST_DIR}/../../scripts/test_board_support.py")
endif()
