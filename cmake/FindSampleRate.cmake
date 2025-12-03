# FindSampleRate.cmake - Find libsamplerate library
#
# This module defines:
#  SAMPLERATE_FOUND - System has libsamplerate
#  SAMPLERATE_INCLUDE_DIRS - libsamplerate include directories
#  SAMPLERATE_LIBRARIES - Libraries needed to use libsamplerate
#
# Uses pkg-config to find libsamplerate

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_SAMPLERATE QUIET samplerate)
endif()

find_path(SAMPLERATE_INCLUDE_DIR
    NAMES samplerate.h
    HINTS ${PC_SAMPLERATE_INCLUDEDIR} ${PC_SAMPLERATE_INCLUDE_DIRS}
    PATH_SUFFIXES include
)

find_library(SAMPLERATE_LIBRARY
    NAMES samplerate samplerate-0
    HINTS ${PC_SAMPLERATE_LIBDIR} ${PC_SAMPLERATE_LIBRARY_DIRS}
    PATH_SUFFIXES lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SampleRate
    REQUIRED_VARS SAMPLERATE_LIBRARY SAMPLERATE_INCLUDE_DIR
    VERSION_VAR PC_SAMPLERATE_VERSION
)

if(SAMPLERATE_FOUND)
    set(SAMPLERATE_LIBRARIES ${SAMPLERATE_LIBRARY})
    set(SAMPLERATE_INCLUDE_DIRS ${SAMPLERATE_INCLUDE_DIR})

    if(NOT TARGET SampleRate::samplerate)
        add_library(SampleRate::samplerate UNKNOWN IMPORTED)
        set_target_properties(SampleRate::samplerate PROPERTIES
            IMPORTED_LOCATION "${SAMPLERATE_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SAMPLERATE_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(SAMPLERATE_INCLUDE_DIR SAMPLERATE_LIBRARY)
