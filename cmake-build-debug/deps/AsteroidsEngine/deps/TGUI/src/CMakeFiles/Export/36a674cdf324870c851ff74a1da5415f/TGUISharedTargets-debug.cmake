#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "TGUI::TGUI" for configuration "Debug"
set_property(TARGET TGUI::TGUI APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(TGUI::TGUI PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libtgui-d.so.1.1.0"
  IMPORTED_SONAME_DEBUG "libtgui-d.so.1.1.0"
  )

list(APPEND _cmake_import_check_targets TGUI::TGUI )
list(APPEND _cmake_import_check_files_for_TGUI::TGUI "${_IMPORT_PREFIX}/lib/libtgui-d.so.1.1.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
