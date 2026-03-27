#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "beerocks::bml" for configuration "Release"
set_property(TARGET beerocks::bml APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(beerocks::bml PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "beerocks::bcl;tlvf;elpp;beerocks::btlvf"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libbml.so.5.2.0"
  IMPORTED_SONAME_RELEASE "libbml.so.5"
  )

list(APPEND _cmake_import_check_targets beerocks::bml )
list(APPEND _cmake_import_check_files_for_beerocks::bml "${_IMPORT_PREFIX}/lib/libbml.so.5.2.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
