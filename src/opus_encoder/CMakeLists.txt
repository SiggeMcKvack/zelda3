cmake_minimum_required(VERSION 3.10)
project(zelda3_opus_encoder C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add cmake module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Find required libraries
find_package(Opus REQUIRED)
find_package(SampleRate REQUIRED)

# Warning flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wno-gnu-zero-variadic-macro-arguments)
  endif()
endif()

# ============================================================================
# Static Library
# ============================================================================

set(OPUS_ENCODER_LIB_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/opus_encoder_lib.c
)

# Shared utilities from main codebase
set(SHARED_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/../platform.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../logging.c
)

# Create static library
add_library(opus_encoder_lib STATIC
  ${OPUS_ENCODER_LIB_SOURCES}
  ${SHARED_SOURCES}
)

target_include_directories(opus_encoder_lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/src
  ${OPUS_INCLUDE_DIRS}
  ${SAMPLERATE_INCLUDE_DIRS}
)

target_link_libraries(opus_encoder_lib PUBLIC
  ${OPUS_LIBRARIES}
  ${SAMPLERATE_LIBRARIES}
)

# Link math library on Unix
if(UNIX)
  target_link_libraries(opus_encoder_lib PUBLIC m)
endif()

# Export for parent CMakeLists.txt
set(OPUS_ENCODER_LIB_TARGET opus_encoder_lib PARENT_SCOPE)

# ============================================================================
# CLI Executable
# ============================================================================

set(CLI_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/cli.c
)

add_executable(zelda3_opus_encoder ${CLI_SOURCES})

target_link_libraries(zelda3_opus_encoder PRIVATE opus_encoder_lib)

# Output to main build directory
set_target_properties(zelda3_opus_encoder PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Install target
install(TARGETS zelda3_opus_encoder
  RUNTIME DESTINATION bin
)
