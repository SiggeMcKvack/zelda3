# Zelda3 Launcher

The Zelda3 launcher is a small desktop app for configuring game settings, creating the asset file and starting the game, without editing `zelda3.ini` by hand. It is built with [Tauri 2](https://tauri.app): a Rust backend that calls the GTK-free C launcher core (`src/launcher_core/`) over FFI, and a plain HTML/CSS/JS frontend rendered by the OS webview.

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

Release builds ship the launcher with the game inside:

| Platform | Package | Notes |
|----------|---------|-------|
| macOS | `Zelda3.app` (zip) | Unsigned: right-click → Open the first time |
| Windows | `zelda3-windows-*-setup.exe` (NSIS installer) | Needs the WebView2 runtime (preinstalled on Windows 10/11) |
| Linux | `zelda3-linux-amd64-*.deb` | Pulls in SDL2, opus, libyaml, libsamplerate, OpenAL and webkit2gtk |

## Building the Launcher

The launcher lives in `launcher-tauri/src-tauri/`. Its `build.rs` builds `launcher_core_lib` from the top-level CMake project, so you need the game's build dependencies plus a Rust toolchain.

**Extra prerequisites:**
- Rust (stable), via [rustup](https://rustup.rs)
- `libclang` (for bindgen): Xcode command line tools on macOS, `libclang-dev` on Linux, LLVM on Windows
- `libsamplerate` (for the MSU → OPUZ encoder)
- Linux only: `libwebkit2gtk-4.1-dev librsvg2-dev libxdo-dev libssl-dev`

**Development run:**
```bash
cd launcher-tauri/src-tauri
cargo run
```
The frontend is embedded at build time; no npm or `cargo tauri` CLI is needed for development. After editing files in `launcher-tauri/frontend/`, run `touch tauri.conf.json` so the next `cargo build` re-embeds them. To test **Save & Launch** in a dev build, copy the `zelda3` game binary next to `target/debug/zelda3-launcher`.

**Bundling** (what CI does in `.github/workflows/release.yml`):
```bash
cargo install tauri-cli --version "^2" --locked
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j

# The game ships as a Tauri sidecar named zelda3-game-<rust target triple>
mkdir -p launcher-tauri/src-tauri/binaries
cp build/zelda3 "launcher-tauri/src-tauri/binaries/zelda3-game-$(rustc -vV | sed -n 's/^host: //p')"

cd launcher-tauri/src-tauri
ZELDA3_CMAKE_BUILD_DIR=$PWD/../../build cargo tauri build --config tauri.bundle.conf.json --bundles app   # or deb / nsis

# macOS only: copy Homebrew dylibs into the bundle and re-sign ad-hoc
../../scripts/bundle-macos.sh target/release/bundle/macos/Zelda3.app
```
`ZELDA3_CMAKE_BUILD_DIR` makes `build.rs` link against an existing CMake build instead of configuring its own. On Windows, also pass `--config tauri.bundle.windows.conf.json` and stage the vcpkg DLLs in `binaries/dlls/`.

## Using the Launcher

### Buttons

- **Reset to defaults** - Overwrite `zelda3.ini` with the default configuration
- **Validate** - Check the current settings without saving
- **Save** - Save configuration to `zelda3.ini`
- **Save & Launch** - Save configuration and launch the game

### Configuration File Location

The launcher keeps `zelda3.ini`, `zelda3_assets.dat` and the game's `saves/` in a per-user data directory:

| Platform | Directory |
|----------|-----------|
| macOS | `~/Library/Application Support/Zelda3` |
| Linux | `$XDG_CONFIG_HOME/zelda3` (default `~/.config/zelda3`) |
| Windows | `%APPDATA%\Zelda3` |

The launcher starts the game with this directory as its working directory. When the game is run directly, it uses the current directory if it contains `zelda3_assets.dat`, and otherwise falls back to its executable directory (the old behavior).

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

- **Shader File** - Shader for visual effects:
  - Click "Browse..." to open file chooser
  - Supports .glsl/.glslp (OpenGL) and .slang/.slangp (Vulkan) formats
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

- **MSU** - MSU audio format dropdown (Disabled, PCM, PCM Deluxe, Opuz, Opuz Deluxe)
- **MSU Folder** - Directory containing MSU audio tracks:
  - Click "Browse..." to select the MSU folder
  - **Auto-detection:** The launcher automatically scans the folder and detects:
    - Format (PCM or Opuz) by reading file headers
    - Deluxe status by checking for track numbers > 47
    - Filename prefix pattern
  - The dropdown and path are updated automatically based on detected format
  - Info label shows: "115 MSU tracks detected (PCM Deluxe)"

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

Click each binding button and press the desired key to bind it. Right-click a binding to remove the assignment.

**Default controls:**
- Arrow Keys - Movement
- Enter - Start
- Shift - Select
- Z - B Button
- X - A Button
- A/S - L/R Buttons

**Clear Bindings:**
Right-click any keyboard binding to unbind the key without assigning a new one. This is useful when you want to disable specific controls.

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
Right-click a binding button to remove the assignment without rebinding.

**Gamepad Settings:**
- **Enable Gamepad** - Turn gamepad support on/off
- **Controller Port** - Select which controller to use (0-3)

**Supported Controllers:**
- Xbox controllers (One, Series X/S)
- PlayStation controllers (DualShock 4, DualSense)
- Nintendo Switch Pro Controller
- Any controller the browser Gamepad API reports with the standard mapping

## First Run

When you run the launcher for the first time:

1. It creates the per-user data directory (see [Configuration File Location](#configuration-file-location))
2. If there is no `zelda3.ini` there, it writes one with default settings
3. On the General tab, pick your `zelda3.sfc` ROM and create the asset file
4. Adjust any settings you want, then click "Save & Launch" to start the game

Upgrading from the old GTK launcher: settings and assets used to live next to the executable. Copy your old `zelda3.ini`, `zelda3_assets.dat` and `saves/` into the per-user data directory, or let the launcher create new ones.

## Troubleshooting

### Launcher Won't Build

**Problem:** `cargo build` fails in `build.rs`

**Solution:**
- `libyaml (yaml-0.1) not found via pkg-config`: install libyaml (`brew install libyaml`, `apt install libyaml-dev`)
- `bindgen failed`: install libclang (see [Building the Launcher](#building-the-launcher))
- CMake configure errors: the top-level game must configure; install SDL2 and opus first

### Game Executable Not Found

**Problem:** Clicking "Save & Launch" shows "game executable not found"

**Solution:**
- Release builds ship the game as `zelda3-game` next to the launcher; reinstall if it is missing
- Dev builds: copy the CMake-built `zelda3` next to `target/debug/zelda3-launcher`

### Gamepad Not Detected

**Problem:** Controller not working in the launcher

**Solution:**
- Press any button on the controller after the launcher opens; webviews only expose a gamepad after the first input
- Try reconnecting the controller

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

- [Installation Guide](installation.md) - Building the game
- [Configuration Reference](configuration.md) - Detailed config file format
- [Getting Started](getting-started.md) - First-time setup and gameplay
- [Troubleshooting](troubleshooting.md) - Common issues and solutions
