
set(INPUT
    "error.invalid-block-type.in"
    "uncompressed.empty.in"
    "uncompressed.empty.out"
    "uncompressed.error.nlen.in"
    "uncompressed.single.in"
    "uncompressed.single.out"
    "uncompressed.multiple.in"
    "uncompressed.multiple.out"
    "dynamic.empty.in"
    "dynamic.empty.out"
    "dynamic.single.in"
    "dynamic.single.out"
    "dynamic.multiple.in"
    "dynamic.multiple.out"
    "dynamic.overlap.in"
    "dynamic.overlap.out"
    "dynamic.length-distance-stress.in"
    "dynamic.length-distance-stress.out"
    "dynamic.error.tree-size.code-lens.short.in"
    "dynamic.error.tree-size.code-lens.tall.in"
    "dynamic.error.tree-size.literals.short.in"
    "dynamic.error.tree-size.literals.tall.in"
    "dynamic.error.tree-size.distances.short.in"
    "dynamic.error.tree-size.distances.tall.in"
    "dynamic.error.code-lens-oob-repeat.begin.in"
    "dynamic.error.code-lens-oob-repeat.end-prev.in"
    "dynamic.error.code-lens-oob-repeat.end-short.in"
    "dynamic.error.code-lens-oob-repeat.end-long.in"
    "dynamic.error.failed-lookup.code-lens.in"
    "dynamic.error.failed-lookup.literals.short.in"
    "dynamic.error.failed-lookup.literals.long.in"
    "dynamic.error.failed-lookup.distances.short.in"
    "dynamic.error.failed-lookup.distances.long.in"
    "dynamic.error.invalid-symbol.286.in"
    "dynamic.error.invalid-symbol.287.in"
    "dynamic.error.distance-oob.short.in"
    "dynamic.error.distance-oob.long.in"
    "static.empty.in"
    "static.empty.out"
    "static.single.in"
    "static.single.out"
    "static.multiple.in"
    "static.multiple.out"
    "static.overlap.in"
    "static.overlap.out"
    "static.length-distance-stress.in"
    "static.length-distance-stress.out"
    "static.error.invalid-symbol.286.in"
    "static.error.invalid-symbol.287.in"
    "static.error.distance-oob.short.in"
    "static.error.distance-oob.long.in"
    "mixed.empty.in"
    "mixed.empty.out"
    "mixed.simple.in"
    "mixed.simple.out"
    "mixed.overlap.in"
    "mixed.overlap.out"
    "file.bin-write.exe.in"
    "file.bin-write.exe.out"
    "file.magna-carta.txt.in"
    "file.magna-carta.txt.out"
    "file.us-constitution.txt.in"
    "file.us-constitution.txt.out"
    "truncated.uncompressed.block.in"
    "truncated.uncompressed.block.out"
    "truncated.uncompressed.no-bfinal.in"
    "truncated.uncompressed.no-bfinal.out"
    "truncated.dynamic.block.in"
    "truncated.dynamic.block.out"
    "truncated.dynamic.no-bfinal.in"
    "truncated.dynamic.no-bfinal.out"
    "truncated.static.block.in"
    "truncated.static.block.out"
    "truncated.static.no-bfinal.in"
    "truncated.static.no-bfinal.out"
    "extra.uncompressed.in"
    "extra.uncompressed.out"
    "extra.dynamic.in"
    "extra.dynamic.out"
    "extra.static.in"
    "extra.static.out"
    )
set(OUTPUT)

foreach(in ${INPUT})
    set(out "${in}.bin")
    list(APPEND OUTPUT ${out})

    add_custom_command(
        OUTPUT ${out}
        DEPENDS bin-write ${in}
        COMMENT "Generating ${out} from ${in}"
        COMMAND bin-write ${CMAKE_CURRENT_SOURCE_DIR}/${in} ${out}
    )
endforeach()

add_custom_target(
    test-data ALL
    DEPENDS ${OUTPUT}
)
