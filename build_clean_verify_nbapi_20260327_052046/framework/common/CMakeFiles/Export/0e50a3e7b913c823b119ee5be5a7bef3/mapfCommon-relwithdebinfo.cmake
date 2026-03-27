#----------------------------------------------------------------
# Generated CMake target import file for configuration "RelWithDebInfo".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "mapf::mapfcommon" for configuration "RelWithDebInfo"
set_property(TARGET mapf::mapfcommon APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(mapf::mapfcommon PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELWITHDEBINFO "elpp"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/lib/libmapfcommon.so.5.2.0"
  IMPORTED_SONAME_RELWITHDEBINFO "libmapfcommon.so.5"
  )

list(APPEND _cmake_import_check_targets mapf::mapfcommon )
list(APPEND _cmake_import_check_files_for_mapf::mapfcommon "${_IMPORT_PREFIX}/lib/libmapfcommon.so.5.2.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
