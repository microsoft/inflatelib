
# Flatten the output into a single directory because that's what OneFuzz seems to expect
set(INFLATELIB_FUZZING_DIRECTORY "${CMAKE_BINARY_DIR}/fuzz")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${INFLATELIB_FUZZING_DIRECTORY}")

add_subdirectory(inflate)
add_subdirectory(inflate64)

add_custom_command(
    OUTPUT "${INFLATELIB_FUZZING_DIRECTORY}/OneFuzzConfig.json"
    DEPENDS "OneFuzzConfig.json"
    COMMENT "Copying OneFuzzConfig.json to output directory"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/OneFuzzConfig.json" "${INFLATELIB_FUZZING_DIRECTORY}/OneFuzzConfig.json"
)

add_custom_target(
    copy-onefuzz-config ALL
    DEPENDS "${INFLATELIB_FUZZING_DIRECTORY}/OneFuzzConfig.json")
