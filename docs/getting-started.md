# Getting Started with Zelda3

[← Back to Documentation Index](index.md)

This guide will help you get Zelda3 up and running as quickly as possible.

## What is Zelda3?

Zelda3 is a reverse-engineered C reimplementation of The Legend of Zelda: A Link to the Past. It's approximately 70-80k lines of C code that reimplements all parts of the original game, making it playable from start to end on modern platforms.

**Supported Platforms:** Linux, macOS, Windows, Nintendo Switch, and Android

## System Requirements

- **ROM:** US version of The Legend of Zelda: A Link to the Past (`zelda3.sfc`)
  - SHA256: `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`
- **Python 3.x** (for asset extraction)
- **SDL2** (for graphics/audio/input)
- **Build tools:** CMake 3.10+, C compiler (GCC, Clang, or MSVC)

## Quick Installation

You have three options to get started:

### Option 1: Windows Launcher (Easiest for Windows Users)

Download and use RadzPrower's Launcher: [Zelda-3-Launcher](https://github.com/ajohns6/Zelda-3-Launcher)

This handles asset extraction and provides a GUI for launching the game.

### Option 2: Pre-built Binaries

Check the [Releases page](https://github.com/snesrev/zelda3/releases) for pre-built binaries for your platform.

You'll still need to extract assets (see below).

### Option 3: Build from Source

See the [Installation Guide](installation.md) for complete build instructions for your platform.

## Asset Extraction (Required)

Before running Zelda3, you need to extract game assets from your ROM:

1. **Install Python dependencies:**
   ```sh
   python3 -m pip install -r requirements.txt
   ```

2. **Place your ROM:** Copy your US ROM file as `zelda3.sfc` in the project root

3. **Extract assets:**
   ```sh
   python3 assets/restool.py --extract-from-rom
   ```

   This creates `zelda3_assets.dat` containing all game resources (levels, graphics, sounds).

**Note:** After extraction, the ROM is no longer needed. Keep `zelda3_assets.dat` alongside the executable.

## First Run

### Running the Game

```sh
./zelda3
```

On Windows:
```cmd
zelda3.exe
```

### Basic Controls

| Button | Default Key |
|--------|-------------|
| D-Pad  | Arrow Keys  |
| A      | X           |
| B      | Z           |
| X      | S           |
| Y      | A           |
| L      | C           |
| R      | V           |
| Start  | Enter       |
| Select | Right Shift |

**Keyboard controls can be customized in `zelda3.ini`**

**Gamepad support:** Controllers are automatically detected. Default bindings include quick save (L2+R3) and quick load (L2+L3). Customize in `[GamepadMap]` section of `zelda3.ini`.

### Common Shortcuts

| Key           | Action                    |
|---------------|---------------------------|
| Alt+Enter     | Toggle Fullscreen         |
| Ctrl+Up/Down  | Adjust Window Size        |
| P             | Pause (with dim)          |
| Tab           | Turbo Mode                |
| F1-F10        | Load Snapshot             |
| Shift+F1-F10  | Save Snapshot             |

## Next Steps

- **[Usage Guide](usage.md)** - Full controls, features, and configuration options
- **[Installation Guide](installation.md)** - Platform-specific build instructions
- **[Configuration](usage.md#configuration)** - Customize zelda3.ini settings
- **[Troubleshooting](troubleshooting.md)** - Solutions to common issues

## Getting Help

- **Discord:** [https://discord.gg/AJJbJAzNNJ](https://discord.gg/AJJbJAzNNJ)
- **GitHub Issues:** [Report bugs or request features](https://github.com/snesrev/zelda3/issues)
- **Wiki:** [https://github.com/snesrev/zelda3/wiki](https://github.com/snesrev/zelda3/wiki)

## Enhanced Features

Zelda3 includes several optional enhancements not in the original game:

- **Widescreen support** (16:9 or 16:10 aspect ratios)
- **Pixel shaders** for visual effects
- **MSU audio** support for high-quality soundtracks
- **Higher quality world map**
- **Secondary item slot** on button X
- **Item switching** with L/R buttons

See the [Usage Guide](usage.md) for details on enabling these features.

---

[← Back to Documentation Index](index.md)
