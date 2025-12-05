cmake_minimum_required(VERSION 3.22)

project(ZELDA3_ANDROID)

# SDL2 library (vendored)
add_subdirectory(SDL2)

# SDL2_mixer library (vendored)
add_subdirectory(SDL2_mixer)

# Opus library (vendored) - Required for MSU audio (.opuz files)
add_subdirectory(SDL2_mixer/external/opus EXCLUDE_FROM_ALL)

# libyaml library (vendored) - Required for restool YAML parsing
add_subdirectory(yaml)

# Paths to main repo source code
set(ZELDA3_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../src)
set(ZELDA3_SNES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../snes)
set(ZELDA3_THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../third_party)

# Collect all game source files from main repo
file(GLOB GAME_SOURCES
    "${ZELDA3_SRC_DIR}/*.c"
    "${ZELDA3_SNES_DIR}/*.c"
)

# Android-specific platform files
set(ANDROID_SOURCES
    ${ZELDA3_SRC_DIR}/platform/android/android_jni.c
    ${ZELDA3_SRC_DIR}/platform/android/jni_helpers.c
)

# Restool sources for asset extraction
# Note: main.c included but RESTOOL_NO_MAIN disables CLI entry point
set(RESTOOL_SOURCES
    ${ZELDA3_SRC_DIR}/restool/main.c
    ${ZELDA3_SRC_DIR}/restool/restool_util.c
    ${ZELDA3_SRC_DIR}/restool/restool_lib.c
    ${ZELDA3_SRC_DIR}/restool/asset_compiler.c
    ${ZELDA3_SRC_DIR}/restool/asset_reader.c
    ${ZELDA3_SRC_DIR}/restool/extract_common.c
    ${ZELDA3_SRC_DIR}/restool/graphics.c
    ${ZELDA3_SRC_DIR}/restool/overworld.c
    ${ZELDA3_SRC_DIR}/restool/text.c
    ${ZELDA3_SRC_DIR}/restool/text_decode.c
    ${ZELDA3_SRC_DIR}/restool/yaml_util.c
    ${ZELDA3_SRC_DIR}/restool/tables.c
    ${ZELDA3_SRC_DIR}/restool/music_compiler.c
    ${ZELDA3_SRC_DIR}/restool/sprite_loader.c
    ${ZELDA3_SRC_DIR}/rom_sha1.c
    ${ZELDA3_THIRD_PARTY_DIR}/lodepng/lodepng.c
    ${ZELDA3_THIRD_PARTY_DIR}/sha256/sha256.c
)

# Create main shared library
add_library(main SHARED
    ${GAME_SOURCES}
    ${ANDROID_SOURCES}
    ${RESTOOL_SOURCES}
    ${ZELDA3_THIRD_PARTY_DIR}/gl_core/gl_core_3_1.c
)

# Include directories
target_include_directories(main PRIVATE
    ${ZELDA3_SRC_DIR}
    ${ZELDA3_SRC_DIR}/platform/android
    ${ZELDA3_SRC_DIR}/restool
    ${ZELDA3_SRC_DIR}/launcher
    ${ZELDA3_SNES_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../
    ${CMAKE_CURRENT_SOURCE_DIR}/SDL2/include
    ${CMAKE_CURRENT_SOURCE_DIR}/SDL2_mixer/include
    ${CMAKE_CURRENT_SOURCE_DIR}/SDL2_mixer/external/opus/include
    ${ZELDA3_THIRD_PARTY_DIR}/lodepng
    ${ZELDA3_THIRD_PARTY_DIR}/sha256
    ${ZELDA3_THIRD_PARTY_DIR}/stb
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Find Android system libraries
find_library(LOG_LIBRARY log)
find_library(ANDROID_LIBRARY android)
find_library(GLESV2_LIBRARY GLESv2)
find_library(OPENSLES_LIBRARY OpenSLES)

# Link libraries (order matches old working build)
target_link_libraries(main
    SDL2
    SDL2_mixer
    opus
    yaml
    GLESv1_CM
    GLESv2
    OpenSLES
    log
    android
)

# On Android API 24+, try to find Vulkan library (not available on API 21-23)
find_library(VULKAN_LIBRARY vulkan)
if(VULKAN_LIBRARY)
    message(STATUS "Found Vulkan library: ${VULKAN_LIBRARY}")
    target_link_libraries(main ${VULKAN_LIBRARY})
    target_compile_definitions(main PRIVATE VULKAN_AVAILABLE=1)
else()
    message(STATUS "Vulkan library not found - Vulkan renderer will not be available")
endif()

# Compiler flags
target_compile_options(main PRIVATE
    -O3
    -fvisibility=hidden
)

# Platform defines
target_compile_definitions(main PRIVATE
    PLATFORM_ANDROID=1
    RESTOOL_NO_MAIN=1
    NDEBUG
)
