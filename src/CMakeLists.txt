include(GNUInstallDirs)

add_subdirectory(lib)

if (INFLATELIB_API_DOC)
  find_package(Doxygen REQUIRED doxygen dot)

  set(DOXYGEN_PROJECT_NAME "InflateLib")
  set(DOXYGEN_OPTIMIZE_OUTPUT_FOR_C YES)
  set(DOXYGEN_ENABLE_PREPROCESSING YES)
  set(DOXYGEN_MACRO_EXPANSION      YES)
  set(DOXYGEN_EXPAND_ONLY_PREDEF   YES)
  set(DOXYGEN_PREDEFINED "INFLATELIB_CALLCONV=" "INFLATELIB_EXPORT=")

  set(DOXYGEN_FULL_PATH_NAMES YES)
  set(DOXYGEN_STRIP_FROM_PATH
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/src/include"
  )

  doxygen_add_docs(inflatelib_api_docs
      ./index.dox
      ./include/inflatelib.h
      ALL
      USE_STAMP_FILE
  )

  install(
      DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html
      DESTINATION ${CMAKE_INSTALL_DOCDIR}
      COMPONENT doc
  )
endif()
