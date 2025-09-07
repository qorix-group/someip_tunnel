#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "iceoryx2-cxx::static-lib-cxx" for configuration ""
set_property(TARGET iceoryx2-cxx::static-lib-cxx APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(iceoryx2-cxx::static-lib-cxx PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "CXX"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libiceoryx2_cxx.a"
  )

list(APPEND _cmake_import_check_targets iceoryx2-cxx::static-lib-cxx )
list(APPEND _cmake_import_check_files_for_iceoryx2-cxx::static-lib-cxx "${_IMPORT_PREFIX}/lib/libiceoryx2_cxx.a" )

# Import target "iceoryx2-cxx::shared-lib-cxx" for configuration ""
set_property(TARGET iceoryx2-cxx::shared-lib-cxx APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(iceoryx2-cxx::shared-lib-cxx PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libiceoryx2_cxx.so"
  IMPORTED_SONAME_NOCONFIG "libiceoryx2_cxx.so"
  )

list(APPEND _cmake_import_check_targets iceoryx2-cxx::shared-lib-cxx )
list(APPEND _cmake_import_check_files_for_iceoryx2-cxx::shared-lib-cxx "${_IMPORT_PREFIX}/lib/libiceoryx2_cxx.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
