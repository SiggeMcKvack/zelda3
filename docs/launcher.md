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
  - [Keyboard and Gamepad](#keyboard-and-gamepad)
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

- **Reset to defaults** - Overwrite `zelda3.ini` with the default configuration (asks for confirmation)
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

The Graphics, Sound and Features tabs are generated from `launcher-tauri/frontend/js/schema.js`; each control maps to one `zelda3.ini` key. See the comments in `zelda3.ini` for what each option does.

### General

Manages the asset file and the game language:

- **Asset File** - Shows whether `zelda3_assets.dat` exists in the data directory and which languages it contains.
- **Language** - The dialogue language to use, from the languages in the asset file.
- **Create Asset File from ROM:**
  1. **Base US ROM** - Browse to your US `zelda3.sfc`. The launcher identifies the ROM and shows e.g. "Recognized: ..." or "Not a recognized ALttP ROM."
  2. **Language ROMs or .ips patches (optional)** - Add translated ROMs (German, French, ...) to include their dialogue. Each one is tagged with its detected language and can be removed again. Instead of a translated ROM you can add its `.ips` translation patch (select the base US ROM first). The patch is applied to the US ROM in memory, whether it was made for a ROM with or without a 512-byte header, and is accepted only if the result is a known translated ROM. This works for the Dutch and Swedish patches.
  3. **Create Asset File** - Builds `zelda3_assets.dat` in the data directory and refreshes the language list. The button is disabled while the build runs.

### Graphics

- **Output Method** - SDL, SDL-Software, OpenGL, OpenGL ES or Vulkan
- **Window Width / Height** (0 = auto) and **Window Scale** (used in auto mode)
- **Display Mode** - Windowed, Borderless Fullscreen or Fullscreen
- **Aspect Ratio** - 4:3, 16:9, 16:10, 18:9 or Custom (set **Custom Aspect Width/Height**)
- Toggles: stretch to fill window, extend render height to 240 lines, linear filtering, optimized PPU renderer, Enhanced Mode 7, no sprite limit, FPS in window title, disable frame delay, dim flashes
- **Shader File** - `.glsl`/`.glslp` (OpenGL) or `.slang`/`.slangp` (Vulkan), with a Browse… button
- **Link Sprite File** - A `.zspr` file replacing Link's graphics, with a Browse… button

### Sound

- **Enable Audio**, **Audio Channels** (Mono/Stereo), **Audio Frequency**, **Audio Samples**
- **MSU** - Disabled, PCM, PCM Deluxe, Opuz or Opuz Deluxe. Changing it also sets the audio frequency to match (44100 for PCM, 48000 for Opuz).
- **MSU Volume** (0-100)
- **Resume MSU position** when re-entering an overworld area
- **MSU folder** - Browse to the folder with your MSU-1 tracks. The launcher scans it, picks the most common filename prefix, and shows how many PCM and OPUZ tracks it found.
- **Track filename prefix** - The part before the track number (e.g. `alttp_msu-` for `alttp_msu-1.pcm`). Folder and prefix are saved together as `MSUPath`.
- **Encode to OPUZ** - Shown when the folder has PCM tracks. Converts them to OPUZ (much smaller), switches the MSU mode to the matching Opuz mode, then asks whether to move the PCM files to a `pcm/` subfolder or delete them.

**Note:** MSU-1 audio packs replace the game's music with CD-quality tracks. See [Usage Guide](usage.md#audio-enhancements) for details.

### Features

Enhancements beyond the original game, grouped as in `zelda3.ini`. All are off by default; leave them off for the original experience.

- **Save** - Autosave on quit and reload on start
- **Controls** - L/R item switching (optionally limited to the first 4 items), turn while dashing
- **Gameplay** - Skip intro on keypress, mirror warp to the Dark World, collect items with sword, more active bombs, 9999 rupees, cancel bird travel with X, break pots with sword (minimum sword level)
- **Interface** - Highlight maxed items, disable low health beep
- **Bug Fixes** - Fix misc bugs, fix bugs that change gameplay
- **Experimental** - Pokemode, Princess Zelda helps in battle

### Keyboard and Gamepad

Both tabs edit the same set of bindings, for keyboard (`[KeyMap]`) and controller (`[GamepadMap]`):

- **Controls** - Up, Down, Left, Right, Select, Start, A, B, X, Y, L, R
- **Load state / Save state / Replay** - Slots 1-10
- **Cheats** - Life / Magic, Keys, Walk through walls
- **System** - Clear key log, stop replay, toggle fullscreen, reset, pause, pause (dimmed), turbo, replay turbo, window bigger/smaller, volume up/down, toggle perf display, toggle renderer

**Binding:**
1. Click a binding; it shows "Press…"
2. Press a key or controller button. Hold Shift/Ctrl/Alt (keyboard) or another button (controller) first to record a combo, e.g. `Shift+F1` or `L2+L3`.
3. Click the binding again to cancel, or right-click it to clear it.

Keys are recorded by keyboard layout (AZERTY etc.) and saved as SDL key names, e.g. `Return`, `Right Shift`, `Keypad Enter`. Controller triggers are recorded as L2/R2; analog stick directions can't be bound.

**Default keyboard controls:**

| SNES | Key |
|------|-----|
| D-pad | Arrow keys |
| Select / Start | Right Shift / Return |
| A / B | X / Z |
| X / Y | S / A |
| L / R | C / V |

**Controllers:** any controller the webview's Gamepad API reports with the standard mapping (Xbox, PlayStation, Switch Pro, ...). Press a button after the launcher opens; webviews only expose a controller after the first input.

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
- `[General]` - Autosave, language, display options
- `[Graphics]` - Renderer, window, shader, Link sprite
- `[Sound]` - Audio and MSU settings
- `[Features]` - Enhanced feature toggles
- `[KeyMap]` - Keyboard bindings (SDL key names, with `Shift+`/`Ctrl+`/`Alt+` modifiers)
- `[GamepadMap]` - Controller bindings

See the [Usage Guide](usage.md#configuration) for details on all options.

## See Also

- [Installation Guide](installation.md) - Building the game
- [Usage Guide](usage.md#configuration) - Configuration options
- [Getting Started](getting-started.md) - First-time setup and gameplay
- [Troubleshooting](troubleshooting.md) - Common issues and solutions
