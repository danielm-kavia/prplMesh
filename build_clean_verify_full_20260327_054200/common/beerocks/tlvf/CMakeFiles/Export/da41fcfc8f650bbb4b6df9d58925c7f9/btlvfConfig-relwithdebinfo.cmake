#----------------------------------------------------------------
# Generated CMake target import file for configuration "RelWithDebInfo".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "beerocks::btlvf" for configuration "RelWithDebInfo"
set_property(TARGET beerocks::btlvf APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(beerocks::btlvf PROPERTIES
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/lib/libbtlvf.so.5.2.0"
  IMPORTED_SONAME_RELWITHDEBINFO "libbtlvf.so.5"
  )

list(APPEND _cmake_import_check_targets beerocks::btlvf )
list(APPEND _cmake_import_check_files_for_beerocks::btlvf "${_IMPORT_PREFIX}/lib/libbtlvf.so.5.2.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
