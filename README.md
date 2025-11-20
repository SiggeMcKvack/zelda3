# Zelda3 - A Link to the Past

A reverse-engineered C reimplementation of The Legend of Zelda: A Link to the Past for SNES.

[![Discord](https://img.shields.io/discord/XXXXXXXXXX?label=Discord&logo=discord)](https://discord.gg/AJJbJAzNNJ)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE.txt)

## About

This is a ~70-80kLOC C reimplementation of the original SNES game, reverse-engineered with function and variable names from community disassembly efforts. The game is fully playable from start to finish.

**Supported Platforms:** Linux, macOS, Windows, Nintendo Switch, and Android

## Key Features

- **Complete reimplementation** - All game logic recreated in portable C
- **Frame-perfect verification** - Can run alongside original ROM for validation
- **Enhanced features** - Optional widescreen support, MSU audio, pixel shaders
- **Cross-platform** - Runs on desktop, mobile, and console platforms
- **Snapshot system** - Save/load/replay game state with input history
- **Multiple renderers** - SDL software, OpenGL/OpenGL ES, Vulkan support
- **GTK3 launcher** - Graphical settings editor (optional, desktop only)

## Quick Start

### 1. Get the Assets

You need a US region ROM to extract game assets:

```bash
# Install Python dependencies
python3 -m pip install -r requirements.txt

# Place your zelda3.sfc ROM in the project root
# SHA256: 66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb

# Extract assets
python3 assets/restool.py --extract-from-rom
```

This creates `zelda3_assets.dat` containing all game resources.

### 2. Build

**Linux/macOS:**
```bash
# Install dependencies
# Ubuntu/Debian: sudo apt install libsdl2-dev libopus-dev cmake build-essential
# macOS: brew install sdl2 opus cmake

# Optional: Install GTK3 for graphical launcher
# Ubuntu/Debian: sudo apt install libgtk-3-dev
# macOS: brew install gtk+3

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

**Windows:**
```cmd
# Using vcpkg for dependencies:
vcpkg install sdl2:x64-windows opus:x64-windows

# Build with CMake
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

**See detailed instructions:** [Installation Guide](docs/installation.md) | [Platform-Specific Guides](docs/index.md#building--installation)

### 3. Run

```bash
./zelda3
```

Or on Windows: `zelda3.exe`

**Optional:** Use the graphical launcher to configure settings (if GTK3 is installed):
```bash
./zelda3-launcher
```

## Controls

| Button | Default Key |
|--------|-------------|
| D-Pad  | Arrow Keys  |
| A      | X           |
| B      | Z           |
| Start  | Enter       |
| Select | Right Shift |

**Gamepad:** Plug-and-play support with quick save (L2+R3) and quick load (L2+L3).

**Customize controls** in `zelda3.ini` after first run.

**Useful shortcuts:**
- `Alt+Enter` - Toggle fullscreen
- `F1-F10` - Load snapshot
- `Shift+F1-F10` - Save snapshot
- `Tab` - Turbo mode

[Full controls and features →](docs/usage.md)

## Documentation

- **[Getting Started](docs/getting-started.md)** - First-time setup guide
- **[Installation](docs/installation.md)** - Detailed build instructions
- **[Usage Guide](docs/usage.md)** - Controls, features, and configuration
- **[Launcher Guide](docs/launcher.md)** - GTK3 settings editor (optional)
- **[Architecture](docs/architecture.md)** - Technical architecture overview
- **[Contributing](CONTRIBUTING.md)** - How to contribute
- **[Changelog](CHANGELOG.md)** - Recent updates

[Browse all documentation →](docs/index.md)

## Enhanced Features

Optional enhancements not in the original game:

- **Widescreen support** (16:9, 16:10 aspect ratios)
- **MSU audio** (high-quality soundtrack tracks)
- **Pixel shaders** (custom visual effects)
- **Higher quality world map**
- **Secondary item slot** on button X
- **L/R item switching**

All features are disabled by default to preserve original behavior. Enable in `zelda3.ini`.

## Community

- **Discord:** [https://discord.gg/AJJbJAzNNJ](https://discord.gg/AJJbJAzNNJ)
- **Report bugs:** [GitHub Issues](https://github.com/snesrev/zelda3/issues)
- **Wiki:** [https://github.com/snesrev/zelda3/wiki](https://github.com/snesrev/zelda3/wiki)

## Contributing

Contributions are welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

**Quick links:**
- [Development Guide](docs/development.md)
- [Code Architecture](docs/architecture.md)
- [Debugging Tools](docs/debugging.md)

## License

This project is licensed under the MIT License. See [LICENSE.txt](LICENSE.txt) for details.

## Credits

- Original game by Nintendo
- Base C reimplementation by [snesrev/zelda3](https://github.com/snesrev/zelda3)
- Community disassembly by [spannerisms](https://github.com/spannerisms/zelda3) and contributors
- PPU/DSP implementation from [LakeSnes](https://github.com/elzo-d/LakeSnes)
- Android port integration from [Waterdish/zelda3-android](https://github.com/Waterdish/zelda3-android)
- Touchpad controls from [Eden emulator](https://git.eden-emu.dev/eden-emu/eden) project
