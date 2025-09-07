#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "iceoryx_hoofs::iceoryx_hoofs" for configuration "Release"
set_property(TARGET iceoryx_hoofs::iceoryx_hoofs APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(iceoryx_hoofs::iceoryx_hoofs PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libiceoryx_hoofs.so.2.95.7"
  IMPORTED_SONAME_RELEASE "libiceoryx_hoofs.so.2"
  )

list(APPEND _cmake_import_check_targets iceoryx_hoofs::iceoryx_hoofs )
list(APPEND _cmake_import_check_files_for_iceoryx_hoofs::iceoryx_hoofs "${_IMPORT_PREFIX}/lib/libiceoryx_hoofs.so.2.95.7" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
