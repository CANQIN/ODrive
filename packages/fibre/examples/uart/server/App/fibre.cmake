set(FIBRE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../.." CACHE PATH "Fibre 包源码目录")
set(FIBRE_BUILD_SERVER ON CACHE BOOL "" FORCE)
set(FIBRE_BUILD_CLIENT OFF CACHE BOOL "" FORCE)
set(FIBRE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory("${FIBRE_SOURCE_DIR}" fibre)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE App/device.cpp App/fibre_app.cpp)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE App)
target_link_libraries(${CMAKE_PROJECT_NAME} fibre::server)
# 生成端点的类型查询使用 RTTI，覆盖 CubeMX 对 C++ 的默认关闭选项。
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-frtti>)
fibre_generate_interfaces(${CMAKE_PROJECT_NAME}
    DEFINITIONS "${CMAKE_CURRENT_SOURCE_DIR}/App/device.yaml" ROOT ExampleDevice)

# 本机 GCC 9 的链接器不支持 CubeMX 新版 READONLY 标记；只生成构建目录副本。
set(fibre_linker_source "${CMAKE_CURRENT_SOURCE_DIR}/STM32F405xx_FLASH.ld")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${fibre_linker_source}")
file(READ "${fibre_linker_source}" fibre_linker_script)
string(REPLACE "(READONLY)" "" fibre_linker_script "${fibre_linker_script}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fibre_stm32f405.ld" "${fibre_linker_script}")
string(REPLACE "${fibre_linker_source}" "${CMAKE_CURRENT_BINARY_DIR}/fibre_stm32f405.ld"
    CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
