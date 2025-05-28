
add_compile_options(${SANITIZER_FLAGS})
link_libraries(${SANITIZER_LIBS})

add_subdirectory(inflate)
