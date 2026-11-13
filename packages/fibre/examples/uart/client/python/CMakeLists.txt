cmake_minimum_required(VERSION 3.20)
project(fibre_python_client_example LANGUAGES C CXX)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(FIBRE_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../.." CACHE PATH "Fibre 包源码目录")
set(FIBRE_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(FIBRE_BUILD_CLIENT ON CACHE BOOL "" FORCE)
set(FIBRE_BUILD_SHARED ON CACHE BOOL "" FORCE)
if(MSVC)
    add_compile_options(/Zc:__cplusplus /utf-8)
endif()
add_subdirectory("${FIBRE_SOURCE_DIR}" fibre)
if(MINGW)
    # DLL 运行库部署属于示例应用，不放进平台无关核心。
    target_link_options(libfibre PRIVATE -static)
endif()

# Python 源码直接使用包内绑定；CMake 负责构建所需的 C ABI 动态库。
set(SERIAL_PORT "" CACHE STRING "电脑串口名称，例如 COM5")
add_custom_target(run
    COMMAND ${CMAKE_COMMAND} -E env
        "PYTHONPATH=${FIBRE_SOURCE_DIR}/bindings/python"
        "FIBRE_LIBRARY_PATH=$<TARGET_FILE:libfibre>"
        "PYTHONUTF8=1"
        "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/main.py" --port "${SERIAL_PORT}"
    DEPENDS libfibre USES_TERMINAL VERBATIM)
