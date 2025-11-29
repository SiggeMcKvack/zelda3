# Zelda3 Launcher

The Zelda3 GTK3 Launcher provides a graphical interface for configuring game settings without manually editing the `zelda3.ini` configuration file.

## Table of Contents

- [Installation](#installation)
- [Building the Launcher](#building-the-launcher)
- [Using the Launcher](#using-the-launcher)
- [Settings Tabs](#settings-tabs)
  - [General](#general)
  - [Graphics](#graphics)
  - [Sound](#sound)
  - [Features](#features)
  - [Keyboard](#keyboard)
  - [Gamepad](#gamepad)
- [First Run](#first-run)
- [Troubleshooting](#troubleshooting)

## Installation

### Prerequisites

The launcher requires GTK3 to be installed on your system.

**macOS (Homebrew):**
```bash
brew install gtk+3
```

**Linux (Debian/Ubuntu):**
```bash
sudo apt-get install libgtk-3-dev
```

**Linux (Fedora):**
```bash
sudo dnf install gtk3-devel
```

**Linux (Arch):**
```bash
sudo pacman -S gtk3
```

**Windows:**
- Download and install GTK3 from https://www.gtk.org/docs/installations/windows
- Or use MSYS2: `pacman -S mingw-w64-x86_64-gtk3`

## Building the Launcher

The launcher is built alongside the main game using CMake:

```bash
mkdir build && cd build
cmake .. -DBUILD_LAUNCHER=ON
cmake --build . -j$(nproc)
```

**Note:** The launcher is enabled by default. If GTK3 is not found, CMake will skip the launcher build and only build the main game.

To explicitly disable the launcher:
```bash
cmake .. -DBUILD_LAUNCHER=OFF
```

After building, you'll have two executables:
- `zelda3` - The main game executable
- `zelda3-launcher` - The GTK3 launcher

## Using the Launcher

### Starting the Launcher

From the build directory:
```bash
./zelda3-launcher
```

The launcher looks for `zelda3.ini` in the current directory. If no configuration file exists, it will create one with default settings.

### Buttons

- **Cancel** - Close the launcher without saving changes
- **Save** - Save configuration to `zelda3.ini` and close
- **Save & Launch** - Save configuration and launch the game

### Configuration File Location

The launcher operates on `./zelda3.ini` in the current working directory. This is the same file the game uses, so any changes made in the launcher will be reflected when you run the game.

## Settings Tabs

### General

The General tab manages game assets and language settings:

**Asset File Status:**
- Shows current status of `zelda3_assets.dat`
- Green indicator: Asset file found and valid
- Red indicator: Asset file missing or invalid

**Language Selection:**
- Dropdown populated from available languages in asset file
- Options depend on which dialogue files were extracted
- Default: US English

**Create Asset File:**

If you don't have `zelda3_assets.dat`, you can create it directly from the launcher:

1. **ROM Path** - Click "Browse..." to select your ROM file
   - Supports .sfc and .smc formats
   - Only USA ROMs fully supported for all features

2. **Create Asset File** - Click to run extraction
   - Shows progress in status area
   - Displays success message or error details
   - Automatically refreshes language dropdown on success

**Error Messages:**
- "Detected German ROM" - Non-USA ROM detected, extraction limited
- "ROM not found" - Invalid path or missing file
- "Unknown ROM" - ROM doesn't match any known version (check SHA1)

### Graphics

Configure visual rendering options:

- **Output Method** - Rendering backend selection:
  - SDL (software renderer)
  - OpenGL (hardware-accelerated)
  - OpenGL ES (mobile/embedded)
  - Vulkan (modern cross-platform)

- **Window Size** - Game window dimensions:
  - Auto (native resolution)
  - 1x (256x224 - original SNES resolution)
  - 2x (512x448)
  - 3x (768x672)
  - 4x (1024x896)
  - 5x (1280x1120)
  - Fullscreen

- **Extended Aspect Ratio** - Widescreen support:
  - Off (4:3 original)
  - extend_y, 16:9 (recommended)
  - extend_y, 16:10
  - Other presets (18:9)
  - Custom ratios via zelda3.ini (e.g., 21:9, 32:9)

- **Linear Filtering** - Smooth scaling (on/off)
- **Integer Scaling** - Pixel-perfect scaling (on/off)

- **Shader File** - GLSL shader for visual effects:
  - Click "Browse..." to open file chooser
  - Supports .glsl and .glslp shader files
  - Selected path saved to zelda3.ini

**Recommendations:**
- For modern displays: OpenGL or Vulkan with 16:9 extended aspect ratio
- For retro feel: SDL with 2x/3x window size and integer scaling

### Sound

Adjust audio settings:

- **Music Volume** (0-128) - Background music volume
- **SFX Volume** (0-128) - Sound effects volume
- **MSU Audio Volume** (0-255) - CD-quality audio track volume (if using MSU-1 audio pack)
- **Stereo** (on/off) - Enable stereo audio

- **MSU Folder** - Directory containing MSU audio tracks:
  - Click "Browse..." to open folder chooser
  - Select directory with MSU audio files
  - Path automatically saved to zelda3.ini with `/alttp_msu-` suffix

**Note:** MSU Audio is an enhanced audio pack with CD-quality music tracks. See the main documentation for details on obtaining MSU audio files.

### Features

Toggle enhanced features (all disabled by default for original behavior):

- **Item Switch with L/R** - Use shoulder buttons to cycle items
- **Switch Item on B Button Press** - Alternative item switching
- **Collect Items with Sword** - Pick up items by swinging sword
- **Run in Town and Houses** - Remove walk-only restrictions
- **Daggers on Some Pots** - Throw daggers at certain objects
- **Warp Without S+Q** - Fast warp without save-quit
- **Warp on Mirror Outside** - Mirror works in more areas
- **Random Overworld Spawns** - Randomize enemy spawns
- **Fixed Volume Tables** - Improved audio mixing
- **Ow MSU1 with FadeOut** - Smooth music transitions in overworld
- **Skip MSU1 Resume** - Don't resume MSU tracks
- **MSU Resume After S+Q** - Resume music after save-quit
- **Equip Sword with Item** - Sword + item combinations
- **Switch Palettes** - Alternative color schemes
- **Free Play Mode** - Start with all items/upgrades
- **Turbo Mode** - Faster gameplay speed

**Important:** These features are enhancements beyond the original game. For authentic SNES experience, leave all features disabled.

### Keyboard

Configure keyboard controls:

- **Up/Down/Left/Right** - Movement
- **Select/Start** - Menu/pause
- **A/B/X/Y** - Action buttons
- **L/R** - Shoulder buttons

Click each input field and press the desired key to bind it. Each binding has a "Clear" button to remove the assignment.

**Default controls:**
- Arrow Keys - Movement
- Enter - Start
- Shift - Select
- Z - B Button
- X - A Button
- A/S - L/R Buttons

**Clear Bindings:**
Every keyboard binding includes a "Clear" button to unbind the key without assigning a new one. This is useful when you want to disable specific controls.

### Gamepad

Configure game controller mappings with full button remapping UI.

The Gamepad tab is organized into 4 subtabs matching the keyboard layout:

#### Controls Subtab
Map the core SNES controller buttons:
- **Up/Down/Left/Right** - D-pad movement
- **Select/Start** - Menu navigation
- **A/B/X/Y** - Action buttons
- **L/R** - Shoulder buttons

#### Save States Subtab
Configure snapshot controls:
- **Load (F1-F10)** - Load save state slots
- **Save (Shift+F1-F10)** - Save state slots
- **Replay (Ctrl+F1-F10)** - Replay recordings

#### Cheats Subtab
Map cheat activation buttons:
- **CheatLife** - Refill health and magic
- **CheatKeys** - Set dungeon key count
- **CheatWalkThroughWalls** - Enable no-clip mode

#### System Subtab
Map system control buttons:
- **Fullscreen** - Toggle fullscreen mode
- **Reset** - Reset the game
- **Pause/PauseDimmed** - Pause with/without dimming
- **Turbo/ReplayTurbo** - Fast-forward controls
- **WindowBigger/WindowSmaller** - Resize window
- **VolumeUp/VolumeDown** - Adjust audio volume
- **StopReplay** - Stop replay playback
- **ClearKeyLog** - Clear input recording log (debug)

**Binding Buttons:**
1. Click any binding button - it shows "Press button..."
2. Press a gamepad button or trigger
3. The detected button is displayed and saved
4. For modifier combos (e.g., L2+A), hold L2 first, then press A

**Clear Bindings:**
Each gamepad binding has a "Clear" button to remove the assignment without rebinding.

**Gamepad Settings:**
- **Enable Gamepad** - Turn gamepad support on/off
- **Controller Port** - Select which controller to use (0-3)

**Supported Controllers:**
- Xbox controllers (One, Series X/S)
- PlayStation controllers (DualShock 4, DualSense)
- Nintendo Switch Pro Controller
- Any SDL2-compatible gamepad

## First Run

When you run the launcher for the first time:

1. The launcher checks for `zelda3.ini` in the current directory
2. If not found, it creates a new config file with default settings
3. You'll see: `✓ Created default config at ./zelda3.ini`
4. The launcher window opens with all default values loaded
5. Adjust any settings you want, then click "Save & Launch" to start the game

## Troubleshooting

### Launcher Won't Build

**Problem:** CMake reports "GTK3 not found"

**Solution:**
- Make sure GTK3 is installed (see [Installation](#installation))
- On macOS arm64: Ensure GTK3 is installed via Homebrew in `/opt/homebrew`
- On Linux: Install the `-dev` or `-devel` package, not just the runtime
- Check `pkg-config --modversion gtk+-3.0` to verify installation

### Game Executable Not Found

**Problem:** Clicking "Save & Launch" shows error "Game executable not found: ./zelda3"

**Solution:**
- Make sure you're running the launcher from the same directory as the `zelda3` executable
- Build the main game first: `cmake --build . --target zelda3`
- Both executables should be in the same directory

### Configuration Not Saving

**Problem:** Changes don't persist after closing the launcher

**Solution:**
- Check file permissions in the current directory
- Make sure you clicked "Save" or "Save & Launch", not "Cancel"
- Verify `zelda3.ini` exists and is writable
- Check the terminal output for error messages

### GTK Warnings in Terminal

**Problem:** Seeing GTK warnings like "Allocating size to GtkWindow without calling gtk_widget_get_preferred_width/height"

**Solution:**
- These are harmless warnings from the GTK3 library
- They don't affect functionality
- Can be safely ignored

### Gamepad Not Detected

**Problem:** Controller not working in launcher

**Solution:**
- Ensure controller is connected before starting the launcher
- Check `SDL_INIT_GAMECONTROLLER` initialization succeeded
- Try reconnecting the controller
- Verify controller works with other SDL2 applications

### Wrong Architecture (macOS)

**Problem:** Build fails with "found x86_64 but target is arm64"

**Solution:**
- Reinstall GTK3 via Homebrew: `brew reinstall gtk+3`
- Clear CMake cache: `rm -rf build && mkdir build`
- CMake will automatically detect the correct architecture

## Advanced Configuration

For advanced users, you can manually edit `zelda3.ini`. The launcher generates a well-commented configuration file with all available options.

Key sections:
- `[General]` - Autosave, aspect ratio, scaling
- `[Graphics]` - Rendering, window settings
- `[Sound]` - Volume controls
- `[Features]` - Enhanced feature toggles
- `[KeyMap]` - Keyboard bindings (scancode format)
- `[GamepadMap]` - Controller bindings

See `docs/configuration.md` for detailed explanation of all options.

## See Also

- [Installation Guide](installation.md) - Building the game and launcher
- [Configuration Reference](configuration.md) - Detailed config file format
- [Getting Started](getting-started.md) - First-time setup and gameplay
- [Troubleshooting](troubleshooting.md) - Common issues and solutions
