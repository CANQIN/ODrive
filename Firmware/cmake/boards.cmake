include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board-version.cmake")

foreach(_field major minor voltage)
    string(JSON _value GET "${odrive_board_manifest}" hardware ${_field})
    string(TOUPPER "${_field}" _upper)
    list(APPEND odrive_board_definitions "HW_VERSION_${_upper}=${_value}")
endforeach()
list(APPEND odrive_board_definitions "${odrive_mcu}")
set(odrive_root_interface ODrive3)

# Profiles select their generated peripheral units, bindings and platform port.
include("${odrive_board_dir}/board.cmake")
foreach(_required odrive_cpu_options odrive_board_sources odrive_board_includes
        odrive_hal_sources odrive_rtos_dir odrive_usb_dir odrive_cmsis_includes odrive_dsp_library)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "Board ${ODRIVE_BOARD_VERSION} did not define ${_required}")
    endif()
endforeach()

# MotorControl owns startup; CubeMX supplies clock, HAL tick and error callbacks.
set_source_files_properties("${odrive_cube_dir}/Core/Src/main.c" PROPERTIES
    COMPILE_DEFINITIONS "main=odrive_cubemx_main")
