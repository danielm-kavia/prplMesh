#----------------------------------------------------------------
# Generated CMake target import file for configuration "RelWithDebInfo".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "elpp" for configuration "RelWithDebInfo"
set_property(TARGET elpp APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(elpp PROPERTIES
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/lib/libelpp.so.5.2.0"
  IMPORTED_SONAME_RELWITHDEBINFO "libelpp.so.5"
  )

list(APPEND _cmake_import_check_targets elpp )
list(APPEND _cmake_import_check_files_for_elpp "${_IMPORT_PREFIX}/lib/libelpp.so.5.2.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
