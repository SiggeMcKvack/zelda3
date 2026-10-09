# ==============================================================================
# Launcher Module Unit Tests
# ==============================================================================
# Tests for launcher components. We compile source files directly with
# TEST_BUILD defined to expose static functions for testing.
# ==============================================================================

# Common include directories for all launcher tests
set(LAUNCHER_TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/launcher
    ${CMAKE_SOURCE_DIR}/src/launcher_core
    ${TEST_UTILS_INCLUDE}
    ${SDL2_INCLUDE_DIRS}
)

# GTK-free launcher core sources (real keymap storage + control-string helpers)
set(LAUNCHER_CORE_TEST_SOURCES
    ${CMAKE_SOURCE_DIR}/src/launcher_core/keymap_state.c
    ${CMAKE_SOURCE_DIR}/src/launcher_core/control_strings.c
)

# ==============================================================================
# Test: config_writer
# ==============================================================================
add_executable(test_config_writer
    test_config_writer.c
    ${LAUNCHER_CORE_TEST_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/launcher/config_writer.c
    ${CMAKE_SOURCE_DIR}/src/logging.c
)

target_include_directories(test_config_writer PRIVATE ${LAUNCHER_TEST_INCLUDES})
target_link_libraries(test_config_writer PRIVATE unity)
target_compile_definitions(test_config_writer PRIVATE TEST_BUILD)

add_test(NAME config_writer COMMAND test_config_writer)

# ==============================================================================
# Test: config_reader
# ==============================================================================
add_executable(test_config_reader
    test_config_reader.c
    ${LAUNCHER_CORE_TEST_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/launcher/config_reader.c
    ${CMAKE_SOURCE_DIR}/src/launcher/config_writer.c
    ${CMAKE_SOURCE_DIR}/src/logging.c
    ${CMAKE_SOURCE_DIR}/src/platform.c
)

target_include_directories(test_config_reader PRIVATE ${LAUNCHER_TEST_INCLUDES})
# Use SDL2::SDL2 (not ${SDL2_LIBRARIES}) to avoid linking SDL2main which conflicts with SDL_MAIN_HANDLED
target_link_libraries(test_config_reader PRIVATE unity SDL2::SDL2)
target_compile_definitions(test_config_reader PRIVATE TEST_BUILD)

add_test(NAME config_reader COMMAND test_config_reader)

# ==============================================================================
# Test: launcher_ffi (GTK-free FFI round-trip against launcher_core_lib)
# Proves the JSON marshalling reproduces the canonical INI byte-for-byte.
# ==============================================================================
if(TARGET launcher_core_lib)
    add_executable(test_launcher_ffi test_launcher_ffi.c)
    target_include_directories(test_launcher_ffi PRIVATE ${LAUNCHER_TEST_INCLUDES})
    target_link_libraries(test_launcher_ffi PRIVATE unity launcher_core_lib)
    target_compile_definitions(test_launcher_ffi PRIVATE
        TEST_BUILD
        ZELDA3_INI_PATH="${CMAKE_SOURCE_DIR}/zelda3.ini"
    )
    add_test(NAME launcher_ffi COMMAND test_launcher_ffi)
endif()
