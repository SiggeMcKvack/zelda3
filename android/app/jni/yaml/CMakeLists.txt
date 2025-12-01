# Simplified CMakeLists.txt for libyaml 0.2.5 - Android build only
# Removed: tests, install rules, cmake package config (not needed for embedded use)

cmake_minimum_required(VERSION 3.10)
project(yaml C)

set(YAML_VERSION_MAJOR 0)
set(YAML_VERSION_MINOR 2)
set(YAML_VERSION_PATCH 5)
set(YAML_VERSION_STRING "${YAML_VERSION_MAJOR}.${YAML_VERSION_MINOR}.${YAML_VERSION_PATCH}")

# Build as static library only
add_library(yaml STATIC
    src/api.c
    src/dumper.c
    src/emitter.c
    src/loader.c
    src/parser.c
    src/reader.c
    src/scanner.c
    src/writer.c
)

# Generate config.h
set(config_h ${CMAKE_CURRENT_BINARY_DIR}/include/config.h)
configure_file(cmake/config.h.in ${config_h})

# Include directories
target_include_directories(yaml PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

# Compile definitions
target_compile_definitions(yaml
    PRIVATE HAVE_CONFIG_H
    PUBLIC YAML_DECLARE_STATIC
)
