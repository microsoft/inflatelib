
# Concatenates the file referenced by HEADER with the file referenced by SRC, writing the result to DEST. This is used to
# prepend the fuzzing buffer-size header to each corpus entry. We use 'cmake -E cat' via execute_process (rather than
# invoking it directly from add_custom_command) because we need to redirect the concatenated output to a file, which
# OUTPUT_FILE does in a binary-safe, cross-platform way.

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E cat "${HEADER}" "${SRC}"
    OUTPUT_FILE "${DEST}"
    RESULT_VARIABLE exit_code
)

if (exit_code)
    message(FATAL_ERROR "Failed to prepend '${HEADER}' to '${SRC}' (exit code: ${exit_code})")
endif()
