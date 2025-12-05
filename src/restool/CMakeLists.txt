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
# Static Library (shared between CLI and launcher)
# ============================================================================

# Library source files (everything except CLI entry point)
set(RESTOOL_LIB_SOURCES
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

# Create static library
add_library(restool_lib STATIC
  ${RESTOOL_LIB_SOURCES}
  ${THIRD_PARTY_SOURCES}
  ${SHARED_SOURCES}
)

# Include directories for the library
target_include_directories(restool_lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}              # For restool headers
  ${CMAKE_SOURCE_DIR}/third_party/stb      # For stb_image.h, stb_image_write.h
  ${CMAKE_SOURCE_DIR}/third_party/sha256   # For sha256.h
  ${CMAKE_SOURCE_DIR}/third_party/lodepng  # For lodepng.h
  ${YAML_INCLUDE_DIRS}                     # For yaml.h
)

# Link libraries for the static lib
target_link_libraries(restool_lib PUBLIC
  ${YAML_LIBRARIES}
)

# Link math library on Unix
if(UNIX)
  target_link_libraries(restool_lib PUBLIC m)
endif()

# Export variables for parent CMakeLists.txt to use
set(RESTOOL_LIB_TARGET restool_lib PARENT_SCOPE)
set(RESTOOL_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/third_party/stb
  ${CMAKE_SOURCE_DIR}/third_party/sha256
  ${CMAKE_SOURCE_DIR}/third_party/lodepng
  ${YAML_INCLUDE_DIRS}
  PARENT_SCOPE
)

# ============================================================================
# CLI Executable
# ============================================================================

# CLI source file (entry point)
set(CLI_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/cli.c
)

# Create executable linking against the library
add_executable(zelda3_restool ${CLI_SOURCES})

# Link against the static library
target_link_libraries(zelda3_restool PRIVATE restool_lib)

# Output to main build directory (same as zelda3 and zelda3-launcher)
set_target_properties(zelda3_restool PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  OUTPUT_NAME "zelda3-restool"
)

# Install target
install(TARGETS zelda3_restool
  RUNTIME DESTINATION bin
)
