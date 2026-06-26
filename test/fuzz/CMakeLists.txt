
# Flatten the output into a single directory because that's what OneFuzz seems to expect
set(INFLATELIB_FUZZING_DIRECTORY "${CMAKE_BINARY_DIR}/fuzz")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${INFLATELIB_FUZZING_DIRECTORY}")

# The fuzzing harnesses read a list of input buffer sizes followed by a list of output buffer sizes from the front of
# each input. Generate a header that requests a single 64 KiB input buffer and a single 64 KiB output buffer; the corpus
# sub-directories prepend it to each entry (via 'prepend-file.cmake') so the seed corpus exercises the decompression
# code paths. See 'buffer-sizes.in' for the encoding details.
set(INFLATELIB_FUZZ_BUFFER_SIZE_HEADER "${CMAKE_CURRENT_BINARY_DIR}/buffer-sizes.bin")
set(INFLATELIB_FUZZ_PREPEND_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/prepend-file.cmake")
add_custom_command(
    OUTPUT "${INFLATELIB_FUZZ_BUFFER_SIZE_HEADER}"
    DEPENDS bin-write "${CMAKE_CURRENT_SOURCE_DIR}/buffer-sizes.in"
    COMMENT "Generating fuzzing buffer-size header"
    COMMAND bin-write "${CMAKE_CURRENT_SOURCE_DIR}/buffer-sizes.in" "${INFLATELIB_FUZZ_BUFFER_SIZE_HEADER}"
)
add_custom_target(fuzz-buffer-size-header DEPENDS "${INFLATELIB_FUZZ_BUFFER_SIZE_HEADER}")

add_subdirectory(inflate)
add_subdirectory(inflate64)

add_custom_target(
    copy-onefuzz-config ALL
    DEPENDS
        "${INFLATELIB_FUZZING_DIRECTORY}/OneFuzzConfig.json"
        "${INFLATELIB_FUZZING_DIRECTORY}/fuzzing-sources.txt"
)

add_custom_command(
    OUTPUT "${INFLATELIB_FUZZING_DIRECTORY}/OneFuzzConfig.json"
    DEPENDS "OneFuzzConfig.json"
    COMMENT "Copying OneFuzzConfig.json to output directory"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/OneFuzzConfig.json" "${INFLATELIB_FUZZING_DIRECTORY}/OneFuzzConfig.json"
)

add_custom_command(
    OUTPUT "${INFLATELIB_FUZZING_DIRECTORY}/fuzzing-sources.txt"
    DEPENDS "fuzzing-sources.txt"
    COMMENT "Copying fuzzing-sources.txt to output directory"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/fuzzing-sources.txt" "${INFLATELIB_FUZZING_DIRECTORY}/fuzzing-sources.txt"
)
