# CLAUDE.md

This file provides AI-specific guidance for Claude Code when working with this repository.

## Project Overview

Zelda3 is a reverse-engineered C reimplementation of The Legend of Zelda: A Link to the Past. ~70-80kLOC of C code reimplementing all game logic, using SDL2 for rendering/input and SNES hardware emulation (PPU/DSP).

**Key Characteristics:**
- Original behavior preserved for replay compatibility
- Can verify against original ROM frame-by-frame
- Enhanced features optional and off by default
- Variable names from community disassembly efforts

**Documentation:**
- **[BUILDING.md](BUILDING.md)** - Build instructions, dependencies, troubleshooting
- **[ARCHITECTURE.md](ARCHITECTURE.md)** - Detailed code architecture, modules, patterns
- **[CHANGELOG.md](CHANGELOG.md)** - Recent changes, Android port integration
- **[REFACTORING_TODO.md](REFACTORING_TODO.md)** - Ongoing code quality improvements, security fixes

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

See [BUILDING.md](BUILDING.md) for full details.

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
- `src/opengl.c` - OpenGL renderer
- `src/glsl_shader.c` - Shader support

**Platform:**
- `src/platform.h/c` - File I/O abstraction
- `src/config.c` - INI parsing, gamepad binding

See [ARCHITECTURE.md](ARCHITECTURE.md) for complete breakdown.

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

### Platform-Specific Code
Use conditional compilation:
```c
#ifdef _WIN32
  // Windows code
#elif defined(__APPLE__)
  // macOS code
#elif defined(__SWITCH__)
  // Switch code
#else
  // Linux/generic
#endif
```

Platform-specific files go in `src/platform/<name>/`

## File Locations

**When modifying:**
- Build system: `CMakeLists.txt`
- Dependencies: Check CMake auto-detection (SDL2, OpenGL, Vulkan)
- Config options: `src/config.c`, `src/config.h`
- Feature flags: `src/features.h`
- Type definitions: `src/types.h`
- Constants: Typically in module header files

**When adding files:**
- Game logic: `src/`
- SNES emulation: `snes/`
- Platform code: `src/platform/<name>/`
- Logging: Use `src/logging.h` for errors/warnings
- CMake auto-detects `.c` files via `file(GLOB ...)`

**Logging:**
- Use `LogError()`, `LogWarn()`, `LogInfo()`, `LogDebug()` from `logging.h`
- Simple stderr-based with ANSI colors (TTY auto-detected)
- Control via `ZELDA3_LOG_LEVEL` environment variable
- No modules, no file I/O - follows YAGNI/KISS principles
- Portable: stderr works on all platforms (Android, Switch, desktop)

## Common Tasks

### Building
```bash
# Standard build
mkdir build && cd build && cmake .. && cmake --build . -j$(nproc)

# Debug build
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Specific compiler
cmake .. -DCMAKE_C_COMPILER=clang
```

### Debugging
- Use `kDebugFlag` for debug-only code (types.h)
- Frame counter: `frame_counter` macro (variables.h)
- Snapshot system: F1-F10 save/load, Ctrl+F1-F10 replay

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
- Globals: `g_prefix`
- Constants: `kConstantName`

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
- Uses CMake (Makefile removed)
- Auto-detects dependencies
- Out-of-source builds (build/ directory)
- See [BUILDING.md](BUILDING.md) for details

## Recent Changes

**Code quality improvements (2025):**
- Security fixes: Buffer overflow prevention, integer overflow checks
- Memory safety: Bounds validation, resource leak fixes, proper cleanup
- Logging system: Simple stderr-based with color support
- Error handling: Graceful degradation instead of crashes
- See [REFACTORING_TODO.md](REFACTORING_TODO.md) for details

**Major update: Android port integration (2024)**
- CMake build system
- Platform abstraction layer
- Bug fixes (OpenGL, memory safety)
- New APIs (frame buffer, gamepad binding)
- Documentation reorganization

See [CHANGELOG.md](CHANGELOG.md) for complete details.

## Getting Help

- **Build issues:** See [BUILDING.md](BUILDING.md) troubleshooting
- **Architecture questions:** See [ARCHITECTURE.md](ARCHITECTURE.md)
- **Recent changes:** See [CHANGELOG.md](CHANGELOG.md)
- **Original project:** https://github.com/snesrev/zelda3
- **Discord:** https://discord.gg/AJJbJAzNNJ
