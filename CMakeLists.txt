cmake_minimum_required(VERSION 3.10)

project(zelda3 C)

# Add cmake modules directory
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Always generate compile_commands.json for tooling (clang-tidy, IDEs, etc.)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optional clang-tidy integration during build
option(ENABLE_CLANG_TIDY "Enable clang-tidy static analysis during build" OFF)
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
        set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXE}")
    else()
        message(WARNING "ENABLE_CLANG_TIDY is ON but clang-tidy not found")
    endif()
endif()

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

# Find glslang (optional, for slang shader support - requires Vulkan)
option(ENABLE_SLANG_SHADERS "Enable slang shader support (requires glslang)" ON)
if(HAVE_VULKAN AND ENABLE_SLANG_SHADERS)
    find_package(glslang CONFIG QUIET)
    if(glslang_FOUND)
        message(STATUS "Found glslang: slang shader support enabled")
        set(HAVE_SLANG_SHADERS ON)
    else()
        # Try pkg-config as fallback
        if(PkgConfig_FOUND)
            pkg_check_modules(GLSLANG QUIET glslang)
        endif()
        if(GLSLANG_FOUND)
            message(STATUS "Found glslang via pkg-config: slang shader support enabled")
            set(HAVE_SLANG_SHADERS ON)
        else()
            message(STATUS "glslang not found - slang shader support disabled")
            message(STATUS "  Install via: brew install glslang (macOS) or apt install glslang-dev (Linux)")
            set(HAVE_SLANG_SHADERS OFF)
        endif()
    endif()
else()
    set(HAVE_SLANG_SHADERS OFF)
endif()

# Option to build asset extraction tool
option(BUILD_RESTOOL "Build asset extraction tool" ON)

# Option to build PCM to OPUZ encoder
option(BUILD_OPUS_ENCODER "Build PCM to OPUZ encoder" ON)

# GTK-free launcher core (INI read/write + control mapping + FFI surface).
# Built as a static lib for the Tauri launcher backend to link via FFI.
option(BUILD_LAUNCHER_CORE "Build GTK-free launcher core static lib" ON)

# Find libyaml (required by restool)
find_package(YAML REQUIRED)
if(YAML_FOUND)
    message(STATUS "Found libyaml: ${YAML_LIBRARIES}")
endif()

# Collect all source files (launcher files now in separate directory)
file(GLOB GAME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/snes/*.c"
)

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
    # Suppress warning for intentional use of GNU ## extension in variadic macros
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(zelda3 PRIVATE -Wno-gnu-zero-variadic-macro-arguments)
    endif()
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

# Link glslang for slang shader support
if(HAVE_SLANG_SHADERS)
    if(glslang_FOUND)
        # CMake config-based glslang (modern approach)
        target_link_libraries(zelda3 PRIVATE
            glslang::glslang
            glslang::SPIRV
            glslang::glslang-default-resource-limits
        )
    else()
        # pkg-config based glslang
        target_include_directories(zelda3 PRIVATE ${GLSLANG_INCLUDE_DIRS})
        target_link_libraries(zelda3 PRIVATE ${GLSLANG_LIBRARIES})
    endif()
    target_compile_definitions(zelda3 PRIVATE SLANG_SHADERS_AVAILABLE=1)
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
# Generate config defaults header from zelda3.ini
# ==============================================================================

# Read INI file content
set(CONFIG_INI_PATH "${CMAKE_SOURCE_DIR}/zelda3.ini")
set(CONFIG_DEFAULTS_HEADER "${CMAKE_SOURCE_DIR}/src/config_defaults_generated.h")

file(READ "${CONFIG_INI_PATH}" INI_CONTENT)

# Escape special characters for C string literal
string(REPLACE "\\" "\\\\" INI_CONTENT "${INI_CONTENT}")
string(REPLACE "\"" "\\\"" INI_CONTENT "${INI_CONTENT}")
string(REPLACE "\n" "\\n\"\n    \"" INI_CONTENT "${INI_CONTENT}")

# Write generated header
file(WRITE "${CONFIG_DEFAULTS_HEADER}"
"#ifndef CONFIG_DEFAULTS_GENERATED_H
#define CONFIG_DEFAULTS_GENERATED_H

// Auto-generated from zelda3.ini at build time
// Do not edit this file directly - edit zelda3.ini instead

const char* kDefaultConfigIni =
    \"${INI_CONTENT}\";

#endif // CONFIG_DEFAULTS_GENERATED_H
")

message(STATUS "Generated config defaults header from zelda3.ini")

# ==============================================================================
# Asset extraction tool (restool library and CLI)
# ==============================================================================
# Build restool first so launcher can link against restool_lib
if(BUILD_RESTOOL)
    add_subdirectory(src/restool)
    message(STATUS "Asset extraction tool will be built: zelda3-restool")
endif()

# ==============================================================================
# PCM to OPUZ encoder tool
# ==============================================================================
if(BUILD_OPUS_ENCODER)
    # Find libsamplerate (required for resampling)
    find_package(SampleRate)
    if(SAMPLERATE_FOUND)
        add_subdirectory(src/opus_encoder)
        message(STATUS "OPUZ encoder tool will be built: zelda3_opus_encoder")
    else()
        message(STATUS "libsamplerate not found - opus_encoder will not be built")
        message(STATUS "Install libsamplerate-dev or use: brew install libsamplerate")
    endif()
endif()

# ==============================================================================
# GTK-free launcher core static library (Tauri launcher backend / FFI)
# ==============================================================================
if(BUILD_LAUNCHER_CORE)
    if(BUILD_RESTOOL AND TARGET restool_lib)
        add_subdirectory(src/launcher_core)
        message(STATUS "Launcher core lib will be built: launcher_core_lib")
    else()
        message(STATUS "launcher_core requires BUILD_RESTOOL=ON - skipping")
    endif()
endif()

# ==============================================================================
# Unit Tests (optional)
# ==============================================================================
option(BUILD_TESTS "Build unit tests" OFF)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
    message(STATUS "Unit tests will be built")
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
message(STATUS "Slang Shaders: ${HAVE_SLANG_SHADERS}")
message(STATUS "Tests: ${BUILD_TESTS}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "===================================")
message(STATUS "")
