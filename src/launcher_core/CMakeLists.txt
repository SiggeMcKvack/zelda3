# ==============================================================================
# launcher_core_lib - GTK-free launcher core
# ==============================================================================
# INI read/write, keyboard/gamepad control-mapping storage, control-string
# helpers, and the C-ABI FFI surface consumed by the Tauri launcher backend.
# Deliberately free of GTK so it can be linked into the Rust/Tauri binary.
# ==============================================================================

set(LAUNCHER_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(LAUNCHER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../launcher)
set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

add_library(launcher_core_lib STATIC
    ${LAUNCHER_CORE_DIR}/keymap_state.c
    ${LAUNCHER_CORE_DIR}/control_strings.c
    ${LAUNCHER_CORE_DIR}/default_config_ini.c
    ${LAUNCHER_CORE_DIR}/launcher_ffi.c
    ${LAUNCHER_DIR}/config_reader.c
    ${LAUNCHER_DIR}/config_writer.c
    ${REPO_ROOT}/third_party/cjson/cJSON.c
)

target_include_directories(launcher_core_lib PUBLIC
    ${LAUNCHER_CORE_DIR}
    ${LAUNCHER_DIR}                       # config_reader.h / config_writer.h
    ${REPO_ROOT}/src                      # config.h, types.h
    ${REPO_ROOT}/third_party/cjson
    ${SDL2_INCLUDE_DIRS}                  # config.h includes <SDL_keycode.h>
)

# restool_lib provides Platform_ReadWholeFile (platform.c) and LogError
# (logging.c) plus the restool headers used by later FFI wrappers.
target_link_libraries(launcher_core_lib PUBLIC restool_lib)

# Optional: opus encoder (MSU .pcm -> .opuz). Linked only if it was built.
if(TARGET opus_encoder_lib)
    target_link_libraries(launcher_core_lib PUBLIC opus_encoder_lib)
    target_include_directories(launcher_core_lib PRIVATE ${REPO_ROOT}/src/opus_encoder)
    target_compile_definitions(launcher_core_lib PRIVATE HAVE_OPUS_ENCODER)
endif()

if(NOT MSVC)
    target_compile_options(launcher_core_lib PRIVATE
        $<$<CONFIG:Release>:-O2>
        $<$<CONFIG:Debug>:-O0 -g>
        -Wall
    )
endif()
