# Zelda3 Architecture

[Home](index.md) > Architecture

## Overview

Zelda3 is a ~70-80kLOC C reimplementation of The Legend of Zelda: A Link to the Past for SNES. This is a complete reverse-engineering of the original game, preserving original behavior for replay compatibility while enabling modern enhancements and cross-platform support.

**Key Design Principles:**
- **Replay Compatible:** Original behavior preserved for frame-by-frame verification
- **Enhanced Features:** Optional improvements disabled by default for compatibility
- **Cross-Platform:** Runs on Linux, macOS, Windows, Switch, and Android via SDL2
- **Verified Accuracy:** Can run side-by-side with original ROM for verification

**Technology Stack:**
- **Language:** C (C99/C11)
- **Graphics:** SDL2 with OpenGL 3.3+, OpenGL ES 3.0+, or Vulkan 1.0
- **Audio:** SDL2 audio with SPC-700 emulation, MSU audio support (Opus)
- **Build Systems:** CMake (desktop), Gradle (Android)

## Core Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                      main.c / SDL_main                      │
│  Platform Init → SDL Init → Asset Loading → Game Loop       │
└─────────────────────────────────────────────────────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        │                     │                     │
        ▼                     ▼                     ▼
┌──────────────┐    ┌──────────────────┐    ┌─────────────┐
│ Input System │    │ Game Logic Core  │    │   Graphics  │
│  SDL Events  │───▶│  ZeldaRunFrame() │───▶│ PPU → Render│
│  Gamepad     │    │  zelda_rtl.c     │    │ OpenGL/Vulkan│
└──────────────┘    └──────────────────┘    └─────────────┘
                             │
                             ▼
                    ┌──────────────────┐
                    │   Memory System  │
                    │   g_ram buffer   │
                    │  RAM macros      │
                    └──────────────────┘
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
        ▼                    ▼                    ▼
┌──────────────┐   ┌──────────────┐    ┌──────────────┐
│ Player Logic │   │ Sprite/Enemy │    │ World Systems│
│  player.c    │   │ sprite_main.c│    │ dungeon.c    │
│ 211KB        │   │ 808KB        │    │ overworld.c  │
└──────────────┘   └──────────────┘    └──────────────┘
```

## Game Loop

The main game loop runs at 60 FPS and follows this flow:

```c
// Simplified game loop structure
while (running) {
  // 1. Handle Input
  SDL_PollEvent(&event);
  ProcessInput(event);

  // 2. Run Game Logic (1/60 second)
  ZeldaRunFrame(input_state);

  // 3. Render Frame
  renderer->BeginDraw(width, height, &pixels, &pitch);
  PpuRender(&g_zenv.ppu);  // SNES PPU emulation
  renderer->EndDraw();

  // 4. Audio Processing
  SpcPlayer_GenerateSamples(audio_buffer);

  // 5. Frame Timing
  SDL_Delay(frame_time);
}
```

**Key Components:**
- **Input Handling:** SDL events → input state bitmask
- **Game Logic:** `ZeldaRunFrame()` executes one frame of game state updates
- **PPU Rendering:** SNES Picture Processing Unit emulation generates frame buffer
- **Renderer:** Platform-specific renderer (SDL/OpenGL/Vulkan) displays frame
- **Audio:** SPC-700 emulation or MSU audio tracks mixed by SDL

## Memory Architecture

All game state lives in a unified `g_ram` buffer that mirrors SNES memory layout. Access is provided through macro wrappers defined in `src/variables.h`.

**Memory Regions:**
- **`g_ram`:** Main game RAM (SNES WRAM equivalent, ~64KB)
- **`g_zenv.vram`:** Video RAM for tile/sprite graphics (64KB)
- **`g_zenv.sram`:** Save RAM for game progress (8KB)

**Access Pattern Example:**
```c
// Read Link's position
uint16 x = link_x_coord;  // Expands to: *(uint16*)(g_ram+0x22)

// Write Link's position
link_x_coord = 100;

// Subpixel precision
uint8 subpixel = link_subpixel_x;
```

This design preserves the original SNES memory layout, enabling:
- Snapshot/replay compatibility
- ROM verification mode
- Easy porting from disassembly

**See:** [Memory Layout Details](technical/memory-layout.md)

## Module Breakdown

### Game Logic Modules

| Module | Size | Description |
|--------|------|-------------|
| **player.c** | 211KB | Link's movement, state machine, actions, collision |
| **sprite_main.c** | 808KB | All enemy/NPC AI implementations (largest file) |
| **ancilla.c** | 238KB | Projectiles and effects (arrows, boomerang, hookshot) |
| **dungeon.c** | 296KB | Dungeon rooms, puzzles, blocks, doors |
| **overworld.c** | 135KB | Overworld map, transitions, weather effects |
| **hud.c** | 55KB | HUD rendering, inventory display |
| **messaging.c** | 99KB | Text boxes, dialogue system |

**See:** [Feature Flags](technical/feature-flags.md) for adding new features

### Graphics Pipeline

```
Frame Start
  │
  ├─ Load Graphics (load_gfx.c)
  │   └─ Decompress tiles, manage VRAM
  │
  ├─ Update OAM (player_oam.c, sprite.c)
  │   └─ Sprite positions, animations
  │
  ├─ PPU Rendering (snes/ppu.c)
  │   ├─ Background layers (tilemaps)
  │   ├─ Sprite compositing (OAM)
  │   ├─ Mode 7 (rotation/scaling)
  │   └─ Color math (transparency, brightness)
  │
  └─ Renderer Output (opengl.c / vulkan.c)
      ├─ Optional: GLSL shaders
      └─ Frame buffer → Screen
```

**Graphics Modules:**
- **snes/ppu.c (60KB):** Complete SNES PPU emulation
- **src/opengl.c (9KB):** OpenGL 3.3+ / OpenGL ES 3.0+ backend
- **src/vulkan.c:** Vulkan 1.0 backend (cross-platform, MoltenVK on macOS)
- **src/glsl_shader.c (25KB):** Pixel shader support for CRT effects

**See:** [Graphics Pipeline Details](technical/graphics-pipeline.md)

### Audio System

```
Audio Frame (60Hz)
  │
  ├─ SPC Player (spc_player.c)
  │   └─ Manages SPC-700 playback
  │
  ├─ APU/DSP Emulation (snes/)
  │   ├─ SPC-700 CPU (spc.c)
  │   ├─ DSP (dsp.c) - 8 channel mixing
  │   └─ Sample generation
  │
  ├─ MSU Audio (audio.c)
  │   └─ Modern Opus tracks (optional replacement)
  │
  └─ SDL Audio Callback
      └─ Mix samples → Output device
```

**See:** [Audio System Details](technical/audio-system.md)

### Platform Abstraction

Zelda3 provides platform abstraction layers for cross-platform compatibility:

**File I/O Layer** (`src/platform.h/c`):
```c
PlatformFile *Platform_OpenFile(const char *filename, const char *mode);
uint8_t *Platform_ReadWholeFile(const char *filename, size_t *length_out);
char *Platform_FindFileWithCaseInsensitivity(const char *path);
```

**Platform Detection** (`src/platform_detect.h`):
```c
#ifdef PLATFORM_ANDROID
  // Android-specific code
#elif defined(PLATFORM_WINDOWS)
  // Windows code
#elif defined(PLATFORM_MACOS)
  // macOS code
#endif
```

**Renderer Abstraction** (`src/util.h`):
```c
struct RendererFuncs {
  bool (*Initialize)(SDL_Window *window);
  void (*Destroy)();
  void (*BeginDraw)(int width, int height, uint8 **pixels, int *pitch);
  void (*EndDraw)();
  void (*OnResize)(int width, int height);
};
```

**See:** [Renderer Details](technical/renderers.md)

## Code Style Conventions

The codebase follows conventions from the original SNES disassembly:

**Naming Conventions:**
```c
// Functions: Module_FunctionName() or ModuleFunctionName()
void Player_HandleMovement();
void LinkOam_Main();

// Global variables: g_prefix
uint8 *g_ram;
ZeldaEnv g_zenv;

// Constants: kConstantName
enum { kMaxSprites = 16 };

// RAM macros: descriptive_name (no prefix, unless conflicts with Windows)
#define link_x_coord (*(uint16*)(g_ram+0x22))
#define g_r12 (*(uint16*)(g_ram+0x12))  // Prefixed to avoid Windows register names
```

**File Organization:**
- Game logic: `src/`
- SNES emulation: `snes/`
- Platform code: `src/platform/<name>/`
- Android: `android/` (separate Gradle build)

**Memory Access:**
- Always use RAM macros from `variables.h`
- Never access `g_ram` directly: ~~`*(uint16*)(g_ram+0x22)`~~
- Use: `link_x_coord = value;`

## Extension Points

### Adding New Features

1. **Define flag** in `src/features.h`:
   ```c
   kFeatures0_MyNewFeature = 1024,
   ```

2. **Store in unused RAM** (next available offset from 0x648+)

3. **Add INI option** in `zelda3.ini`:
   ```ini
   [Features]
   EnableMyFeature = false
   ```

4. **Implement with guard**:
   ```c
   if (enhanced_features0 & kFeatures0_MyNewFeature) {
     // New behavior
   }
   ```

**See:** [Feature Flags System](technical/feature-flags.md)

### Adding New Platforms

1. Add platform detection to `src/platform_detect.h`
2. Create `src/platform/<name>/` for platform-specific code
3. Implement platform API overrides (File I/O, logging)
4. Desktop: Update `CMakeLists.txt`
5. Mobile: Create separate build system (Gradle/Xcode)

### Adding New Renderers

1. Implement `RendererFuncs` interface
2. Add to output method enum in `config.h`
3. Register renderer in `main.c`

**See:** [Renderer Implementation Guide](technical/renderers.md)

## Performance Considerations

**Hot Paths:**
- `ZeldaRunFrame()`: Called 60 times per second
- PPU rendering: Per-scanline, per-pixel operations
- Sprite AI: Processes hundreds of entities per frame

**Optimizations:**
- **Inlined macros:** RAM access with zero overhead
- **Lookup tables:** Trigonometry (sine/cosine tables)
- **No allocations:** Game loop is allocation-free
- **Fast renderer:** Optional `NewRenderer=1` for modern GPUs

## State Machine

The game uses a module/submodule system for state management:

```c
// Global state variables
main_module_index     // 0-25 (title, overworld, dungeon, ending, etc.)
submodule_index       // Substates within each module
```

Each module has update functions called per-frame based on the current state.

## Debugging Tools

**Built-in:**
- **Snapshots:** F1-F10 save/load game state, Ctrl+F1-F10 replay
- **Turbo:** Tab for fast-forward
- **Cheats:** W (health), Shift+W (items)
- **Performance:** F key shows FPS

**Development:**
- `kDebugFlag`: Compile-time debug mode
- `frame_counter`: Global frame counter
- Verification mode: Frame-by-frame comparison with ROM

## See Also

- **[Memory Layout](technical/memory-layout.md)** - Detailed memory system documentation
- **[Graphics Pipeline](technical/graphics-pipeline.md)** - Graphics rendering details
- **[Audio System](technical/audio-system.md)** - Audio pipeline documentation
- **[Feature Flags](technical/feature-flags.md)** - How to add new features
- **[Renderers](technical/renderers.md)** - Renderer implementations

**User Documentation:**
- [Getting Started](getting-started.md)
- [Building](../BUILDING.md)
- [Platform Guides](platforms/)

**References:**
- [Original Disassembly](https://github.com/spannerisms/zelda3)
- [SDL2 Documentation](https://www.libsdl.org/)
- [SNES Development Wiki](https://wiki.superfamicom.org/)
