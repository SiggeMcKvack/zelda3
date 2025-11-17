# Debug Console Output

## Overview

In debug builds (`-DCMAKE_BUILD_TYPE=Debug`), zelda3 outputs event-driven game state information to the console. This helps track what's happening during gameplay without requiring a debugger.

## What Gets Logged

The debug system tracks and logs:

- **Module transitions**: When the game changes modes (file select → overworld → dungeon, etc.)
- **Room/area changes**: When Link enters a new dungeon room or overworld area
- **Sprite events**: When enemies/NPCs spawn or are removed
- **State snapshots**: Full state dump after each change event

## Output Format

```
[Frame: 1234] Debug state tracking initialized
[Frame: 1234] Initial state
  Module: FileSelect (1/0)
  Location: Area 0x0000
  Link: Pos=(512,384,0) Health=160/160 Dir=2 State=0
  Objects: 0 sprites, 0 ancillae

[Frame: 1890] Module change: FileSelect (1/0) -> Overworld (9/0)
[Frame: 1890] Current state
  Module: Overworld (9/0)
  Location: Area 0x001B
  Link: Pos=(1024,768,0) Health=160/160 Dir=2 State=0
  Objects: 3 sprites, 0 ancillae
  Sprite types: [0x83 0x83 0x91]

[Frame: 2450] Sprite spawned: 3 -> 4 active
[Frame: 2450] Current state
  Module: Overworld (9/0)
  Location: Area 0x001B
  Link: Pos=(1200,820,0) Health=152/160 Dir=1 State=0
  Objects: 4 sprites, 1 ancillae
  Sprite types: [0x83 0x83 0x91 0x15]

[Frame: 3120] Location change: Area 0x001B -> Room 0x0084 (dungeon)
[Frame: 3120] Current state
  Module: Dungeon (7/0)
  Location: Room 0x0084
  Link: Pos=(512,640,0) Health=152/160 Dir=0 State=0
  Objects: 2 sprites, 0 ancillae
  Sprite types: [0x83 0x91]
```

## Enabling Debug Output

### Build Configuration

Debug output is **only active in debug builds**. Debug builds have `NDEBUG` undefined, enabling all `#ifndef NDEBUG` gated code.

To build with debug output:

```bash
# Create debug build directory
mkdir build-debug && cd build-debug

# Configure with debug flags (NDEBUG will NOT be defined)
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build . -j$(nproc)
```

**Note:** Release builds automatically define `NDEBUG`, which removes all debug-only code at compile time.

### Runtime Configuration

Set the `ZELDA3_LOG_LEVEL` environment variable to control verbosity:

```bash
# Show all debug messages (required for debug state output)
ZELDA3_LOG_LEVEL=3 ./zelda3

# Show info and above (won't see debug state)
ZELDA3_LOG_LEVEL=2 ./zelda3

# Show warnings and errors only (default)
ZELDA3_LOG_LEVEL=1 ./zelda3
```

**Note:** Debug state logging uses `LOG_DEBUG` level, so you must set `ZELDA3_LOG_LEVEL=3` to see it.

## State Information Reference

### Module Index

Main game modes (`main_module_index`):
- 0: Intro sequence
- 1: File select screen
- 6: Pre-dungeon initialization
- 7: Dungeon gameplay
- 9: Overworld gameplay
- 12: Game over screen
- 14: Interface/menu
- 19: Triforce room
- 26: Credits

### Link Position

- Positions are in fixed-point 16-bit format
- Coordinates include X, Y (2D position) and Z (height for jumping)
- Direction: 0=up, 1=down, 2=left, 3=right

### Sprite Types

Active sprites show their type ID (hex). Common examples:
- 0x83: Soldier
- 0x91: Octorok
- 0x15: Green Zora

Sprite state values:
- 0-8: Inactive/dead
- 9+: Active

### Health

Health is stored in 8ths of a heart:
- 160 = 20 hearts (full health at game start)
- 8 = 1 heart
- Each heart container = 8 health units

## Implementation Details

**Files:**
- `src/debug_state.h` - Header with tracker structure
- `src/debug_state.c` - Implementation with change detection
- `src/zelda_rtl.c` - Hook point in `ZeldaRunFrame()` (line ~758)

**Gating:**
- All debug state code is wrapped in `#ifndef NDEBUG`
- Zero overhead in release builds (NDEBUG defined)
- Compiles to stub functions when NDEBUG is defined

**Event Detection:**
- Tracks previous frame's state
- Compares current vs previous each frame
- Only logs when changes are detected

## Use Cases

1. **Bug reproduction**: See exact state when crashes occur
2. **Behavior validation**: Verify module transitions happen correctly
3. **Sprite debugging**: Track when enemies spawn/die
4. **State investigation**: Understand game state without a debugger
5. **Testing features**: Verify changes don't affect state transitions

## Disabling

To disable debug output in a debug build, either:
1. Build with release mode: `cmake .. -DCMAKE_BUILD_TYPE=Release`
2. Set log level below debug: `ZELDA3_LOG_LEVEL=2` (or lower)
3. Comment out the `DebugState_Update()` call in `zelda_rtl.c:758`

## Technical Notes

- Runs once per frame (60 FPS)
- Minimal performance impact (only active in debug builds)
- Event-driven (only logs on changes, not every frame)
- Uses existing `variables.h` RAM macros for state access
- Outputs to stderr via logging system
- ANSI colors in TTY terminals
