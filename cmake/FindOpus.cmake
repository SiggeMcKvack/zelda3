# FindOpus.cmake - Find Opus audio codec library
#
# This module defines:
#  OPUS_FOUND - System has Opus
#  OPUS_INCLUDE_DIRS - Opus include directories
#  OPUS_LIBRARIES - Libraries needed to use Opus
#
# Uses pkg-config to find Opus

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_OPUS QUIET opus)
endif()

find_path(OPUS_INCLUDE_DIR
    NAMES opus/opus.h
    HINTS ${PC_OPUS_INCLUDEDIR} ${PC_OPUS_INCLUDE_DIRS}
    PATH_SUFFIXES include
)

find_library(OPUS_LIBRARY
    NAMES opus
    HINTS ${PC_OPUS_LIBDIR} ${PC_OPUS_LIBRARY_DIRS}
    PATH_SUFFIXES lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Opus
    REQUIRED_VARS OPUS_LIBRARY OPUS_INCLUDE_DIR
    VERSION_VAR PC_OPUS_VERSION
)

if(OPUS_FOUND)
    set(OPUS_LIBRARIES ${OPUS_LIBRARY})
    set(OPUS_INCLUDE_DIRS ${OPUS_INCLUDE_DIR})

    if(NOT TARGET Opus::opus)
        add_library(Opus::opus UNKNOWN IMPORTED)
        set_target_properties(Opus::opus PROPERTIES
            IMPORTED_LOCATION "${OPUS_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OPUS_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(OPUS_INCLUDE_DIR OPUS_LIBRARY)
