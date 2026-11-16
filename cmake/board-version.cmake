# Keep the selector for existing build commands, but reject every other board.
set(ODRIVE_BOARD_VERSION "v3.6-56V" CACHE STRING "硬件版本（仅支持 v3.6-56V）")
set_property(CACHE ODRIVE_BOARD_VERSION PROPERTY STRINGS v3.6-56V)
if(NOT ODRIVE_BOARD_VERSION STREQUAL "v3.6-56V")
    message(FATAL_ERROR "本工程仅支持 ODRIVE_BOARD_VERSION=v3.6-56V，当前值：${ODRIVE_BOARD_VERSION}")
endif()
