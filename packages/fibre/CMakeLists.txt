cmake_minimum_required(VERSION 3.20)
project(fibre VERSION 0.1.4 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(GenerateExportHeader)
option(FIBRE_BUILD_SERVER "构建无堆分配的 MCU 服务端" ON)
option(FIBRE_BUILD_CLIENT "构建上位机客户端" ON)
option(FIBRE_BUILD_SHARED "构建供语言绑定使用的 C ABI 动态库" ON)
set(FIBRE_LOG_VERBOSITY 0 CACHE STRING "日志级别，0 关闭")

function(fibre_configure target server client heap)
    target_compile_features(${target} PUBLIC cxx_std_17)
    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    target_compile_definitions(${target} PUBLIC
        FIBRE_ENABLE_SERVER=${server} FIBRE_ENABLE_CLIENT=${client}
        FIBRE_ALLOW_HEAP=${heap}
        FIBRE_MAX_LOG_VERBOSITY=${FIBRE_LOG_VERBOSITY} FIBRE_DEFAULT_LOG_VERBOSITY=${FIBRE_LOG_VERBOSITY})
endfunction()

set(client_sources src/fibre.cpp src/channel_discoverer.cpp
    src/legacy_protocol.cpp src/legacy_object_client.cpp src/logging.cpp)
set(fibre_targets)
if(FIBRE_BUILD_SERVER)
    add_library(fibre_server STATIC src/legacy_protocol.cpp src/logging.cpp)
    add_library(fibre::server ALIAS fibre_server)
    set_target_properties(fibre_server PROPERTIES EXPORT_NAME server)
    fibre_configure(fibre_server 1 0 0)
    list(APPEND fibre_targets fibre_server)
endif()

if(FIBRE_BUILD_CLIENT)
    add_library(fibre_client STATIC ${client_sources})
    add_library(fibre::client ALIAS fibre_client)
    set_target_properties(fibre_client PROPERTIES EXPORT_NAME client POSITION_INDEPENDENT_CODE ON)
    fibre_configure(fibre_client 0 1 1)
    list(APPEND fibre_targets fibre_client)

    if(FIBRE_BUILD_SHARED)
        add_library(libfibre SHARED src/libfibre.cpp)
        add_library(fibre::c ALIAS libfibre)
        target_link_libraries(libfibre PRIVATE fibre_client)
        generate_export_header(libfibre
            EXPORT_MACRO_NAME FIBRE_PUBLIC NO_EXPORT_MACRO_NAME FIBRE_PRIVATE
            EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/fibre/export.h")
        target_compile_definitions(libfibre PUBLIC FIBRE_USE_EXPORT_HEADER)
        target_include_directories(libfibre PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>)
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/include/fibre/export.h"
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fibre)
        target_include_directories(libfibre INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
        set_target_properties(libfibre PROPERTIES EXPORT_NAME c OUTPUT_NAME fibre)
        list(APPEND fibre_targets libfibre)
    endif()
endif()

include(cmake/FibreGenerate.cmake)

install(TARGETS ${fibre_targets} EXPORT fibreTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/fibre DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(PROGRAMS codegen/interface_generator.py DESTINATION ${CMAKE_INSTALL_DATADIR}/fibre/codegen)
install(FILES codegen/templates/interfaces_template.j2 codegen/templates/function_stubs_template.j2
    codegen/templates/endpoints_template.j2 codegen/templates/type_info_template.j2
    DESTINATION ${CMAKE_INSTALL_DATADIR}/fibre/codegen/templates)
install(FILES cmake/FibreGenerate.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fibre)
configure_package_config_file(cmake/fibreConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/fibreConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fibre
    PATH_VARS CMAKE_INSTALL_DATADIR)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/fibreConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(EXPORT fibreTargets NAMESPACE fibre:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fibre)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/fibreConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/fibreConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fibre)
