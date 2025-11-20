cmake_minimum_required(VERSION 3.10)

project(zelda3 C)

# Add cmake modules directory
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build type defaults to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Find SDL2 (required)
find_package(SDL2 REQUIRED)
if(SDL2_FOUND)
    message(STATUS "Found SDL2: ${SDL2_LIBRARIES}")
endif()

# Find Opus (required for MSU audio support)
find_package(Opus REQUIRED)
if(Opus_FOUND)
    message(STATUS "Found Opus: ${OPUS_LIBRARIES}")
endif()

# Find OpenGL (optional, for OpenGL renderer)
find_package(OpenGL)
if(OPENGL_FOUND)
    message(STATUS "Found OpenGL: ${OPENGL_LIBRARIES}")
    set(HAVE_OPENGL ON)
else()
    message(STATUS "OpenGL not found - OpenGL renderer will be disabled")
    set(HAVE_OPENGL OFF)
endif()

# Find Vulkan (optional, for Vulkan renderer)
find_package(Vulkan)
if(Vulkan_FOUND)
    message(STATUS "Found Vulkan: ${Vulkan_LIBRARIES}")
    set(HAVE_VULKAN ON)
else()
    message(STATUS "Vulkan not found - Vulkan renderer will be disabled")
    set(HAVE_VULKAN OFF)
endif()

# Option to build launcher (manual override)
option(BUILD_LAUNCHER "Build GTK3 launcher" ON)

# Find GTK3 (optional, for settings launcher)
if(BUILD_LAUNCHER)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        # On macOS arm64, prefer Homebrew in /opt/homebrew over /usr/local
        if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            set(ENV{PKG_CONFIG_PATH} "/opt/homebrew/lib/pkgconfig:/opt/homebrew/share/pkgconfig:$ENV{PKG_CONFIG_PATH}")
        endif()

        pkg_check_modules(GTK3 QUIET gtk+-3.0)
        if(GTK3_FOUND)
            message(STATUS "Found GTK3: ${GTK3_VERSION}")
            set(HAVE_GTK3 ON)
        else()
            message(STATUS "GTK3 not found - launcher will not be built")
            set(HAVE_GTK3 OFF)
        endif()
    else()
        message(STATUS "pkg-config not found - cannot detect GTK3")
        set(HAVE_GTK3 OFF)
    endif()
else()
    message(STATUS "Launcher build disabled via BUILD_LAUNCHER=OFF")
    set(HAVE_GTK3 OFF)
endif()

# Collect all source files (exclude launcher files)
file(GLOB GAME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/snes/*.c"
)

# Exclude launcher files from main game
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*launcher_.*\.c$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*/config_reader\.c$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*/config_writer\.c$")

# Third-party sources
set(THIRD_PARTY_SOURCES
    third_party/gl_core/gl_core_3_1.c
)

# Create executable
add_executable(zelda3
    ${GAME_SOURCES}
    ${THIRD_PARTY_SOURCES}
)

# Include directories
target_include_directories(zelda3 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SDL2_INCLUDE_DIRS}
    ${OPUS_INCLUDE_DIRS}
)

# Compiler flags
if(MSVC)
    target_compile_options(zelda3 PRIVATE
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Debug>:/Od /Zi>
        /W3
        /std:c17
    )
    target_compile_definitions(zelda3 PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
    )
else()
    target_compile_options(zelda3 PRIVATE
        $<$<CONFIG:Release>:-O2>
        $<$<CONFIG:Debug>:-O0 -g>
        -Wall
    )
endif()

# Optional: Enable -Werror (disabled by default due to pre-existing warnings)
# To enable: cmake .. -DENABLE_WERROR=ON
option(ENABLE_WERROR "Treat warnings as errors" OFF)
if(ENABLE_WERROR)
    target_compile_options(zelda3 PRIVATE -Werror)
endif()

# Compiler definitions
target_compile_definitions(zelda3 PRIVATE
    SYSTEM_VOLUME_MIXER_AVAILABLE=0
)

if(CMAKE_BUILD_TYPE MATCHES Release)
    target_compile_definitions(zelda3 PRIVATE NDEBUG)
endif()

# Platform-specific definitions
if(APPLE)
    target_compile_definitions(zelda3 PRIVATE _THREAD_SAFE)
endif()

# Link libraries
target_link_libraries(zelda3 PRIVATE
    ${SDL2_LIBRARIES}
    ${OPUS_LIBRARIES}
)

# Link math library on Unix-like systems
if(UNIX AND NOT APPLE)
    target_link_libraries(zelda3 PRIVATE m)
endif()

# Link OpenGL if available
if(HAVE_OPENGL)
    target_link_libraries(zelda3 PRIVATE ${OPENGL_LIBRARIES})
endif()

# Link Vulkan if available
if(HAVE_VULKAN)
    target_link_libraries(zelda3 PRIVATE Vulkan::Vulkan)
    target_compile_definitions(zelda3 PRIVATE VULKAN_AVAILABLE=1)
endif()

# Windows-specific libraries
if(WIN32)
    target_link_libraries(zelda3 PRIVATE winmm)
endif()

# macOS-specific settings
if(APPLE)
    # On macOS, may need to link against frameworks
    target_link_libraries(zelda3 PRIVATE
        "-framework CoreFoundation"
        "-framework CoreAudio"
        "-framework AudioToolbox"
    )
endif()

# Installation
install(TARGETS zelda3 DESTINATION bin)

# ==============================================================================
# Launcher target (optional, requires GTK3)
# ==============================================================================

if(HAVE_GTK3)
    # Launcher source files (will add more as we implement)
    set(LAUNCHER_SOURCES
        src/launcher_main.c
        src/launcher_ui.c
        src/launcher_gamepad.c
        src/config_writer.c
        src/config_reader.c
        src/platform.c
        src/logging.c
    )

    # Add macOS-specific source
    if(APPLE)
        list(APPEND LAUNCHER_SOURCES src/platform/macos/launcher_macos.m)
    endif()

    # Create launcher executable
    add_executable(zelda3-launcher ${LAUNCHER_SOURCES})

    # Include directories for launcher
    target_include_directories(zelda3-launcher PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${SDL2_INCLUDE_DIRS}
        ${GTK3_INCLUDE_DIRS}
    )

    # Compiler flags for launcher
    if(MSVC)
        target_compile_options(zelda3-launcher PRIVATE
            $<$<CONFIG:Release>:/O2>
            $<$<CONFIG:Debug>:/Od /Zi>
            /W3
            /std:c17
        )
        target_compile_definitions(zelda3-launcher PRIVATE
            _CRT_SECURE_NO_WARNINGS
            _CRT_NONSTDC_NO_DEPRECATE
        )
    else()
        target_compile_options(zelda3-launcher PRIVATE
            $<$<CONFIG:Release>:-O2>
            $<$<CONFIG:Debug>:-O0 -g>
            -Wall
        )
    endif()

    # Link GTK3 and SDL2 for launcher
    target_link_libraries(zelda3-launcher PRIVATE
        ${SDL2_LIBRARIES}
        ${GTK3_LIBRARIES}
    )

    # Link directories for GTK3
    target_link_directories(zelda3-launcher PRIVATE
        ${GTK3_LIBRARY_DIRS}
    )

    # Add GTK3 compile flags
    target_compile_options(zelda3-launcher PRIVATE ${GTK3_CFLAGS_OTHER})

    # Platform-specific settings for launcher
    if(UNIX AND NOT APPLE)
        target_link_libraries(zelda3-launcher PRIVATE m)
    endif()

    if(APPLE)
        target_compile_definitions(zelda3-launcher PRIVATE _THREAD_SAFE)
        target_link_libraries(zelda3-launcher PRIVATE "-framework Cocoa")
    endif()

    # Install launcher
    install(TARGETS zelda3-launcher DESTINATION bin)

    message(STATUS "Launcher target will be built: zelda3-launcher")
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "=== Zelda3 Build Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "SDL2 Found: ${SDL2_FOUND}")
message(STATUS "Opus Found: ${Opus_FOUND}")
message(STATUS "OpenGL Found: ${HAVE_OPENGL}")
message(STATUS "Vulkan Found: ${HAVE_VULKAN}")
message(STATUS "GTK3 Found: ${HAVE_GTK3}")
message(STATUS "Launcher: ${HAVE_GTK3}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "===================================")
message(STATUS "")
