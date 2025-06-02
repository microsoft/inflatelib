
# Use all test inputs that are not Deflate64-specific for the fuzzing corpus
set(INPUT
    "uncompressed.empty.in.bin"
    "uncompressed.single.in.bin"
    "uncompressed.multiple.in.bin"
    "dynamic.empty.in.bin"
    "dynamic.single.deflate64.in.bin"
    "dynamic.multiple.deflate64.in.bin"
    "dynamic.overlap.deflate64.in.bin"
    "dynamic.length-distance-stress.deflate64.in.bin"
    "static.empty.in.bin"
    "static.single.deflate64.in.bin"
    "static.multiple.deflate64.in.bin"
    "static.overlap.deflate64.in.bin"
    "static.length-distance-stress.deflate64.in.bin"
    "mixed.empty.in.bin"
    "mixed.simple.in.bin"
    "mixed.overlap.deflate64.in.bin"
    "file.bin-write.deflate64.exe.in.bin"
    "file.magna-carta.deflate64.txt.in.bin"
    "file.us-constitution.deflate64.txt.in.bin"
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

foreach(file ${INPUT})
    add_custom_command(
        OUTPUT ${file}
        DEPENDS "${CMAKE_BINARY_DIR}/test/data/${file}"
        COMMENT "Copying ${file} to corpus"
        COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_BINARY_DIR}/test/data/${file}" "../corpus-in/${file}"
    )
endforeach()

add_custom_target(
    inflate64-corpus ALL
    DEPENDS ${INPUT}
)
