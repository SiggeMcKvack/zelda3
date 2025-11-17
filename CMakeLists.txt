cmake_minimum_required(VERSION 3.10)

project(zelda3 C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build type defaults to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Find SDL2 (required)
find_package(SDL2 REQUIRED)
if(SDL2_FOUND)
    message(STATUS "Found SDL2: ${SDL2_LIBRARIES}")
endif()

# Find OpenGL (optional, for OpenGL renderer)
find_package(OpenGL)
if(OPENGL_FOUND)
    message(STATUS "Found OpenGL: ${OPENGL_LIBRARIES}")
    set(HAVE_OPENGL ON)
else()
    message(STATUS "OpenGL not found - OpenGL renderer will be disabled")
    set(HAVE_OPENGL OFF)
endif()

# Find Vulkan (optional, for Vulkan renderer)
find_package(Vulkan)
if(Vulkan_FOUND)
    message(STATUS "Found Vulkan: ${Vulkan_LIBRARIES}")
    set(HAVE_VULKAN ON)
else()
    message(STATUS "Vulkan not found - Vulkan renderer will be disabled")
    set(HAVE_VULKAN OFF)
endif()

# Collect all source files
file(GLOB GAME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/snes/*.c"
)

# Third-party sources
set(THIRD_PARTY_SOURCES
    third_party/gl_core/gl_core_3_1.c
    third_party/opus-1.3.1-stripped/opus_decoder_amalgam.c
)

# Create executable
add_executable(zelda3
    ${GAME_SOURCES}
    ${THIRD_PARTY_SOURCES}
)

# Include directories
target_include_directories(zelda3 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SDL2_INCLUDE_DIRS}
)

# Compiler flags
if(MSVC)
    target_compile_options(zelda3 PRIVATE
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Debug>:/Od /Zi>
        /W3
    )
    target_compile_definitions(zelda3 PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
    )
else()
    target_compile_options(zelda3 PRIVATE
        $<$<CONFIG:Release>:-O2>
        $<$<CONFIG:Debug>:-O0 -g>
        -Wall
    )
endif()

# Optional: Enable -Werror (disabled by default due to pre-existing warnings)
# To enable: cmake .. -DENABLE_WERROR=ON
option(ENABLE_WERROR "Treat warnings as errors" OFF)
if(ENABLE_WERROR)
    target_compile_options(zelda3 PRIVATE -Werror)
endif()

# Compiler definitions
target_compile_definitions(zelda3 PRIVATE
    SYSTEM_VOLUME_MIXER_AVAILABLE=0
)

if(CMAKE_BUILD_TYPE MATCHES Release)
    target_compile_definitions(zelda3 PRIVATE NDEBUG)
endif()

# Platform-specific definitions
if(APPLE)
    target_compile_definitions(zelda3 PRIVATE _THREAD_SAFE)
endif()

# Link libraries
target_link_libraries(zelda3 PRIVATE
    ${SDL2_LIBRARIES}
)

# Link math library on Unix-like systems
if(UNIX AND NOT APPLE)
    target_link_libraries(zelda3 PRIVATE m)
endif()

# Link OpenGL if available
if(HAVE_OPENGL)
    target_link_libraries(zelda3 PRIVATE ${OPENGL_LIBRARIES})
endif()

# Link Vulkan if available
if(HAVE_VULKAN)
    target_link_libraries(zelda3 PRIVATE Vulkan::Vulkan)
    target_compile_definitions(zelda3 PRIVATE VULKAN_AVAILABLE=1)
endif()

# Windows-specific libraries
if(WIN32)
    target_link_libraries(zelda3 PRIVATE winmm)
endif()

# macOS-specific settings
if(APPLE)
    # On macOS, may need to link against frameworks
    target_link_libraries(zelda3 PRIVATE
        "-framework CoreFoundation"
        "-framework CoreAudio"
        "-framework AudioToolbox"
    )
endif()

# Installation
install(TARGETS zelda3 DESTINATION bin)

# Print configuration summary
message(STATUS "")
message(STATUS "=== Zelda3 Build Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "SDL2 Found: ${SDL2_FOUND}")
message(STATUS "OpenGL Found: ${HAVE_OPENGL}")
message(STATUS "Vulkan Found: ${HAVE_VULKAN}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "===================================")
message(STATUS "")
