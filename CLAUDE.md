# CLAUDE.md

Project-specific instructions for Claude Code. For personal notes, use `CLAUDE.local.md` (git-ignored).

## Project Overview

Zelda3: Reverse-engineered C reimplementation of The Legend of Zelda: A Link to the Past (~70-80kLOC).

- Original behavior preserved for replay compatibility
- SDL2 for rendering/input, SNES PPU/DSP emulation
- Enhanced features optional, disabled by default
- Variable names from community disassembly

**Key docs:** [docs/architecture.md](docs/architecture.md), [docs/installation.md](docs/installation.md), [CHANGELOG.md](CHANGELOG.md)

## Quick Start

```bash
# Build
mkdir build && cd build && cmake .. && cmake --build . -j$(nproc)

# Extract assets (required before running)
./zelda3-restool --extract-from-rom ../zelda3.sfc --compile

# Android
cd android && ./gradlew assembleDebug
```

## Critical Concepts

### RAM Macros (variables.h)
All game state via macros - never access g_ram directly:
```c
#define link_x_coord (*(uint16*)(g_ram+0x22))
link_x_coord = 100;  // Correct
```

Memory regions: `g_ram` (game), `g_zenv.vram` (video), `g_zenv.sram` (save)

### Feature Flags (features.h)
```c
if (enhanced_features0 & kFeatures0_SwitchLR) { /* enabled */ }
```
Flags at unused RAM 0x648+. Must be toggleable via zelda3.ini.

## Module Map

**Game Logic:** `player.c`, `sprite_main.c` (808KB), `ancilla.c`, `dungeon.c`, `overworld.c`

**Graphics:** `snes/ppu.c`, `opengl.c`, `vulkan.c`, `glsl_shader.c`

**Platform:** `platform.h/c` (file I/O, save files), `config.c`, `logging.h/c`, `platform_detect.h`

**Android:** `android/app/jni/`, `src/platform/android/` (jni_helpers, android_jni), `android/.../util/` (Kotlin utils)

**Launcher:** `src/launcher/` (GTK3 UI, config reader/writer)

**Restool:** `src/restool/` (C asset extraction, library API)

**Opus Encoder:** `src/opus_encoder/` (PCM to OPUZ converter, library + CLI)

See [docs/architecture.md](docs/architecture.md) for details.

## Development Patterns

### Adding Features
1. Define flag in `features.h` (power-of-2)
2. Store at unused RAM offset (0x648+)
3. Add INI option in zelda3.ini
4. Gate implementation with flag check
5. Disabled by default

### Platform Code
Use `platform_detect.h` macros:
```c
#ifdef PLATFORM_ANDROID
#elif defined(PLATFORM_WINDOWS)
#elif defined(PLATFORM_MACOS)
#elif defined(PLATFORM_LINUX)
#endif
```
Platform files go in `src/platform/<name>/`

### Android Specifics
- Init order: audio mutex → SDL_Init → LoadAssets
- Use `__android_log_print()` before logging system ready
- File I/O via Platform_* API
- Save files via SAF (Storage Access Framework)
- Entry point: `SDL_main`

## File Locations

**Modify:**
- Build: `CMakeLists.txt`
- Config: `config.c/h`, `features.h`
- Types: `types.h`, `variables.h`
- Platform: `platform_detect.h`, `platform.h/c`

**Add files:**
- Game logic: `src/`
- Platform: `src/platform/<name>/`
- Android: `android/app/jni/` or `android/app/src/`
- SNES emu: `snes/`
- Docs: `docs/`

**APIs:**
- Logging: `LogError/Warn/Info/Debug()` from `logging.h`
- File I/O: `Platform_ReadWholeFile()`, `Platform_OpenSaveFile()`
- Arrays: `DYNARR_*` from `dynamic_array.h`

## Build Commands

**Desktop:**
```bash
cmake --build build -j$(nproc)
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake .. -DENABLE_CLANG_TIDY=ON  # Enable clang-tidy during build
```

**Android:**
```bash
./gradlew assembleDebug
./gradlew installDebug
adb logcat | grep Zelda3
```

## Code Style

- Function names: `Module_FunctionName()` or camelCase
- Globals: `g_prefix` (g_ram, g_r12)
- Constants: `kConstantName`
- Follows original SNES code structure
- Static analysis: `clang-tidy` configured in `.clang-tidy`

## Constraints

- Original behavior preserved
- Enhanced features optional
- Replay compatibility required
- No dynamic allocation in game loop (60 FPS)
- Hot paths: `ZeldaRunFrame()`, PPU rendering

## Help

- Build: [docs/installation.md](docs/installation.md), [docs/platforms/](docs/platforms/)
- Architecture: [docs/architecture.md](docs/architecture.md), [docs/technical/](docs/technical/)
- Changes: [CHANGELOG.md](CHANGELOG.md)
- Debug: [docs/debugging.md](docs/debugging.md)
