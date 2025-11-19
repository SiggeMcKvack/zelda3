# Installation Guide

[← Back to Documentation Index](index.md)

Complete build instructions for Zelda3 on all supported platforms.

## Quick Navigation

- [Prerequisites](#prerequisites)
- [Asset Extraction](#asset-extraction-required)
- [Building with CMake](#building-with-cmake)
- **Platform-Specific Guides:**
  - [Windows](platforms/windows.md)
  - [Linux](platforms/linux.md)
  - [macOS](platforms/macos.md)
  - [Android](platforms/android.md)
  - [Nintendo Switch](platforms/switch.md)
- [Troubleshooting](troubleshooting.md)

## Prerequisites

### Desktop Platforms (Linux, macOS, Windows)

- **Python 3.x** with pip
- **SDL2** development libraries
- **Opus** audio codec library
- **CMake 3.10+**
- **C Compiler** (GCC, Clang, or MSVC)

### Platform-Specific Dependencies

See the detailed platform guides for installation instructions:
- **[Windows](platforms/windows.md)** - vcpkg, Visual Studio, or MinGW
- **[Linux](platforms/linux.md)** - Distribution-specific package managers
- **[macOS](platforms/macos.md)** - Homebrew
- **[Android](platforms/android.md)** - Android SDK, NDK, Java 17+
- **[Nintendo Switch](platforms/switch.md)** - DevKitPro toolchain

## Asset Extraction (Required)

Before building or running Zelda3, you must extract game assets from your ROM.

### Required ROM

- **Filename:** `zelda3.sfc`
- **Region:** USA
- **SHA256:** `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`

### Extract Assets

```bash
# Place zelda3.sfc in project root
python3 -m pip install -r requirements.txt
python3 assets/restool.py --extract-from-rom

# Or specify ROM path
python3 assets/restool.py --extract-from-rom -r /path/to/zelda3.sfc
```

This creates `zelda3_assets.dat` (~2MB) containing all game resources.

**Important:** Keep `zelda3_assets.dat` in the same directory as the executable when running the game.

## Building with CMake

### Quick Start

```bash
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
./zelda3
```

### Build Configurations

**Release Build (optimized, default):**
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

**Debug Build (symbols, no optimization):**
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j$(nproc)
```

**Specify Compiler:**
```bash
cmake .. -DCMAKE_C_COMPILER=clang
cmake .. -DCMAKE_C_COMPILER=gcc-12
```

**Enable Warnings as Errors:**
```bash
cmake .. -DENABLE_WERROR=ON
```

### CMake Options

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | Release, Debug | Release | Optimization level |
| `ENABLE_WERROR` | ON, OFF | OFF | Treat warnings as errors |
| `CMAKE_C_COMPILER` | clang, gcc, etc | System default | C compiler |
| `CMAKE_INSTALL_PREFIX` | path | `/usr/local` | Installation prefix |

### Automatic Dependency Detection

CMake automatically detects and configures:
- **SDL2** (required) - Game framework
- **Opus** (required) - Audio codec
- **OpenGL** (optional) - Hardware rendering
- **Vulkan** (optional) - Modern graphics API

If a dependency is missing, CMake will report it during configuration with installation suggestions.

### Installation

```bash
# Install to /usr/local/bin (requires sudo on Linux/macOS)
sudo cmake --install .

# Install to custom prefix
cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --install .
```

### Cleaning Build Directory

```bash
# Remove and recreate build directory
cd .. && rm -rf build && mkdir build && cd build

# Or clean and rebuild
cmake --build . --clean-first
```

## Platform-Specific Builds

For detailed platform-specific instructions, see:

### [Windows](platforms/windows.md)
- Visual Studio setup
- vcpkg dependency management
- MSBuild and command-line builds
- Windows-specific troubleshooting

### [Linux](platforms/linux.md)
- Ubuntu/Debian dependencies
- Fedora/Red Hat dependencies
- Arch Linux dependencies
- Case-sensitive filesystem considerations

### [macOS](platforms/macos.md)
- Homebrew dependencies
- Apple Silicon (M1/M2) support
- Code signing and quarantine issues
- MoltenVK for Vulkan support

### [Android](platforms/android.md)
- Android Studio setup
- Gradle build system
- APK deployment
- Asset installation on device
- Performance optimization

### [Nintendo Switch](platforms/switch.md)
- DevKitPro installation
- Switch-specific build process
- NRO deployment
- Atmosphere setup

## Advanced Build Options

### Cross-Compilation

```bash
# Example: Build for ARM64 on x86_64
cmake .. -DCMAKE_SYSTEM_PROCESSOR=arm64 \
         -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc
```

### Custom Dependency Locations

```bash
# Specify SDL2 location
cmake .. -DSDL2_INCLUDE_DIR=/path/to/include \
         -DSDL2_LIBRARY=/path/to/lib/libSDL2.so

# Specify Opus location
cmake .. -DOPUS_INCLUDE_DIR=/path/to/include \
         -DOPUS_LIBRARY=/path/to/lib/libopus.so
```

### Parallel Builds

```bash
# Use all available cores (default)
cmake --build . -j$(nproc)

# Limit to specific number of cores
cmake --build . -j4
```

### Out-of-Source Builds

CMake supports multiple build configurations simultaneously:

```bash
# Create separate build directories
mkdir build-release build-debug

# Configure release build
cd build-release
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .

# Configure debug build
cd ../build-debug
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

## IDE Integration

CMake can generate project files for popular IDEs:

### Visual Studio Code

1. Install **CMake Tools** extension
2. Open project folder
3. Select kit (compiler) from status bar
4. Press F7 to build

### CLion

1. Open project (CLion auto-detects `CMakeLists.txt`)
2. Select build configuration (Debug/Release)
3. Build → Build Project (Ctrl+F9)

### Visual Studio

```bash
cmake .. -G "Visual Studio 17 2022"
# Open zelda3.sln in Visual Studio
```

### Xcode

```bash
cmake .. -G Xcode
open zelda3.xcodeproj
```

## CI/CD Integration

Example GitHub Actions workflow:

```yaml
name: Build Zelda3

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Install Dependencies
        run: |
          sudo apt update
          sudo apt install -y libsdl2-dev libopus-dev cmake build-essential python3 python3-pip
          python3 -m pip install -r requirements.txt

      - name: Extract Assets
        run: python3 assets/restool.py --extract-from-rom -r zelda3.sfc

      - name: Build
        run: |
          mkdir build && cd build
          cmake .. -DCMAKE_BUILD_TYPE=Release
          cmake --build . -j$(nproc)

      - name: Test
        run: ./build/zelda3 --version
```

## Verification

After building, verify the installation:

```bash
# Check executable exists
./zelda3 --help

# Verify assets are present
ls -lh zelda3_assets.dat

# Run the game
./zelda3
```

## Next Steps

- **[Usage Guide](usage.md)** - Learn how to play and configure
- **[Troubleshooting](troubleshooting.md)** - Fix common build issues
- **[Architecture](architecture.md)** - Understand the codebase
- **[Contributing](contributing.md)** - Help improve the project

---

[← Back to Documentation Index](index.md)
