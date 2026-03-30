#----------------------------------------------------------------
# Generated CMake target import file for configuration "RelWithDebInfo".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "beerocks::bml" for configuration "RelWithDebInfo"
set_property(TARGET beerocks::bml APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(beerocks::bml PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELWITHDEBINFO "beerocks::bcl;tlvf;elpp;beerocks::btlvf"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/lib/libbml.so.5.2.0"
  IMPORTED_SONAME_RELWITHDEBINFO "libbml.so.5"
  )

list(APPEND _cmake_import_check_targets beerocks::bml )
list(APPEND _cmake_import_check_files_for_beerocks::bml "${_IMPORT_PREFIX}/lib/libbml.so.5.2.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
