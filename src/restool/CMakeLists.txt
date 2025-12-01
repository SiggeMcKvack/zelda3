cmake_minimum_required(VERSION 3.10)
project(zelda3_restool C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add cmake module path for FindYAML
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Find libyaml (required for YAML-based asset extraction)
find_package(YAML REQUIRED)

# Warning flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
  # Suppress warning for intentional use of GNU ## extension in variadic macros
  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wno-gnu-zero-variadic-macro-arguments)
  endif()
endif()

# ============================================================================
# Embedded Assets Generation
# ============================================================================

# Find Python for asset embedding script
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Output paths for generated files
set(EMBEDDED_ASSETS_C "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.c")
set(EMBEDDED_ASSETS_H "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.h")

# Collect all asset files (for dependency tracking)
file(GLOB_RECURSE ASSET_FILES
  "${CMAKE_SOURCE_DIR}/assets/dungeon/*.yaml"
  "${CMAKE_SOURCE_DIR}/assets/overworld/*.yaml"
  "${CMAKE_SOURCE_DIR}/assets/sound/*.brr"
  "${CMAKE_SOURCE_DIR}/assets/*.txt"
  "${CMAKE_SOURCE_DIR}/assets/*.yaml"
  "${CMAKE_SOURCE_DIR}/assets/*.png"
)

# Custom command to generate embedded assets
add_custom_command(
  OUTPUT ${EMBEDDED_ASSETS_C} ${EMBEDDED_ASSETS_H}
  COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/generate_embedded_assets.py
    ${CMAKE_SOURCE_DIR}/assets
    ${EMBEDDED_ASSETS_C}
    ${EMBEDDED_ASSETS_H}
  DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/generate_embedded_assets.py
    ${ASSET_FILES}
  COMMENT "Generating embedded assets..."
  VERBATIM
)

# Custom target to ensure assets are generated
add_custom_target(generate_embedded_assets
  DEPENDS ${EMBEDDED_ASSETS_C} ${EMBEDDED_ASSETS_H}
)

# ============================================================================
# Source Files
# ============================================================================

# Source files (in this directory)
set(RESTOOL_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/restool_lib.c
  ${CMAKE_CURRENT_SOURCE_DIR}/restool_util.c
  ${CMAKE_CURRENT_SOURCE_DIR}/graphics.c
  ${CMAKE_CURRENT_SOURCE_DIR}/asset_compiler.c
  ${CMAKE_CURRENT_SOURCE_DIR}/overworld.c
  ${CMAKE_CURRENT_SOURCE_DIR}/text.c
  ${CMAKE_CURRENT_SOURCE_DIR}/text_decode.c
  ${CMAKE_CURRENT_SOURCE_DIR}/yaml_util.c
  ${CMAKE_CURRENT_SOURCE_DIR}/tables.c
  ${CMAKE_CURRENT_SOURCE_DIR}/music_compiler.c
  ${CMAKE_CURRENT_SOURCE_DIR}/asset_reader.c
  ${CMAKE_CURRENT_SOURCE_DIR}/extract_common.c
  ${CMAKE_CURRENT_SOURCE_DIR}/sprite_loader.c
  ${EMBEDDED_ASSETS_C}
)

# Third-party vendored sources
set(THIRD_PARTY_SOURCES
  ${CMAKE_SOURCE_DIR}/third_party/sha256/sha256.c
  ${CMAKE_SOURCE_DIR}/third_party/lodepng/lodepng.c
)

# Shared utilities from main codebase
set(SHARED_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/../platform.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../logging.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../rom_sha1.c
)

# Create executable
add_executable(zelda3_restool ${RESTOOL_SOURCES} ${THIRD_PARTY_SOURCES} ${SHARED_SOURCES})

# Output to main build directory (same as zelda3 and zelda3-launcher)
set_target_properties(zelda3_restool PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Include directories
target_include_directories(zelda3_restool PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}              # For restool headers
  ${CMAKE_CURRENT_BINARY_DIR}              # For generated embedded_assets.h
  ${CMAKE_SOURCE_DIR}/third_party/stb      # For stb_image.h, stb_image_write.h
  ${CMAKE_SOURCE_DIR}/third_party/sha256   # For sha256.h
  ${CMAKE_SOURCE_DIR}/third_party/lodepng  # For lodepng.h
  ${YAML_INCLUDE_DIRS}                     # For yaml.h
)

# Link libraries
target_link_libraries(zelda3_restool PRIVATE
  ${YAML_LIBRARIES}
)

# Link math library on Unix
if(UNIX)
  target_link_libraries(zelda3_restool PRIVATE m)
endif()

# Install target
install(TARGETS zelda3_restool
  RUNTIME DESTINATION bin
)
