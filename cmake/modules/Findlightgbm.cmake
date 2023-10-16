# - Find lightgbm library
# Find the lightgbm includes and library
#
# LIGHTGBM_INCLUDE_DIR - where to find gflags.h.
# LIGHTGBM_LIBRARIES - List of libraries when using gflags.
# lightgbm_FOUND - True if gflags found.

find_path(LIGHTGBM_INCLUDE_DIR
  NAMES LightGBM/c_api.h)

find_library(LIGHTGBM_LIBRARIES
  NAMES _lightgbm)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(lightgbm
  DEFAULT_MSG LIGHTGBM_LIBRARIES LIGHTGBM_INCLUDE_DIR)

mark_as_advanced(
  LIGHTGBM_LIBRARIES
  LIGHTGBM_INCLUDE_DIR)

if(lightgbm_FOUND  AND NOT (TARGET lightgbm::lightgbm))
  add_library(lightgbm::lightgbm UNKNOWN IMPORTED)
  set_target_properties(lightgbm::lightgbm
    PROPERTIES
    IMPORTED_LOCATION ${LIGHTGBM_LIBRARIES}
    INTERFACE_INCLUDE_DIRECTORIES ${LIGHTGBM_INCLUDE_DIR})
      # IMPORTED_LINK_INTERFACE_LANGUAGES "C")
endif()
