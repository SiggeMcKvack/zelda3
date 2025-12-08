# ==============================================================================
# Zelda3 Unit Tests - Main Configuration
# ==============================================================================
# This file sets up the test framework and includes module-specific test targets.
# Add new test modules by adding add_subdirectory() calls below.
# ==============================================================================

cmake_minimum_required(VERSION 3.10)

# Unity test framework library
add_library(unity STATIC
    ${CMAKE_SOURCE_DIR}/third_party/unity/unity.c
)

target_include_directories(unity PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/unity
)

# Compile with TEST_BUILD to expose internal functions
target_compile_definitions(unity PUBLIC TEST_BUILD)

# Common test utilities header path
set(TEST_UTILS_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "Test utilities include dir")

# ==============================================================================
# Module test directories
# ==============================================================================
# Each module has its own CMakeLists.txt defining test targets

# Launcher module tests
add_subdirectory(launcher)

# Future modules (uncomment when tests are added):
# add_subdirectory(restool)
# add_subdirectory(opus_encoder)
# add_subdirectory(snes)
# add_subdirectory(core)
