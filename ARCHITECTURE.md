# Zelda3 Architecture

Detailed technical architecture of the Zelda3 reimplementation.

## Overview

This is a ~70-80kLOC C reimplementation of The Legend of Zelda: A Link to the Past for SNES. The codebase is reverse-engineered from the original game, with function and variable names from community disassembly efforts.

**Key Design Principles:**
- Original behavior preserved for replay compatibility
- Frame-by-frame verification against original ROM possible
- Enhanced features are optional and disabled by default
- Cross-platform via SDL2

## Core Architecture

### Game Loop

```
main.c
  ├─ SDL initialization
  ├─ Asset loading (zelda3_assets.dat)
  ├─ Game loop (60 FPS)
  │   ├─ Input handling
  │   ├─ ZeldaRunFrame() → zelda_rtl.c
  │   ├─ PPU rendering → snes/ppu.c
  │   └─ Audio processing → snes/apu.c
  └─ Cleanup
```

**Key Files:**
- `src/main.c`: Entry point, SDL lifecycle, input
- `src/zelda_rtl.c`: Runtime environment, frame execution
- `src/zelda_cpu_infra.c`: Optional ROM verification mode

## Memory Architecture

### RAM Layout

All game state lives in a single `g_ram` buffer accessed via macro wrappers:

```c
// variables.h defines RAM layout
#define link_x_coord (*(uint16*)(g_ram+0x22))
#define link_y_coord (*(uint16*)(g_ram+0x20))
#define frame_counter (*(uint8*)(g_ram+0x1A))
```

**Memory Regions:**
- `g_ram`: Main game RAM (SNES WRAM equivalent)
- `g_vram`: Video RAM (via `g_zenv.vram`)
- `g_sram`: Save RAM (via `g_zenv.sram`)

**Access Pattern:**
```c
// Read
uint16 x = link_x_coord;

// Write
link_x_coord = 100;

// Expands to: *(uint16*)(g_ram+0x22) = 100
```

### Feature Flags

Enhanced features use special unused RAM locations:

```c
// features.h
enum {
  kRam_Features0 = 0x64c,  // Feature bitmask
};

#define enhanced_features0 (*(uint32*)(g_ram+0x64c))

// Usage
if (enhanced_features0 & kFeatures0_SwitchLR) {
  // L/R item switching enabled
}
```

## Module Breakdown

### Game Logic

**Player System:**
- `src/player.c` (211KB): Link's movement, state machine, actions
- `src/player_oam.c` (61KB): Link's sprite rendering (OAM = Object Attribute Memory)

**Enemy/NPC System:**
- `src/sprite.c` (148KB): Generic sprite logic
- `src/sprite_main.c` (808KB): All enemy/NPC AI implementations
- `src/ancilla.c` (238KB): Projectiles, effects (arrows, boomerang, hookshot, etc.)

**World Systems:**
- `src/dungeon.c` (296KB): Dungeon rooms, puzzles, blocks
- `src/overworld.c` (135KB): Overworld map, transitions, weather
- `src/overlord.c` (20KB): Level-wide entities (falling tiles, etc.)

**UI/UX:**
- `src/hud.c` (55KB): HUD rendering, inventory
- `src/messaging.c` (99KB): Text boxes, dialogue system
- `src/select_file.c` (34KB): File select screen

**Game Flow:**
- `src/attract.c` (35KB): Title screen, demos
- `src/ending.c` (81KB): Credits, ending sequences

### Graphics Pipeline

```
Frame Start
  ↓
Load Graphics (load_gfx.c)
  ↓
Update OAM (player_oam.c, sprite.c)
  ↓
PPU Rendering (snes/ppu.c)
  ├─ BG layers (tilemaps)
  ├─ Sprites (OAM)
  ├─ Mode 7 (rotation/scaling)
  └─ Color math
  ↓
Output (opengl.c or SDL software)
  ├─ Optional: GLSL shaders (glsl_shader.c)
  └─ Frame buffer → Screen
```

**Graphics Modules:**
- `src/load_gfx.c` (68KB): VRAM management, graphics decompression
- `snes/ppu.c` (60KB): SNES PPU emulation
  - Tile rendering
  - Sprite compositing
  - Mode 7 transformations
  - Color blending
- `src/opengl.c` (9KB): OpenGL rendering backend
- `src/glsl_shader.c` (25KB): Pixel shader support

**New APIs:**
- `PpuGetFrameBuffer()`: Access raw frame buffer for screenshots

### Audio System

```
Audio Frame
  ↓
SPC Player (spc_player.c)
  ↓
APU/DSP Emulation (snes/)
  ├─ SPC-700 CPU (spc.c)
  ├─ DSP (dsp.c)
  └─ Sample generation
  ↓
Optional: MSU Audio (audio.c)
  ↓
SDL Mixer → Output
```

**Audio Modules:**
- `src/audio.c` (20KB): SDL audio interface, MSU audio support
- `src/spc_player.c` (48KB): SPC playback
- `snes/spc.c` (39KB): SPC-700 CPU emulation
- `snes/dsp.c` (24KB): SNES DSP emulation
- `snes/apu.c` (5KB): APU registers

**MSU Audio:** Modern audio track replacement system (lossless audio support).

### SNES Hardware Emulation

Located in `snes/` directory - complete SNES hardware emulation for verification mode:

- `cpu.c` (68KB): 65816 CPU emulation
- `ppu.c` (60KB): Picture Processing Unit
- `dma.c` (10KB): DMA controller
- `snes.c` (12KB): System integration
- `cart.c` (4KB): Cartridge mapping

**Purpose:** Allows running original ROM code side-by-side for frame-by-frame verification.

### Configuration System

**Runtime Configuration:**
- `zelda3.ini`: User-editable config file
- `src/config.c` (large): INI parser, key/gamepad mapping

**Gamepad Binding API:**
```c
// Add binding
void GamepadMap_Add(int button, uint32 modifiers, uint16 cmd);

// Clear all bindings
void GamepadMap_Clear(void);

// Lookup
int GamepadMap_GetBindingForCommand(int cmd, uint32 *modifiers_out);
const char* FindCmdName(int cmd);
```

### Platform Abstraction

**File I/O Layer:** (`src/platform.h`, `src/platform.c`)
```c
PlatformFile *Platform_OpenFile(const char *filename, const char *mode);
size_t Platform_ReadFile(void *ptr, size_t size, size_t count, PlatformFile *file);
// ... etc
```

Default implementation uses standard C `FILE*`. Future platforms can override.

**Platform-Specific Code:**
- `src/platform/win32/`: Windows volume control
- `src/platform/switch/`: Nintendo Switch port

## Asset Pipeline

Located in `assets/` directory - Python tools for ROM extraction:

```
zelda3.sfc (ROM)
  ↓
restool.py
  ├─ extract_resources.py
  │   ├─ Graphics extraction
  │   ├─ Text extraction
  │   └─ Map extraction
  ↓
Intermediate files (tables/*.png, tables/*.yaml)
  ↓
compile_resources.py
  ├─ sprite_sheets.py
  ├─ text_compression.py
  └─ compile_music.py
  ↓
zelda3_assets.dat (~2MB)
```

**Asset File Format:** Custom binary format containing:
- Compressed graphics
- Tilemap data
- Text/dialogue
- Sprite sheets
- Audio samples (SPC)

## Data Flow

### Frame Execution
```c
// main.c
while (running) {
  HandleInput();               // Process SDL events
  ZeldaRunFrame(input_state);  // Execute game logic
  RenderFrame();               // Draw to screen
  SDL_Delay(frame_time);       // 60 FPS timing
}

// zelda_rtl.c
bool ZeldaRunFrame(int input_state) {
  // Update game state
  // Module calls based on game mode
  // Returns true if frame completed
}
```

### State Machine
Game uses module/submodule system:
```c
main_module_index    // 0-25 (title, overworld, dungeon, etc.)
submodule_index      // Substates within module
```

Each module has update functions called per-frame.

## Coordinate Systems

**Fixed Point Math:**
- Positions are 16-bit integers (pixel precision)
- Subpixel precision stored separately:
  ```c
  link_x_coord        // Whole pixels
  link_subpixel_x     // Fractional component
  ```

**Screen Space:**
- Base: 256×224 (SNES native)
- Widescreen: 256+kPpuExtraLeftRight×2 (configurable)
- Aspect ratios: 4:3, 16:9, 16:10

## Performance Considerations

**Hot Paths:**
- `ZeldaRunFrame()`: Called 60 times/second
- PPU rendering: Per-scanline operations
- Sprite AI: Hundreds of entities per frame

**Optimizations:**
- Inlined accessor macros
- Lookup tables for trigonometry
- Fast renderer option (`NewRenderer=1`)
- No dynamic allocation in game loop

## Extension Points

**Adding Features:**
1. Define flag in `src/features.h`
2. Store in unused RAM (0x648+)
3. Add INI option in `zelda3.ini`
4. Implement in relevant module
5. Guard with feature check

**Adding Platforms:**
1. Create `src/platform/<name>/`
2. Implement platform-specific overrides
3. Add conditional compilation
4. Update CMakeLists.txt

**Adding Renderers:**
1. Implement `RendererFuncs` interface
2. Add to output method enum
3. Register in `main.c`

## Code Style

**Naming Conventions:**
- Functions: `Module_FunctionName()` or `ModuleFunctionName()`
- Globals: `g_variable_name`
- Constants: `kConstantName`
- Macros: `ALL_CAPS` or `lowercase_with_underscores`

**File Organization:**
- Header: Type definitions, function declarations
- Source: Implementation
- Large modules split: `sprite.c` + `sprite_main.c`

## Debugging Tools

**Built-in:**
- Snapshot system (F1-F10): Save/load/replay game state
- Turbo mode (Tab): Fast-forward
- Cheats: W (health), Shift+W (items), etc.
- Performance display (F): Show FPS

**Development:**
- `kDebugFlag`: Compile-time debug mode
- `frame_counter`: Global frame count
- Verification mode: Compare with original ROM

## References

- Original disassembly: [Spannerisms' Zelda3 disassembly](https://github.com/spannerisms/zelda3)
- SNES PPU: [LakeSnes](https://github.com/elzo-d/LakeSnes)
- SDL2 documentation: [libsdl.org](https://www.libsdl.org/)
