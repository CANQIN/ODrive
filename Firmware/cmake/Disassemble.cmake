execute_process(COMMAND "${OBJDUMP}" -dSC "${ELF}"
    OUTPUT_FILE "${OUTPUT}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "反汇编失败：${result}")
endif()
