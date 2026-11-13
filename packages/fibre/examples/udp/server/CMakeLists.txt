cmake_minimum_required(VERSION 3.20)
project(fibre_server_example LANGUAGES C CXX)

set(FIBRE_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../.." CACHE PATH "Fibre 包源码目录")
set(FIBRE_BUILD_SERVER ON CACHE BOOL "" FORCE)
set(FIBRE_BUILD_CLIENT OFF CACHE BOOL "" FORCE)
set(FIBRE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
if(MSVC)
    add_compile_options(/Zc:__cplusplus /utf-8)
endif()
add_subdirectory("${FIBRE_SOURCE_DIR}" fibre)

# device.cpp 属于下位机业务；desktop/ 仅用于在电脑上模拟下位机。
add_executable(fibre_server_example device.cpp desktop/main.cpp)
set_target_properties(fibre_server_example PROPERTIES OUTPUT_NAME fibre_server)
target_link_libraries(fibre_server_example PRIVATE fibre::server)
target_include_directories(fibre_server_example PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
if(WIN32)
    target_link_libraries(fibre_server_example PRIVATE ws2_32)
endif()
fibre_generate_interfaces(fibre_server_example
    DEFINITIONS "${CMAKE_CURRENT_SOURCE_DIR}/device.yaml"
    ROOT ExampleDevice)
