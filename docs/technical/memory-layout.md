# Memory Layout

[Home](../index.md) > [Architecture](../architecture.md) > Memory Layout

## Overview

Zelda3's memory architecture mirrors the original SNES memory layout using a unified `g_ram` buffer accessed through macro wrappers. This design preserves the original memory addresses and enables replay compatibility, snapshot/restore functionality, and ROM verification.

## Memory Regions

### Main RAM (g_ram)

The `g_ram` buffer is the primary storage for all game state, equivalent to the SNES WRAM (Work RAM):

```c
// Declared in zelda_rtl.c
uint8 g_ram[0x20000];  // 128KB buffer
```

**Memory Map:**
- **0x0000-0x1FFF:** Zero page and stack (8KB)
- **0x0000-0x00FF:** Zero page (direct addressing, fastest access)
- **0x0100-0x01FF:** Stack
- **0x0200-0x1FFF:** General game state
- **0x2000-0x1FFFF:** Extended RAM space

**Key Areas:**
- **0x0010-0x0020:** Core system state (module indexes, frame counter)
- **0x0020-0x0080:** Link state (position, velocity, direction, health)
- **0x0300-0x03FF:** Sprite state arrays
- **0x0400-0x04FF:** Ancilla (projectile) state
- **0x0648-0x0658:** Enhanced features (unused in original SNES)

### Video RAM (g_vram)

Video RAM stores tile graphics and sprite data:

```c
// Accessed via g_zenv.vram
uint8 vram[0x10000];  // 64KB
```

**VRAM Layout:**
- Organized in 16-bit words
- Stores tile graphics (8x8 pixel tiles)
- Tile data format: 2bpp, 4bpp, or 8bpp planar
- Managed by `load_gfx.c` module

### Save RAM (g_sram)

Save RAM stores persistent game progress:

```c
// Accessed via g_zenv.sram
uint8 sram[0x2000];  // 8KB
```

**SRAM Contents:**
- Three save slots
- Player progress, inventory
- Dungeon completion flags
- Heart pieces collected

## RAM Macro System

All game state is accessed through macro wrappers defined in `src/variables.h`. This provides:
- Consistent naming
- Type safety
- Zero runtime overhead (inlined)
- Easy porting from disassembly

### Macro Definition Pattern

```c
// src/variables.h
#define main_module_index (*(uint8*)(g_ram+0x10))
#define submodule_index (*(uint8*)(g_ram+0x11))
#define frame_counter (*(uint8*)(g_ram+0x1A))
#define link_x_coord (*(uint16*)(g_ram+0x22))
#define link_y_coord (*(uint16*)(g_ram+0x20))
```

**Macro Anatomy:**
```c
#define link_x_coord (*(uint16*)(g_ram+0x22))
        │            │  │       │      │
        │            │  │       │      └─ Offset in RAM
        │            │  │       └─ Base RAM pointer
        │            │  └─ Type cast (uint16*)
        │            └─ Dereference operator
        └─ Macro name (descriptive)
```

### Usage Examples

```c
// Read value
uint16 x = link_x_coord;
// Expands to: uint16 x = *(uint16*)(g_ram+0x22);

// Write value
link_x_coord = 100;
// Expands to: *(uint16*)(g_ram+0x22) = 100;

// Modify value
link_x_coord += link_x_vel;
// Expands to: *(uint16*)(g_ram+0x22) += *(uint8*)(g_ram+0x31);

// Pointer access (for arrays)
uint8 *sprites_x = (uint8*)(g_ram + 0x0D00);
for (int i = 0; i < 16; i++) {
  sprites_x[i] = initial_x + i * 16;
}
```

### Common RAM Macros

**System State:**
```c
#define main_module_index (*(uint8*)(g_ram+0x10))     // Game mode (title, overworld, dungeon)
#define submodule_index (*(uint8*)(g_ram+0x11))       // Substate within module
#define frame_counter (*(uint8*)(g_ram+0x1A))         // Global frame counter (wraps at 256)
#define player_is_indoors (*(uint8*)(g_ram+0x1B))     // 0=overworld, 1=dungeon/cave
```

**Link State:**
```c
#define link_x_coord (*(uint16*)(g_ram+0x22))         // X position (pixels)
#define link_y_coord (*(uint16*)(g_ram+0x20))         // Y position (pixels)
#define link_z_coord (*(uint16*)(g_ram+0x24))         // Z position (height when jumping)
#define link_subpixel_x (*(uint8*)(g_ram+0x2B))       // X subpixel (fractional movement)
#define link_subpixel_y (*(uint8*)(g_ram+0x2A))       // Y subpixel
#define link_direction_facing (*(uint8*)(g_ram+0x2F)) // 0=up, 2=down, 4=left, 6=right
#define link_health_current (*(uint8*)(g_ram+0x??))   // Current hearts (x8 for heart pieces)
```

**Input State:**
```c
#define button_mask_b_y (*(uint8*)(g_ram+0x3A))       // B/Y button state
#define bitfield_for_a_button (*(uint8*)(g_ram+0x3B)) // A button state
#define button_b_frames (*(uint8*)(g_ram+0x3C))       // B button hold duration
```

## Feature Flags System

Enhanced features use special unused RAM locations (0x648+) that weren't used by the original SNES game. This allows new features without breaking compatibility.

### Feature Flag Storage

```c
// src/features.h
enum {
  kRam_Features0 = 0x64c,  // Feature bitmask storage location
};

#define enhanced_features0 (*(uint32*)(g_ram+0x64c))
```

### Feature Flag Definitions

```c
// src/features.h
enum {
  kFeatures0_ExtendScreen64 = 1,           // Widescreen support
  kFeatures0_SwitchLR = 2,                 // L/R item switching
  kFeatures0_TurnWhileDashing = 4,         // Turn during dash
  kFeatures0_MirrorToDarkworld = 8,        // Mirror to dark world
  kFeatures0_CollectItemsWithSword = 16,   // Sword collects items
  kFeatures0_BreakPotsWithSword = 32,      // Sword breaks pots
  kFeatures0_DisableLowHealthBeep = 64,    // Silence low health beep
  kFeatures0_SkipIntroOnKeypress = 128,    // Skip intro with button
  kFeatures0_ShowMaxItemsInYellow = 256,   // Highlight maxed items
  kFeatures0_MoreActiveBombs = 512,        // Allow more bombs
  kFeatures0_WidescreenVisualFixes = 1024, // Widescreen visual fixes
  kFeatures0_CarryMoreRupees = 2048,       // Increase rupee capacity
  kFeatures0_MiscBugFixes = 4096,          // Various bug fixes
  kFeatures0_CancelBirdTravel = 8192,      // Cancel bird travel
  kFeatures0_GameChangingBugFixes = 16384, // Game-changing fixes
  kFeatures0_SwitchLRLimit = 32768,        // Limit L/R switching
  kFeatures0_DimFlashes = 65536,           // Dim screen flashes
  kFeatures0_Pokemode = 131072,            // Pokemode feature
  kFeatures0_PrincessZeldaHelps = 262144,  // Zelda assists player
};
```

### Checking Feature Flags

```c
// Check if feature is enabled
if (enhanced_features0 & kFeatures0_SwitchLR) {
  // L/R item switching is enabled
  HandleItemSwitch();
}

// Check multiple features
if ((enhanced_features0 & kFeatures0_CollectItemsWithSword) &&
    (enhanced_features0 & kFeatures0_BreakPotsWithSword)) {
  // Both sword features enabled
}

// Disable original behavior
if (!(enhanced_features0 & kFeatures0_DisableLowHealthBeep)) {
  PlayLowHealthBeep();
}
```

## Access Patterns

### Reading State

```c
// Simple read
uint16 player_x = link_x_coord;

// Conditional logic
if (link_health_current == 0) {
  Player_Die();
}

// Multi-byte reads
uint16 position_x = link_x_coord;
uint16 position_y = link_y_coord;
```

### Writing State

```c
// Simple write
link_x_coord = 100;

// Modify-in-place
link_x_coord += 5;
frame_counter++;

// Conditional write
if (button_pressed) {
  link_delay_timer_spin_attack = 16;
}
```

### Array Access

Some RAM locations represent arrays:

```c
// Sprite arrays (16 sprites max)
#define sprite_x_lo ((uint8*)(g_ram+0x0D00))
#define sprite_y_lo ((uint8*)(g_ram+0x0D20))

// Access individual sprite
sprite_x_lo[sprite_id] = new_x;
sprite_y_lo[sprite_id] = new_y;

// Iterate over sprites
for (int i = 0; i < 16; i++) {
  if (sprite_ai_state[i] != 0) {
    UpdateSprite(i);
  }
}
```

### Subpixel Precision

The game uses separate variables for whole pixels and fractional components:

```c
// Movement with subpixel precision
link_subpixel_x += velocity_x;
if (link_subpixel_x >= 256) {
  link_x_coord++;
  link_subpixel_x -= 256;
}

// This provides smoother movement at slower speeds
```

## Memory Safety

**Best Practices:**

1. **Always use macros:** Never access `g_ram` directly
   ```c
   // Good
   link_x_coord = 100;

   // Bad
   *(uint16*)(g_ram+0x22) = 100;
   ```

2. **Respect types:** Use the correct type for each macro
   ```c
   // Good
   uint16 x = link_x_coord;  // link_x_coord is uint16

   // Bad
   uint8 x = link_x_coord;   // Truncates to 8 bits!
   ```

3. **Bounds checking:** Verify array indexes
   ```c
   if (sprite_id >= 0 && sprite_id < 16) {
     sprite_x_lo[sprite_id] = new_x;
   }
   ```

4. **Avoid name conflicts:** Prefix with `g_` if needed
   ```c
   // Windows defines R10, R12, etc. as register names
   #define g_r10 (*(uint16*)(g_ram+0x10))  // Prefixed to avoid conflict
   ```

## Snapshot/Restore

The unified memory design enables save states:

```c
// Save snapshot
uint8 snapshot[sizeof(g_ram)];
memcpy(snapshot, g_ram, sizeof(g_ram));

// Restore snapshot
memcpy(g_ram, snapshot, sizeof(g_ram));
```

This is used for:
- F1-F10 quick save/load
- Replay recording/playback
- ROM verification mode

## ROM Verification Mode

The memory layout matches the original SNES, allowing side-by-side execution:

```c
// Run both implementations
ZeldaRunFrame(input);           // Reimplementation
SnesExecuteFrame(input);        // Original ROM via emulator

// Compare RAM state
if (memcmp(g_ram, snes_ram, sizeof(g_ram)) != 0) {
  ReportDivergence();
}
```

## Performance Considerations

**Zero Overhead:**
- RAM macros compile to direct memory access (no function calls)
- Inlined by compiler: `link_x_coord` → `*(uint16*)(g_ram+0x22)`
- Same performance as direct pointer access

**Cache Locality:**
- Frequently accessed state is concentrated at low addresses (0x10-0x100)
- Zero page variables fit in a single cache line
- Hot paths (player, sprites) have excellent locality

## See Also

- **[Architecture](../architecture.md)** - Overall system architecture
- **[Feature Flags](feature-flags.md)** - How to add new features using RAM
- **[Graphics Pipeline](graphics-pipeline.md)** - How VRAM is used
- **Code References:**
  - `src/variables.h` - All RAM macro definitions
  - `src/features.h` - Feature flag definitions
  - `src/zelda_rtl.c` - RAM buffer declaration

**External References:**
- [SNES Memory Map](https://wiki.superfamicom.org/memory-mapping)
- [Zelda3 Disassembly](https://github.com/spannerisms/zelda3)
