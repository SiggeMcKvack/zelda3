# Building Zelda3 on Linux

[← Back to Installation Guide](../installation.md)

This guide covers building Zelda3 on Linux distributions including Ubuntu, Debian, Fedora, and Arch Linux.

## Prerequisites

- **Python 3.x** with pip
- **SDL2** development libraries
- **Opus** audio codec library
- **CMake 3.10+**
- **Build tools** (gcc or clang)

## Installing Dependencies

### Ubuntu/Debian

```bash
sudo apt install libsdl2-dev libopus-dev cmake build-essential python3 python3-pip
python3 -m pip install -r requirements.txt
```

### Fedora

```bash
sudo dnf install SDL2-devel opus-devel cmake gcc python3 python3-pip
python3 -m pip install -r requirements.txt
```

### Arch Linux

```bash
sudo pacman -S sdl2 opus cmake gcc python python-pip
python3 -m pip install -r requirements.txt
```

## Asset Extraction

Before building, you must extract game assets from a USA region ROM.

**Required ROM:**
- Filename: `zelda3.sfc`
- Region: USA
- SHA256: `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`

**Extract assets:**
```bash
# Place zelda3.sfc in project root, then:
python3 assets/restool.py --extract-from-rom

# Or specify ROM path:
python3 assets/restool.py --extract-from-rom -r /path/to/zelda3.sfc
```

This creates `zelda3_assets.dat` (~2MB) containing all game graphics, text, and maps.

## Building with CMake

### Quick Start

```bash
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
./zelda3
```

### Build Types

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

### Compiler Selection

**Use Clang:**
```bash
cmake .. -DCMAKE_C_COMPILER=clang
```

**Use specific GCC version:**
```bash
cmake .. -DCMAKE_C_COMPILER=gcc-12
```

### CMake Options

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | Release, Debug | Release | Build optimization level |
| `ENABLE_WERROR` | ON, OFF | OFF | Treat warnings as errors |
| `CMAKE_C_COMPILER` | clang, gcc, etc | System default | C compiler to use |

**Enable warnings as errors:**
```bash
cmake .. -DENABLE_WERROR=ON
```

## Installation

```bash
# Install to /usr/local/bin (requires sudo)
cd build
sudo cmake --install .

# Install to custom prefix (e.g., ~/.local)
cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --install .
```

## Case-Sensitive Filesystem Notes

Linux filesystems are case-sensitive by default. This affects:

- **MSU audio files** - Must match exact capitalization in paths
- **Shader files** - Must match exact capitalization in paths
- **Configuration** - Config validation warns about case mismatches at startup

**Example:**
```
✗ msu/track-01.ogg  (config says track-01.ogg but file is Track-01.ogg)
✓ msu/Track-01.ogg  (matches file on disk)
```

The game uses `Platform_FindFileWithCaseInsensitivity()` to detect mismatches and provide helpful error messages.

## Renderer Support

Zelda3 on Linux supports multiple rendering backends:

- **SDL Software** - Always available, CPU-based
- **OpenGL** - Hardware-accelerated (automatically detected)
- **Vulkan 1.0** - Modern graphics API (automatically detected)

Configure renderer in `zelda3.ini`:
```ini
[Display]
OutputMethod = opengl  # or: sdl, vulkan
```

## Cleaning Build Directory

```bash
# Remove build directory completely
cd .. && rm -rf build

# Or clean and rebuild
cd build
cmake --build . --clean-first
```

## Parallel Builds

CMake uses all CPU cores by default with `-j$(nproc)`. To limit cores:

```bash
# Use 4 cores only
cmake --build . -j4

# Use single core (slower but less memory)
cmake --build . -j1
```

## IDE Integration

### Visual Studio Code

1. Install "CMake Tools" extension
2. Open project folder
3. Select kit (compiler)
4. Build with F7

### CLion

1. Open project (CLion auto-detects CMakeLists.txt)
2. Build → Build Project

## Troubleshooting

### "SDL2 not found"

**Issue:** CMake cannot find SDL2 development libraries.

**Solution:**
```bash
# Ubuntu/Debian
sudo apt install libsdl2-dev

# Fedora
sudo dnf install SDL2-devel

# Arch
sudo pacman -S sdl2
```

### "Opus not found"

**Issue:** CMake cannot find Opus audio codec library.

**Solution:**
```bash
# Ubuntu/Debian
sudo apt install libopus-dev

# Fedora
sudo dnf install opus-devel

# Arch
sudo pacman -S opus
```

### "zelda3_assets.dat not found"

**Issue:** Game assets not extracted or not in correct location.

**Solution:**
```bash
# Run asset extraction
python3 assets/restool.py --extract-from-rom

# Ensure zelda3_assets.dat is in same directory as zelda3 executable
cp zelda3_assets.dat build/
```

### "Invalid ROM file"

**Issue:** ROM file is wrong region or corrupted.

**Solution:**
```bash
# Verify ROM hash (should match USA version)
sha256sum zelda3.sfc
# Expected: 66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb
```

### Build Warnings

Pre-existing warnings in the codebase are normal and expected. To treat warnings as compilation errors:

```bash
cmake .. -DENABLE_WERROR=ON
```

### MSU Audio Files Not Loading

**Issue:** MSU audio paths in config don't match actual file capitalization.

**Solution:** Check startup warnings for case mismatches:
```
Warning: MSU audio path 'msu/track-01.ogg' not found (case mismatch?)
```

Rename files or fix paths in `zelda3.ini` to match exact capitalization.

### Shader Files Not Loading

**Issue:** Shader paths in config don't match actual file capitalization.

**Solution:** Use exact capitalization for shader paths:
```ini
[Display]
# Wrong on case-sensitive filesystem
Shader = Shaders/crt-lottes.glsl

# Correct
Shader = shaders/crt-lottes.glsl
```

## Advanced Usage

### Cross-Compilation

Build for different architectures:

```bash
# Example: Build for ARM64 on x86_64
cmake .. -DCMAKE_SYSTEM_PROCESSOR=arm64
```

### Custom SDL2 Location

If SDL2 is installed in non-standard location:

```bash
cmake .. -DSDL2_INCLUDE_DIR=/path/to/include -DSDL2_LIBRARY=/path/to/lib
```

### Static Linking

For portable executables (advanced):

```bash
cmake .. -DBUILD_SHARED_LIBS=OFF
```

## Performance Optimization

For maximum performance:

1. Use Release build type
2. Enable OpenGL or Vulkan renderer
3. Use native CPU optimization:
   ```bash
   cmake .. -DCMAKE_C_FLAGS="-march=native"
   ```

## See Also

- [Installation Guide](../installation.md) - Overview of all platforms
- [Troubleshooting](../troubleshooting.md) - Common issues and solutions
- [Usage Guide](../usage.md) - Running and configuring the game
- [BUILDING.md](/Users/carl/Repos/zelda3/BUILDING.md) - Complete build documentation
