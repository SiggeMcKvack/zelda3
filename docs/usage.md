# Usage Guide

[← Back to Documentation Index](index.md)

Complete guide to using Zelda3, including controls, enhanced features, and configuration.

## Running the Game

### Basic Usage

```sh
./zelda3
```

On Windows:
```cmd
zelda3.exe
```

### Verification Mode

Run with a ROM path to verify that the C code matches original SNES behavior frame-by-frame:

```sh
./zelda3 path/to/zelda3.sfc
```

This enables verification mode where RAM state is compared after each frame against the original ROM execution.

## Controls

### Default Keyboard Controls

| Button | Default Key |
|--------|-------------|
| Up     | Up Arrow    |
| Down   | Down Arrow  |
| Left   | Left Arrow  |
| Right  | Right Arrow |
| Start  | Enter       |
| Select | Right Shift |
| A      | X           |
| B      | Z           |
| X      | S           |
| Y      | A           |
| L      | C           |
| R      | V           |

**All keys can be reconfigured in `zelda3.ini`**

### Debug & Utility Keys

| Key           | Action                              |
|---------------|-------------------------------------|
| Alt+Enter     | Toggle Fullscreen                   |
| Ctrl+Up       | Increase Window Size                |
| Ctrl+Down     | Decrease Window Size                |
| Tab           | Turbo Mode                          |
| T             | Toggle Replay Turbo Mode            |
| P             | Pause (with dim)                    |
| Shift+P       | Pause (without dim)                 |
| Ctrl+E        | Reset Game                          |
| R             | Toggle Fast/Slow Renderer           |
| F             | Display Renderer Performance        |

### Snapshot System

Zelda3 includes a powerful snapshot system that saves game state and input history:

| Key           | Action                              |
|---------------|-------------------------------------|
| F1-F10        | Load Snapshot                       |
| Shift+F1-F10  | Save Snapshot                       |
| Ctrl+F1-F10   | Replay Snapshot (with turbo)        |
| L             | Stop Replaying Snapshot             |
| K             | Clear Input History from Joypad Log |

**Dungeon Snapshots:**

| Key      | Action                                 |
|----------|----------------------------------------|
| 1-9      | Load Dungeon Playthrough Snapshot      |
| Ctrl+1-9 | Run Dungeon Playthrough (turbo mode)   |

### Cheat Keys

| Key           | Action                              |
|---------------|-------------------------------------|
| W             | Fill Health/Magic                   |
| Shift+W       | Fill Rupees/Bombs/Arrows            |
| O             | Set Dungeon Key Count to 1          |

## Gamepad Support

Zelda3 supports gamepad/controller input via SDL2. Gamepads are automatically detected.

**Default Gamepad Mappings:**
- **D-Pad:** Directional movement
- **A/B/X/Y:** SNES buttons (B→A, A→B, Y→X, X→Y)
- **Lb/Rb:** L/R shoulder buttons
- **Start/Back:** Start/Select
- **L2+R3:** Quick Save (slot 1)
- **L2+L3:** Quick Load (slot 1)

**To configure gamepad:**
1. Edit `zelda3.ini`
2. Find the `[GamepadMap]` section
3. Customize button mappings using gamepad button names (A, B, X, Y, DpadUp, Lb, Rb, etc.)
4. Use modifiers (e.g., `L2+A`) for complex bindings

See [Configuration](#configuration) below for complete gamepad configuration details.

## Enhanced Features

Zelda3 includes several enhancements not available in the original SNES game. **All features are disabled by default** to preserve original behavior.

### Aspect Ratio & Display

- **Widescreen Support:** 16:9, 16:10, or 18:9 aspect ratios
- **Extended vertical display:** 240 lines instead of 224
- **Higher Quality World Map:** Enhanced Mode 7 rendering
- **No sprite limits:** Eliminates 8 sprites per scanline limit
- **Pixel Shaders:** Custom GLSL shader support (OpenGL only)
- **Custom Link sprites:** ZSPR format support

**Configure in `zelda3.ini`:**
```ini
[Graphics]
# Widescreen with extended vertical display
ExtendedAspectRatio = extend_y, 16:10

# Enhanced graphics
EnhancedMode7 = 1
NoSpriteLimits = 1

# Custom Link sprite (optional)
LinkGraphics = sprites-gfx/snes/zelda3/link/sheets/megaman-x.2.zspr

# GLSL shader (OpenGL only)
Shader = glsl-shaders/crt-geom.glslp
```

### Audio Enhancements

- **MSU Audio Support:** Multiple formats (PCM, Opus, Deluxe variants)
- **Configurable Audio Latency:** Adjust buffer size for your system
- **Volume Control:** Per-track MSU volume

**MSU Audio Setup:**
1. Place MSU-1 audio tracks in `msu/` directory
2. Enable in `zelda3.ini`:
   ```ini
   [Sound]
   # MSU format options: false, true, deluxe, opuz, deluxe-opuz
   EnableMSU = true

   # Path prefix (numbers and extensions added automatically)
   MSUPath = msu/alttp_msu-

   # Audio frequency (44100 for PCM, 48000 for OPUZ)
   AudioFreq = 44100

   # MSU volume control
   MSUVolume = 100%
   ```

### Control Enhancements

- **L/R Item Switching:** Cycle through items with shoulder buttons
- **Item reordering:** Y+direction in inventory to rearrange
- **Multi-button assignment:** Hold X/L/R in menu to assign items
- **Turn while dashing:** Normally restricted to cardinal directions

**Enable in `zelda3.ini`:**
```ini
[Features]
# L/R item switching (includes menu enhancements)
ItemSwitchLR = 1

# Limit to first 4 items only (optional)
ItemSwitchLRLimit = 0

# Allow turning while dashing
TurnWhileDashing = 0
```

### Gameplay Modifications

- **Mirror to Dark World:** Magic mirror warps both directions
- **Collect with sword:** Pick up items using sword
- **Break pots with sword:** Level 2-4 sword breaks pots
- **More active bombs:** 4 bombs instead of 2
- **Increased rupee capacity:** 9999 instead of 999
- **Cancel bird travel:** Press X to exit bird flight

**Enable in `zelda3.ini`:**
```ini
[Features]
MirrorToDarkworld = 0
CollectItemsWithSword = 0
BreakPotsWithSword = 0
MoreActiveBombs = 0
CarryMoreRupees = 0
CancelBirdTravel = 0
```

### Quality of Life

- **Disable low health beep:** Silence annoying beep sound
- **Skip intro on keypress:** Fast intro skip
- **Max items indicator:** Orange/yellow color when items maxed

**Enable in `zelda3.ini`:**
```ini
[Features]
DisableLowHealthBeep = 0
SkipIntroOnKeypress = 0
ShowMaxItemsInYellow = 0
```

### Bug Fixes

- **Misc bug fixes:** Minor bugs without gameplay impact
- **Game-changing fixes:** Bugs that affect mechanics (may break replays)

**Enable in `zelda3.ini`:**
```ini
[Features]
MiscBugFixes = 0
GameChangingBugFixes = 0
```

### Experimental Features

**⚠️ Warning:** These features are experimental and may affect gameplay balance.

- **Pokemode:** Capture enemies with Bug Net, store in bottles, release as allies
- **Princess Zelda follower:** Zelda as permanent companion

**Enable in `zelda3.ini`:**
```ini
[Features]
Pokemode = 0
PrincessZeldaHelps = 0
```

## Configuration

### zelda3.ini

The configuration file `zelda3.ini` is created automatically on first run. It contains all customizable settings.

**Location:**
- Linux/macOS: Same directory as executable
- Windows: Same directory as `zelda3.exe`
- Android: SDL external storage path

**Note:** `zelda3.user.ini` is loaded first if it exists. Use `!include path/to/file.ini` to include other config files.

### Configuration Sections

#### [General]
```ini
[General]
# Language selection (requires appropriate asset file)
# Options: us, en, de, fr, fr-c, es, pl, pt, nl, sv, redux
Language = de

# Automatically save state on quit and reload on start
Autosave = 0

# Display performance metrics (FPS) in window title
DisplayPerfInTitle = 0

# Disable SDL_Delay for 60Hz displays (advanced)
DisableFrameDelay = 0

# Extended aspect ratio (widescreen support)
# Options: 4:3 (default), 16:9, 16:10, 18:9
# Modifiers (comma-separated):
#   extend_y: Display 240 lines instead of 224
#   unchanged_sprites: Preserve sprite spawn/die for replays
#   no_visual_fixes: Disable graphics glitch fixes
ExtendedAspectRatio = extend_y, 16:10
```

See [Translation Support](#translation-support) for language options.

#### [Graphics]
```ini
[Graphics]
# Window size (Auto or WidthxHeight like 1024x768)
WindowSize = Auto

# Window scale multiplier when WindowSize is Auto
# 1=256x224, 2=512x448, 3=768x672, etc.
WindowScale = 3

# Fullscreen mode
# 0 = Windowed
# 1 = Desktop fullscreen (borderless, recommended)
# 2 = Fullscreen with mode change
Fullscreen = 0

# Ignore aspect ratio when scaling (stretch to fill)
IgnoreAspectRatio = 0

# Output rendering method
# Options: SDL, SDL-Software, OpenGL, OpenGL ES, Vulkan
OutputMethod = SDL

# Use linear filtering for smoother pixels
# Disable for crisp, pixelated look
LinearFiltering = 0

# Use optimized SNES PPU renderer (faster)
NewRenderer = 1

# Display world map with higher resolution (Enhanced Mode 7)
EnhancedMode7 = 1

# Remove SNES sprite limit (8 per scanline)
# Eliminates sprite flickering
NoSpriteLimits = 1

# Recreate Virtual Console flash dimming (accessibility)
DimFlashes = 0

# Custom Link sprite (ZSPR format)
# LinkGraphics = sprites-gfx/snes/zelda3/link/sheets/megaman-x.2.zspr

# GLSL shader (OpenGL output method only)
# Shader = glsl-shaders/crt-geom.glslp
```

#### [Sound]
```ini
[Sound]
# Enable audio output
EnableAudio = 1

# Audio sample rate (Hz)
# Use 44100 for PCM MSU, 48000 for OPUZ MSU
# Options: 11025, 22050, 32000, 44100, 48000
AudioFreq = 44100

# Audio channels (1=mono, 2=stereo)
AudioChannels = 2

# Audio buffer size in samples (power of 2)
# Lower = less latency but may crackle
# Higher = more latency but smoother
# Options: 256, 512, 1024, 2048, 4096
AudioSamples = 512

# MSU audio support
# Options: false, true, deluxe, opuz, deluxe-opuz
EnableMSU = false

# Path to MSU files (number and extension added automatically)
MSUPath = msu/alttp_msu-

# Resume MSU position when re-entering area
ResumeMSU = 1

# MSU playback volume (0-100 with or without %)
MSUVolume = 100%
```

#### [Features]

See [Enhanced Features](#enhanced-features) section for complete list. All features default to 0 (disabled).

```ini
[Features]
# Control enhancements
ItemSwitchLR = 0
ItemSwitchLRLimit = 0
TurnWhileDashing = 0

# Gameplay modifications
MirrorToDarkworld = 0
CollectItemsWithSword = 0
BreakPotsWithSword = 0
MoreActiveBombs = 0
CarryMoreRupees = 0
CancelBirdTravel = 0

# Quality of life
DisableLowHealthBeep = 0
SkipIntroOnKeypress = 0
ShowMaxItemsInYellow = 0

# Bug fixes
MiscBugFixes = 0
GameChangingBugFixes = 0

# Experimental
Pokemode = 0
PrincessZeldaHelps = 0
```

#### [KeyMap]

Keyboard controls using SDL key names with optional modifiers (Shift+, Ctrl+, Alt+).

```ini
[KeyMap]
# SNES controller buttons: Up, Down, Left, Right, Select, Start, A, B, X, Y, L, R
Controls = Up, Down, Left, Right, Right Shift, Return, x, z, s, a, c, v

# Save states (F1-F10)
Load = F1, F2, F3, F4, F5, F6, F7, F8, F9, F10
Save = Shift+F1, Shift+F2, Shift+F3, Shift+F4, Shift+F5, Shift+F6, Shift+F7, Shift+F8, Shift+F9, Shift+F10
Replay = Ctrl+F1, Ctrl+F2, Ctrl+F3, Ctrl+F4, Ctrl+F5, Ctrl+F6, Ctrl+F7, Ctrl+F8, Ctrl+F9, Ctrl+F10

# Cheats
CheatLife = w
CheatKeys = o
CheatWalkThroughWalls = Ctrl+e

# Debug tools
ClearKeyLog = k
StopReplay = l
Fullscreen = Alt+Return
Reset = Ctrl+r
PauseDimmed = p
Pause = Shift+p
Turbo = Tab
ReplayTurbo = t
WindowBigger = Ctrl+Up
WindowSmaller = Ctrl+Down
VolumeUp = Shift+=
VolumeDown = Shift+-
```

#### [GamepadMap]

Gamepad button mappings. All keyboard commands from [KeyMap] can be bound to gamepad.

**Button names:**
- Face: A, B, X, Y
- D-pad: DpadUp, DpadDown, DpadLeft, DpadRight
- Shoulders: Lb, Rb (bumpers), L2, R2 (triggers)
- System: Start, Back, Guide
- Sticks: L3, R3 (click)

**Modifiers:** Use + to combine (e.g., `L2+A` = hold L2, press A)

```ini
[GamepadMap]
# SNES controller buttons: Up, Down, Left, Right, Select, Start, A, B, X, Y, L, R
Controls = DpadUp, DpadDown, DpadLeft, DpadRight, Back, Start, B, A, Y, X, Lb, Rb

# Quick Save/Load (enabled by default)
Save = L2+R3
Load = L2+L3

# Additional save state slots (uncomment to use)
# Save = L2+A, L2+B, L2+X, L2+Y
# Load = R2+A, R2+B, R2+X, R2+Y
# Turbo = L3
# Fullscreen = L3+R3
```

### Case-Sensitive Filesystems (Linux)

On case-sensitive filesystems, file paths in `zelda3.ini` must match exact capitalization:

**Incorrect:**
```ini
MSUPath = MSU/    # Will fail if folder is named "msu/"
```

**Correct:**
```ini
MSUPath = msu/    # Matches actual folder name
```

Zelda3 validates paths at startup and warns about case mismatches. Use the suggested corrections from the error messages.

## Translation Support

Zelda3 supports multiple languages through ROM extraction:

**Supported Languages:**
- `us` - English (USA)
- `en` - English (Europe)
- `de` - German
- `fr` - French
- `fr-c` - French (Canada)
- `es` - Spanish
- `pl` - Polish
- `pt` - Portuguese
- `nl` - Dutch
- `sv` - Swedish
- `redux` - English Redux (ROM hack)

**To use a translation:**

1. **Extract dialogue from translated ROM:**
   ```sh
   python3 assets/restool.py --extract-dialogue -r german.sfc
   ```

2. **Build assets with language:**
   ```sh
   python3 assets/restool.py --languages=de
   ```

3. **Set language in `zelda3.ini`:**
   ```ini
   [General]
   Language = de
   ```

See [Getting Started](getting-started.md) for ROM requirements.

## Renderer Selection

Zelda3 supports multiple rendering backends:

| Renderer | Description | Platforms |
|----------|-------------|-----------|
| **SDL** | Hardware-accelerated (default) | All platforms |
| **SDL-Software** | Software renderer (CPU) | All platforms |
| **OpenGL** | OpenGL 3.3+ (required for shaders) | Desktop (Linux, macOS, Windows) |
| **OpenGL ES** | Hardware-accelerated | Android, mobile |
| **Vulkan** | Vulkan 1.0 cross-platform | Desktop, Android (requires SDK/MoltenVK) |

**Performance recommendations:**
- **Desktop:** SDL (default) or OpenGL for shader support
- **Android:** OpenGL ES (default) or Vulkan
- **Low-end systems:** SDL-Software renderer
- **Raspberry Pi:** SDL-Software

**Set in `zelda3.ini`:**
```ini
[Graphics]
OutputMethod = OpenGL
```

## Performance Optimization

### Reducing Latency

1. **Lower audio buffer size** (may cause crackling on slower systems):
   ```ini
   [Sound]
   AudioSamples = 256
   ```

2. **Use hardware rendering**:
   ```ini
   [Graphics]
   OutputMethod = SDL
   ```

3. **Enable optimized PPU renderer**:
   ```ini
   [Graphics]
   NewRenderer = 1
   ```

### Improving Stability

1. **Increase audio buffer size**:
   ```ini
   [Sound]
   AudioSamples = 4096
   ```

2. **Use software rendering** (more compatible):
   ```ini
   [Graphics]
   OutputMethod = SDL-Software
   ```

## Troubleshooting

See the [Troubleshooting Guide](troubleshooting.md) for solutions to common issues.

### Quick Fixes

**Game won't start:**
- Ensure `zelda3_assets.dat` is in the same directory as the executable
- Check console output for error messages

**No audio:**
- Verify SDL2 audio subsystem is working: `SDL_AUDIODRIVER=pulse ./zelda3`
- Increase buffer size in `zelda3.ini`

**Poor performance:**
- Switch to hardware rendering (OpenGL)
- Lower window scale in `zelda3.ini`
- Close other applications

**Controls not working:**
- Verify `zelda3.ini` key mappings
- Check if gamepad is detected: `SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD=0 ./zelda3`

## Advanced Usage

### Frame-by-Frame Verification

Compare C implementation against original SNES ROM:

```sh
./zelda3 path/to/zelda3.sfc
```

Zelda3 will:
1. Run both C code and original SNES machine code in parallel
2. Compare RAM state after each frame
3. Report any discrepancies (bugs in C reimplementation)

This mode is slower but useful for development and debugging.

### Replay System

Snapshots include joypad input history, allowing deterministic replay:

1. **Save a snapshot** during gameplay (Shift+F1-F10)
2. **Load and replay** in turbo mode (Ctrl+F1-F10)
3. **Verify correctness** by checking that replays produce identical results

The replay system enables:
- **Regression testing:** Ensure code changes don't break gameplay
- **Speedrunning:** Replay inputs for verification
- **Bug reproduction:** Save and share exact input sequences

## Next Steps

- **[Installation Guide](installation.md)** - Build instructions for your platform
- **[Architecture](architecture.md)** - Technical architecture overview
- **[Development](development.md)** - Contributing to the project
- **[Debugging](debugging.md)** - Debug builds and tools

---

[← Back to Documentation Index](index.md)
