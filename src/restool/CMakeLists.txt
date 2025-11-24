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
endif()

# Source files (in this directory)
set(RESTOOL_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/restool_util.c
  ${CMAKE_CURRENT_SOURCE_DIR}/graphics.c
  ${CMAKE_CURRENT_SOURCE_DIR}/asset_compiler.c
  ${CMAKE_CURRENT_SOURCE_DIR}/overworld.c
  ${CMAKE_CURRENT_SOURCE_DIR}/text.c
  ${CMAKE_CURRENT_SOURCE_DIR}/yaml_util.c
  ${CMAKE_CURRENT_SOURCE_DIR}/tables.c
  # Add more as we create them:
  # ${CMAKE_CURRENT_SOURCE_DIR}/dungeon.c
  # ${CMAKE_CURRENT_SOURCE_DIR}/sprites.c
  # ${CMAKE_CURRENT_SOURCE_DIR}/music.c
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
)

# Create executable
add_executable(zelda3_restool ${RESTOOL_SOURCES} ${THIRD_PARTY_SOURCES} ${SHARED_SOURCES})

# Include directories
target_include_directories(zelda3_restool PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}              # For restool headers
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
