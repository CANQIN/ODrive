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
    enable_testing()
    set(firmware "${CMAKE_CURRENT_LIST_DIR}/../../Firmware")
    add_executable(odrive_tests
        "${firmware}/Tests/test_runner.cpp"
        "${firmware}/Tests/test_can.cpp"
        "${firmware}/Tests/test_rotate.cpp"
        "${firmware}/Tests/test_timer.cpp"
        "${firmware}/Tests/test_trap_traj.cpp")
    target_compile_features(odrive_tests PRIVATE cxx_std_17)
    target_include_directories(odrive_tests PRIVATE "${firmware}"
        "${firmware}/MotorControl" "${CMAKE_CURRENT_LIST_DIR}/../../packages/fibre/include"
        "${firmware}/Drivers/DRV8301" "${firmware}/doctest")
    add_test(NAME odrive_tests COMMAND odrive_tests)
endif()
