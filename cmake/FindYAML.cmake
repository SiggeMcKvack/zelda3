# FindYAML.cmake - Find libyaml library
#
# This module defines:
#  YAML_FOUND - System has libyaml
#  YAML_INCLUDE_DIRS - The libyaml include directories
#  YAML_LIBRARIES - The libraries needed to use libyaml
#  YAML_VERSION - The version of libyaml found

# Try pkg-config first (works on Linux/macOS with package managers)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_YAML QUIET yaml-0.1)
endif()

# Find include directory
find_path(YAML_INCLUDE_DIR
  NAMES yaml.h
  HINTS ${PC_YAML_INCLUDEDIR} ${PC_YAML_INCLUDE_DIRS}
  PATH_SUFFIXES yaml
)

# Find library
find_library(YAML_LIBRARY
  NAMES yaml libyaml yaml-0.1
  HINTS ${PC_YAML_LIBDIR} ${PC_YAML_LIBRARY_DIRS}
)

# Get version from pkg-config if available
if(PC_YAML_VERSION)
  set(YAML_VERSION ${PC_YAML_VERSION})
endif()

# Handle standard args
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(YAML
  REQUIRED_VARS YAML_LIBRARY YAML_INCLUDE_DIR
  VERSION_VAR YAML_VERSION
)

# Set output variables
if(YAML_FOUND)
  set(YAML_LIBRARIES ${YAML_LIBRARY})
  set(YAML_INCLUDE_DIRS ${YAML_INCLUDE_DIR})

  # Create imported target for modern CMake
  if(NOT TARGET YAML::YAML)
    add_library(YAML::YAML UNKNOWN IMPORTED)
    set_target_properties(YAML::YAML PROPERTIES
      IMPORTED_LOCATION "${YAML_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${YAML_INCLUDE_DIR}"
    )
  endif()
endif()

# Mark cache variables as advanced
mark_as_advanced(YAML_INCLUDE_DIR YAML_LIBRARY)
