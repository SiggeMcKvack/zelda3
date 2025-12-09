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
    ${TEST_UTILS_INCLUDE}
    ${SDL2_INCLUDE_DIRS}
)

# ==============================================================================
# Test: config_writer (uses stub file for launcher_ui dependencies)
# Note: Requires GTK3 headers even though we use stubs, because headers include gtk.h
# ==============================================================================
if(HAVE_GTK3)
    add_executable(test_config_writer
        test_config_writer.c
        launcher_ui_stubs.c
        ${CMAKE_SOURCE_DIR}/src/launcher/config_writer.c
        ${CMAKE_SOURCE_DIR}/src/logging.c
    )

    target_include_directories(test_config_writer PRIVATE
        ${LAUNCHER_TEST_INCLUDES}
        ${GTK3_INCLUDE_DIRS}
    )
    target_link_libraries(test_config_writer PRIVATE unity)
    target_compile_definitions(test_config_writer PRIVATE TEST_BUILD)
    target_compile_options(test_config_writer PRIVATE ${GTK3_CFLAGS_OTHER})

    add_test(NAME config_writer COMMAND test_config_writer)
endif()

# ==============================================================================
# Test: launcher_gamepad (requires SDL2 for types only)
# ==============================================================================
add_executable(test_launcher_gamepad
    test_launcher_gamepad.c
    ${CMAKE_SOURCE_DIR}/src/launcher/launcher_gamepad.c
    ${CMAKE_SOURCE_DIR}/src/logging.c
)

target_include_directories(test_launcher_gamepad PRIVATE ${LAUNCHER_TEST_INCLUDES})
# Use SDL2::SDL2 (not ${SDL2_LIBRARIES}) to avoid linking SDL2main which conflicts with SDL_MAIN_HANDLED
target_link_libraries(test_launcher_gamepad PRIVATE unity SDL2::SDL2)
target_compile_definitions(test_launcher_gamepad PRIVATE TEST_BUILD)

add_test(NAME launcher_gamepad COMMAND test_launcher_gamepad)

# ==============================================================================
# Test: config_reader
# Uses stubs to avoid full launcher_ui dependencies
# ==============================================================================
if(HAVE_GTK3)
    add_executable(test_config_reader
        test_config_reader.c
        launcher_ui_stubs.c
        ${CMAKE_SOURCE_DIR}/src/launcher/config_reader.c
        ${CMAKE_SOURCE_DIR}/src/launcher/config_writer.c
        ${CMAKE_SOURCE_DIR}/src/logging.c
        ${CMAKE_SOURCE_DIR}/src/platform.c
    )

    target_include_directories(test_config_reader PRIVATE
        ${LAUNCHER_TEST_INCLUDES}
        ${GTK3_INCLUDE_DIRS}
    )
    # Use SDL2::SDL2 (not ${SDL2_LIBRARIES}) to avoid linking SDL2main which conflicts with SDL_MAIN_HANDLED
    target_link_libraries(test_config_reader PRIVATE
        unity
        SDL2::SDL2
    )
    target_compile_definitions(test_config_reader PRIVATE TEST_BUILD)
    target_compile_options(test_config_reader PRIVATE ${GTK3_CFLAGS_OTHER})

    add_test(NAME config_reader COMMAND test_config_reader)
endif()

# ==============================================================================
# Test: launcher_ui
# Note: Disabled for now due to complex GTK/restool dependencies
# The string utility functions are tested indirectly via config tests
# ==============================================================================
# TODO: Re-enable when we can properly isolate launcher_ui functions
# if(HAVE_GTK3 AND TARGET restool_lib)
#     add_executable(test_launcher_ui ...)
# endif()
