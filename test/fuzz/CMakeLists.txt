
add_compile_options(${SANITIZER_FLAGS})
link_libraries(${SANITIZER_LIBS})
add_link_options(${SANITIZER_LINKER_FLAGS})

add_subdirectory(inflate)
add_subdirectory(inflate64)
