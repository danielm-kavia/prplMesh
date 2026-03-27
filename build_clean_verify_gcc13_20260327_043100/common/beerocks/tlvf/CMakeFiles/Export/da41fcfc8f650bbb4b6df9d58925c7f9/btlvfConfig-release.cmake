#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "beerocks::btlvf" for configuration "Release"
set_property(TARGET beerocks::btlvf APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(beerocks::btlvf PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libbtlvf.so.5.2.0"
  IMPORTED_SONAME_RELEASE "libbtlvf.so.5"
  )

list(APPEND _cmake_import_check_targets beerocks::btlvf )
list(APPEND _cmake_import_check_files_for_beerocks::btlvf "${_IMPORT_PREFIX}/lib/libbtlvf.so.5.2.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
