# CLAUDE.md

This file provides AI-specific guidance for Claude Code when working with this repository.

**Note:** For personal AI development notes, use `CLAUDE.local.md` (git-ignored).

## Table of Contents

- [Project Overview](#project-overview)
- [Quick Start for Development](#quick-start-for-development)
- [Critical Architecture Concepts](#critical-architecture-concepts)
- [Module Organization](#module-organization)
- [Development Patterns](#development-patterns)
- [File Locations](#file-locations)
- [Common Tasks](#common-tasks)
- [Important Notes](#important-notes)
- [Recent Changes](#recent-changes)
- [Getting Help](#getting-help)

## Project Overview

Zelda3 is a reverse-engineered C reimplementation of The Legend of Zelda: A Link to the Past. ~70-80kLOC of C code reimplementing all game logic, using SDL2 for rendering/input and SNES hardware emulation (PPU/DSP).

**Key Characteristics:**
- Original behavior preserved for replay compatibility
- Can verify against original ROM frame-by-frame
- Enhanced features optional and off by default
- Variable names from community disassembly efforts

**Documentation:**
- **[docs/installation.md](docs/installation.md)** - Build instructions, dependencies
- **[docs/architecture.md](docs/architecture.md)** - Code architecture, modules, patterns
- **[docs/development.md](docs/development.md)** - Development workflow and patterns
- **[docs/debugging.md](docs/debugging.md)** - Debug console and troubleshooting
- **[docs/platforms/](docs/platforms/)** - Platform-specific guides (Windows, Linux, macOS, Android, Switch)
- **[docs/technical/](docs/technical/)** - Deep dives (memory layout, graphics, audio, renderers)
- **[CHANGELOG.md](CHANGELOG.md)** - Recent changes and version history

## Quick Start for Development

### Building
```bash
# Extract assets first (requires USA ROM)
python3 assets/restool.py --extract-from-rom

# Build with CMake
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j$(nproc)
```

See [docs/installation.md](docs/installation.md) for full details and platform-specific guides in [docs/platforms/](docs/platforms/).

### Asset File
`zelda3_assets.dat` must exist before building. Extract from ROM (see above). File must be co-located with executable when running.

## Critical Architecture Concepts

### Memory Layout - RAM Macros
All game state accessed via macros in `src/variables.h`:
```c
#define link_x_coord (*(uint16*)(g_ram+0x22))
#define link_y_coord (*(uint16*)(g_ram+0x20))
#define frame_counter (*(uint8*)(g_ram+0x1A))

// Usage
link_x_coord = 100;        // Sets *(uint16*)(g_ram+0x22) = 100
uint16 x = link_x_coord;   // Reads from RAM
```

**Memory regions:**
- `g_ram` - Main game RAM
- `g_zenv.vram` - Video RAM
- `g_zenv.sram` - Save data

### Feature Flags
Enhanced features use special RAM locations (`src/features.h`):
```c
if (enhanced_features0 & kFeatures0_SwitchLR) {
  // Feature enabled
}
```

Flags stored at unused RAM offsets (0x648+). Must be toggleable via `zelda3.ini`.

## Module Organization

**Game Logic:**
- `src/player.c` - Link's movement/actions (211KB)
- `src/sprite_main.c` - All enemy AI (808KB - largest file)
- `src/ancilla.c` - Projectiles, effects (238KB)
- `src/dungeon.c` - Dungeon rooms, puzzles (296KB)
- `src/overworld.c` - Overworld map (135KB)

**Graphics:**
- `snes/ppu.c` - SNES PPU emulation
- `src/opengl.c` - OpenGL/OpenGL ES renderer
- `src/vulkan.c` - Vulkan 1.0 renderer (cross-platform)
- `src/glsl_shader.c` - OpenGL shader support

**Platform:**
- `src/platform.h/c` - File I/O abstraction
- `src/config.c` - INI parsing, gamepad binding
- `src/logging.h/c` - Logging system with platform detection
- `src/platform_detect.h` - Platform/compiler/architecture detection

**Android Integration:**
- `android/` - Complete Gradle-based Android app
- `android/app/jni/` - JNI glue code and SDL integration
- Main entry point: `SDL_main` (macro defined by SDL)
- Asset loading: SDL external storage path
- Renderer: OpenGL ES or Vulkan 1.0

See [docs/architecture.md](docs/architecture.md) for complete breakdown and [docs/technical/](docs/technical/) for deep dives.

## Development Patterns

### Adding Features
1. Define flag in `src/features.h` with power-of-2 value
2. Store in unused RAM (next available offset from 0x648+)
3. Add INI option in `zelda3.ini` template
4. Implement feature gated by flag check
5. Ensure disabled by default (compatibility)

### Accessing State
Use macro accessors from `variables.h` - don't access `g_ram` directly:
```c
// Correct
link_x_coord = new_x;

// Wrong
*(uint16*)(g_ram+0x22) = new_x;
```

### Coordinate System
- Positions: 16-bit fixed point
- Subpixel precision: Separate variables (`link_subpixel_x`)
- Widescreen offset: `kPpuExtraLeftRight` from `types.h`

### Android Development
When adding Android-specific code:
1. **Early initialization order matters:**
   - Create audio mutex BEFORE SDL_Init (prevents race conditions)
   - Call SDL_Init BEFORE LoadAssets (ensures subsystems are ready)
   - Use `SDL_AndroidGetExternalStoragePath()` and chdir before loading assets

2. **Logging during initialization:**
   - Use `__android_log_print()` for logs before `InitializeLogging()` is called
   - Regular logging automatically disables ANSI colors on Android
   - Tag convention: "Zelda3Main", "Zelda3Platform", etc.

3. **Renderer setup:**
   - Vulkan 1.0 and OpenGL ES both supported on Android
   - Show Toast notifications to user for important fallbacks
   - Update zelda3.ini to persist renderer changes
   - Android requires SDL_WINDOW_OPENGL flag for SDL_Renderer and OpenGL ES (but NOT for Vulkan)

4. **Thread safety:**
   - Audio mutex must exist before SDL audio threads start
   - Use ZeldaApuLock/ZeldaApuUnlock for audio state access
   - Check mutex exists before locking (`if (g_audio_mutex) SDL_LockMutex(...)`)

5. **File I/O:**
   - All file access goes through Platform_* API
   - Android logging in Platform_ReadWholeFile for debugging asset issues
   - Working directory is SDL external storage path

### Platform-Specific Code
Use semantic macros from `src/platform_detect.h`:
```c
#include "platform_detect.h"

#ifdef PLATFORM_ANDROID
  // Android code
  #include <android/log.h>
  __android_log_print(ANDROID_LOG_DEBUG, "Zelda3", "Message");
#elif defined(PLATFORM_WINDOWS)
  // Windows code
#elif defined(PLATFORM_MACOS)
  // macOS code
#elif defined(PLATFORM_SWITCH)
  // Switch code
#elif defined(PLATFORM_LINUX)
  // Linux code
#endif

// Also available: COMPILER_MSVC, COMPILER_GCC, COMPILER_CLANG
//                 ARCH_X64, ARCH_ARM64, etc.
```

Platform-specific files go in `src/platform/<name>/`

**Android-specific patterns:**
- Use `SDL_AndroidGetExternalStoragePath()` for asset directory
- Disable ANSI colors in logging (logcat doesn't support them)
- Early SDL_Init before asset loading to prevent race conditions
- Create audio mutex before SDL_Init to avoid threading issues
- Main function must be named `SDL_main` (SDL.h handles macro)
- Use `__android_log_print()` for critical early initialization logs

## File Locations

**When modifying:**
- Build system: `CMakeLists.txt`
- Dependencies: Check CMake auto-detection (SDL2, OpenGL, Vulkan)
- Config options: `src/config.c`, `src/config.h`
- Feature flags: `src/features.h`
- Type definitions: `src/types.h`
- Platform detection: `src/platform_detect.h`
- Math utilities: `src/math_util.h`
- Dynamic arrays: `src/dynamic_array.h`
- Constants: Typically in module header files

**When adding files:**
- Game logic: `src/`
- SNES emulation: `snes/`
- Platform code: `src/platform/<name>/`
- Android code: `android/app/jni/` or `android/app/src/`
- Documentation: `docs/` (see [docs/contributing.md](docs/contributing.md) for structure)
- Logging: Use `src/logging.h` for errors/warnings
- File I/O: Use `Platform_ReadWholeFile()` from `src/platform.h`
- Path validation: Use `Platform_FindFileWithCaseInsensitivity()` from `src/platform.h` for case-sensitive filesystem support
- Memory: Use `DYNARR_*` macros from `src/dynamic_array.h` for growable arrays
- Desktop: CMake auto-detects `.c` files via `file(GLOB ...)`
- Android: Update `android/app/build.gradle` for new JNI sources

**Excluded from git (`.gitignore`):**
- Build artifacts: `build/`, `android/build/`, `android/.gradle/`
- IDE files: `android/.idea/`, `.vscode/`, `.vs/`
- Local configs: `android/local.properties`, `CLAUDE.local.md`
- Planning docs: `ANDROID_MIGRATION_PLAN.md` (internal development notes)

**Logging:**
- Use `LogError()`, `LogWarn()`, `LogInfo()`, `LogDebug()` from `logging.h`
- Simple stderr-based with ANSI colors (TTY auto-detected)
- Control via `ZELDA3_LOG_LEVEL` environment variable
- No modules, no file I/O - follows YAGNI/KISS principles
- Portable: stderr works on all platforms (Android, Switch, desktop)
- Android: ANSI colors automatically disabled (logcat doesn't support)
- Early initialization logging: Use `__android_log_print()` on Android before logging system is ready

**Renderer abstraction:**
- `src/util.h` - RendererFuncs interface with Init/Destroy/BeginDraw/EndDraw/OnResize callbacks
- Implementations: SDL software, OpenGL, OpenGL ES, Vulkan 1.0
- OnResize callback: Handle window resize events (SDL_WINDOWEVENT_SIZE_CHANGED)
- Vulkan renderer requires VK_KHR_portability_enumeration and VK_KHR_portability_subset on MoltenVK (macOS)
- Android uses OpenGL ES by default (Vulkan optional)

## Common Tasks

### Building

**Desktop (CMake):**
```bash
# Standard build
mkdir build && cd build && cmake .. && cmake --build . -j$(nproc)

# Debug build
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Specific compiler
cmake .. -DCMAKE_C_COMPILER=clang
```

**Android (Gradle):**
```bash
# From android/ directory
./gradlew assembleDebug        # Build debug APK
./gradlew assembleRelease      # Build release APK
./gradlew installDebug         # Install debug APK to device
adb logcat | grep Zelda3       # View Android logs
```

**Prerequisites:**
- Assets must be in SDL external storage path on Android
- Use `SDL_AndroidGetExternalStoragePath()` to locate assets
- Desktop: `zelda3_assets.dat` in same directory as executable

### Debugging
- Use `kDebugFlag` for debug-only code (types.h)
- Frame counter: `frame_counter` macro (variables.h)
- Snapshot system: F1-F10 save/load, Ctrl+F1-F10 replay
- Debug console: See [docs/debugging.md](docs/debugging.md) for details

### Testing Changes
- Must have `zelda3_assets.dat` extracted
- Test with original behavior first
- Check replay compatibility if modifying game logic
- Use verification mode to compare with ROM

## Important Notes

**Code Style:**
- Reverse-engineered, variable names from disassembly
- Follows original SNES code structure
- Function names: `Module_FunctionName()` or camelCase
- Globals: `g_prefix` (e.g., `g_ram`, `g_r12`)
- Constants: `kConstantName`
- RAM macros: Prefix with `g_` if they might conflict with platform symbols (e.g., Windows register names R10-R20)

**Compatibility:**
- Original behavior must be preserved
- Enhanced features must be optional
- Replay files must remain compatible
- Default config matches original game

**Performance:**
- No dynamic allocation in game loop (60 FPS)
- Hot paths: `ZeldaRunFrame()`, PPU rendering
- Use lookup tables over calculations
- Inline performance-critical functions

**Build System:**
- **Desktop:** CMake (Makefile removed)
- **Android:** Gradle with JNI integration (`android/` directory)
- Auto-detects dependencies (SDL2, Opus, OpenGL, Vulkan)
- Uses system libraries (no vendored dependencies)
- Out-of-source builds (build/ directory for desktop)
- See [docs/installation.md](docs/installation.md) and [docs/platforms/](docs/platforms/) for details
- Android build: `./gradlew assembleDebug` from `android/` directory

**Case-Sensitive Filesystems (Linux):**
- MSU audio and shader paths must match exact capitalization
- Config validation at startup warns about case mismatches
- Use `Platform_FindFileWithCaseInsensitivity()` to check paths
- Error messages guide users to correct capitalization

## Recent Changes

**Vulkan renderer integration (November 2025):**
- **Cross-platform Vulkan 1.0:** Ported from zelda3-android, works on desktop and mobile
- **MoltenVK support:** Auto-detects and enables VK_KHR_portability_enumeration/subset on macOS
- **Shader loading:** Cross-platform (filesystem on desktop, APK assets on Android via JNI)
- **Surface creation:** SDL_Vulkan_CreateSurface for cross-platform compatibility
- **Shaders:** Pre-compiled SPIR-V shaders (vert.spv, frag.spv) with GLSL source
- **Configuration:** Added Vulkan to zelda3.ini OutputMethod options

**Android platform integration (November 2025):**
- **Android build system:** Complete Gradle-based Android app with JNI integration
- **Platform detection:** Enhanced logging with Android-specific path handling
- **SDL initialization:** Early SDL_Init before LoadAssets to prevent race conditions
- **Audio mutex timing:** Create audio mutex before SDL_Init to avoid threading issues
- **Working directory:** Automatic chdir to SDL external storage path on Android
- **Renderer architecture:** Added OnResize callback to RendererFuncs interface
- **OpenGL ES:** Android defaults to OpenGL ES renderer with Vulkan as optional
- **Window events:** Handle SDL_WINDOWEVENT_SIZE_CHANGED for dynamic resizing
- **Debug logging:** Android logcat integration via `__android_log_print()`
- **File I/O:** Enhanced Platform_ReadWholeFile with Android-specific error logging
- **Config updates:** Extended aspect ratio now defaults to `extend_y, 16:10` for mobile
- **isatty() handling:** Disable ANSI colors on Android (logcat doesn't support)

**Build system and Windows compatibility (November 2025):**
- **Opus migration:** Switched from vendored Opus 1.3.1 to system library (-924KB)
- **CMake modules:** Added FindOpus.cmake for pkg-config-based detection
- **Windows fix:** Renamed R10/R12/R14/R16/R18/R20 macros to g_r10/g_r12/etc to avoid Windows x64 register name collisions
- **CI improvements:** Updated GitHub Actions with Opus dependencies, vcpkg@v11.5
- **All platforms building:** 10/10 CI jobs passing (Ubuntu, Arch, Windows, macOS x86_64/ARM64)

**Code quality improvements (November 2025):**
- **Refactoring:** Config section handlers, magic number extraction, shader cleanup
- **Platform consistency:** MSU audio now uses Platform_* API
- **Utility headers:** platform_detect.h, math_util.h, dynamic_array.h, logging.h
- **Path validation:** Case-insensitive path lookup for MSU/shader paths on Linux
- **Logging standardization:** All user-facing errors/warnings now use LogError/LogWarn API
- **Debug output gating:** Debug-only code properly gated with NDEBUG
- **Debug state tracking:** Event-driven game state console output for bug fixing
- **Compatibility:** Original SNES bugs preserved, fixes gated behind feature flags

**Major update: Android port integration (2024)**
- CMake build system
- Platform abstraction layer
- Bug fixes (OpenGL, memory safety)
- New APIs (frame buffer, gamepad binding)
- Documentation reorganization

See [CHANGELOG.md](CHANGELOG.md) for complete details.

## Getting Help

- **Getting started:** See [docs/getting-started.md](docs/getting-started.md)
- **Build issues:** See [docs/installation.md](docs/installation.md) and [docs/troubleshooting.md](docs/troubleshooting.md)
- **Architecture questions:** See [docs/architecture.md](docs/architecture.md) and [docs/technical/](docs/technical/)
- **Contributing:** See [docs/contributing.md](docs/contributing.md) and [docs/development.md](docs/development.md)
- **Platform-specific:** See [docs/platforms/](docs/platforms/) (Windows, Linux, macOS, Android, Switch)
- **Recent changes:** See [CHANGELOG.md](CHANGELOG.md)
- **Original project:** https://github.com/snesrev/zelda3 (base C reimplementation)
- **Android integration:** https://github.com/Waterdish/zelda3-android
- **Touchpad controls:** https://git.eden-emu.dev/eden-emu/eden (Eden emulator project)
- **Discord:** https://discord.gg/AJJbJAzNNJ
