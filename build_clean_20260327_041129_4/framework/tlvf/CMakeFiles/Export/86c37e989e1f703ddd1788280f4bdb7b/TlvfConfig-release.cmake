#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "tlvf" for configuration "Release"
set_property(TARGET tlvf APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(tlvf PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "elpp"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libtlvf.so.5.2.0"
  IMPORTED_SONAME_RELEASE "libtlvf.so.5"
  )

list(APPEND _cmake_import_check_targets tlvf )
list(APPEND _cmake_import_check_files_for_tlvf "${_IMPORT_PREFIX}/lib/libtlvf.so.5.2.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
