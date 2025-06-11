
# Use all test inputs that are not Deflate64-specific for the fuzzing corpus
set(INPUT
    "uncompressed.empty.in.bin"
    "uncompressed.single.in.bin"
    "uncompressed.multiple.in.bin"
    "dynamic.empty.in.bin"
    "dynamic.single.deflate.in.bin"
    "dynamic.multiple.deflate.in.bin"
    "dynamic.overlap.deflate.in.bin"
    "dynamic.length-distance-stress.deflate.in.bin"
    "static.empty.in.bin"
    "static.single.deflate.in.bin"
    "static.multiple.deflate.in.bin"
    "static.overlap.deflate.in.bin"
    "static.length-distance-stress.deflate.in.bin"
    "mixed.empty.in.bin"
    "mixed.simple.in.bin"
    "mixed.overlap.deflate.in.bin"
    "file.bin-write.deflate.exe.in.bin"
    "file.magna-carta.deflate.txt.in.bin"
    "file.us-constitution.deflate.txt.in.bin"
    "truncated.uncompressed.block.in.bin"
    "truncated.uncompressed.no-bfinal.in.bin"
    "truncated.dynamic.block.in.bin"
    "truncated.dynamic.no-bfinal.in.bin"
    "truncated.static.block.in.bin"
    "truncated.static.no-bfinal.in.bin"
    "extra.uncompressed.in.bin"
    "extra.dynamic.in.bin"
    "extra.static.in.bin"
    )
set(OUTPUT)

foreach(file ${INPUT})
    set(dest "${INFLATELIB_FUZZING_DIRECTORY}/inflate-corpus-in/${file}")
    list(APPEND OUTPUT "${dest}")
    add_custom_command(
        OUTPUT "${dest}"
        DEPENDS "${CMAKE_BINARY_DIR}/test/data/${file}"
        COMMENT "Copying ${file} to corpus"
        COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_BINARY_DIR}/test/data/${file}" "${dest}"
    )
endforeach()

add_custom_target(
    inflate-corpus ALL
    DEPENDS ${OUTPUT}
)
