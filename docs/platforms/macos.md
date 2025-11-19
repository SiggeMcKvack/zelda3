# Building Zelda3 on macOS

[← Back to Installation Guide](../installation.md)

This guide covers building Zelda3 on macOS for both Intel (x86_64) and Apple Silicon (ARM64) Macs.

## Prerequisites

- **Python 3.x** with pip
- **SDL2** development libraries
- **Opus** audio codec library
- **CMake 3.10+**
- **Xcode Command Line Tools**

## Installing Xcode Command Line Tools

```bash
xcode-select --install
```

## Installing Dependencies with Homebrew

[Homebrew](https://brew.sh/) is the recommended package manager for macOS.

```bash
# Install Homebrew (if not already installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install sdl2 opus cmake python3

# Install Python requirements
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
cmake --build . -j$(sysctl -n hw.ncpu)
./zelda3
```

### Build Types

**Release Build (optimized, default):**
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(sysctl -n hw.ncpu)
```

**Debug Build (symbols, no optimization):**
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j$(sysctl -n hw.ncpu)
```

### Compiler Selection

**Use Clang (default on macOS):**
```bash
cmake .. -DCMAKE_C_COMPILER=clang
```

**Use GCC (if installed via Homebrew):**
```bash
cmake .. -DCMAKE_C_COMPILER=gcc-13
```

### CMake Options

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | Release, Debug | Release | Build optimization level |
| `ENABLE_WERROR` | ON, OFF | OFF | Treat warnings as errors |
| `CMAKE_C_COMPILER` | clang, gcc, etc | clang | C compiler to use |

**Enable warnings as errors:**
```bash
cmake .. -DENABLE_WERROR=ON
```

## Apple Silicon Notes

Zelda3 builds natively on Apple Silicon (M1/M2/M3) Macs:

- Homebrew installs ARM64 libraries by default
- CMake auto-detects ARM64 architecture
- No special configuration needed
- Performance is excellent on Apple Silicon

**Universal Binary (Intel + ARM64):**
```bash
# Build for both architectures (advanced)
cmake .. -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
```

## Vulkan Support with MoltenVK

Zelda3 supports Vulkan 1.0 rendering via MoltenVK on macOS.

**Install MoltenVK via Homebrew:**
```bash
brew install molten-vk
```

**Configure Vulkan renderer:**
Edit `zelda3.ini`:
```ini
[Display]
OutputMethod = vulkan
```

**MoltenVK Requirements:**
- Automatically detects MoltenVK at runtime
- Enables `VK_KHR_portability_enumeration` extension
- Enables `VK_KHR_portability_subset` extension
- Requires macOS 10.13+ (High Sierra or later)

## Installation

```bash
# Install to /usr/local/bin (requires sudo)
cd build
sudo cmake --install .

# Install to custom prefix (e.g., ~/Applications)
cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/Applications
cmake --install .
```

## Renderer Support

Zelda3 on macOS supports multiple rendering backends:

- **SDL Software** - Always available, CPU-based
- **OpenGL** - Hardware-accelerated (automatically detected)
- **Vulkan 1.0** - Via MoltenVK (requires installation)

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

## IDE Integration

### Visual Studio Code

1. Install "CMake Tools" extension
2. Open project folder
3. Select kit (compiler)
4. Build with F7

### Xcode

Generate Xcode project:
```bash
cmake .. -G Xcode
open zelda3.xcodeproj
```

Build in Xcode with Cmd+B.

### CLion

1. Open project (CLion auto-detects CMakeLists.txt)
2. Build → Build Project

## Troubleshooting

### "SDL2 not found"

**Issue:** CMake cannot find SDL2 development libraries.

**Solution:**
```bash
brew install sdl2
```

If still not found, specify path manually:
```bash
cmake .. -DSDL2_INCLUDE_DIR=/opt/homebrew/include -DSDL2_LIBRARY=/opt/homebrew/lib/libSDL2.dylib
```

### "Opus not found"

**Issue:** CMake cannot find Opus audio codec library.

**Solution:**
```bash
brew install opus
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
shasum -a 256 zelda3.sfc
# Expected: 66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb
```

### Permission Denied / Quarantine Issues

**Issue:** macOS blocks the binary from running due to Gatekeeper quarantine.

**Symptom:**
```
"zelda3" cannot be opened because the developer cannot be verified.
```

**Solution:**
```bash
# Remove quarantine attribute
xattr -d com.apple.quarantine build/zelda3

# Or disable quarantine for entire directory
xattr -dr com.apple.quarantine build/
```

**Alternative:** Right-click the binary in Finder, select "Open", then click "Open" in the security dialog.

### Code Signing Issues

**Issue:** Binary not signed, causing security warnings.

**Solution for Distribution:**
```bash
# Sign with ad-hoc signature (local use only)
codesign -s - build/zelda3

# Or sign with Developer ID (for distribution)
codesign -s "Developer ID Application: Your Name" build/zelda3
```

### Homebrew Architecture Mismatch

**Issue:** Installed wrong architecture libraries (Intel vs ARM64).

**Solution:**
```bash
# Check Homebrew architecture
brew config

# Apple Silicon should show: /opt/homebrew
# Intel should show: /usr/local

# Reinstall dependencies with correct architecture
brew reinstall sdl2 opus
```

### Build Warnings

Pre-existing warnings in the codebase are normal and expected. To treat warnings as compilation errors:

```bash
cmake .. -DENABLE_WERROR=ON
```

### MoltenVK Not Found

**Issue:** Vulkan renderer fails to initialize.

**Solution:**
```bash
# Install MoltenVK
brew install molten-vk

# Check installation
ls /opt/homebrew/lib/libMoltenVK.dylib  # ARM64
ls /usr/local/lib/libMoltenVK.dylib     # Intel

# Fallback to OpenGL if needed
# Edit zelda3.ini: OutputMethod = opengl
```

## Advanced Usage

### Universal Binary Build

Build for both Intel and Apple Silicon:

```bash
cmake .. -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
cmake --build . -j$(sysctl -n hw.ncpu)

# Verify architectures
file build/zelda3
# Output: Mach-O universal binary with 2 architectures: [x86_64:...] [arm64:...]
```

### Custom SDL2 Location

If SDL2 is installed in non-standard location:

```bash
cmake .. -DSDL2_INCLUDE_DIR=/path/to/include -DSDL2_LIBRARY=/path/to/lib/libSDL2.dylib
```

### Deployment Target

Set minimum macOS version:

```bash
cmake .. -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13
```

### Framework vs. Library Linking

CMake automatically detects whether SDL2 is installed as framework or library. To force framework:

```bash
cmake .. -DSDL2_LIBRARY=/Library/Frameworks/SDL2.framework
```

## Performance Optimization

For maximum performance:

1. Use Release build type
2. Enable OpenGL or Vulkan renderer
3. Use native CPU optimization:
   ```bash
   cmake .. -DCMAKE_C_FLAGS="-march=native"
   ```
4. On Apple Silicon, ensure native ARM64 build (not Rosetta)

## See Also

- [Installation Guide](../installation.md) - Overview of all platforms
- [Troubleshooting](../troubleshooting.md) - Common issues and solutions
- [Usage Guide](../usage.md) - Running and configuring the game
- [BUILDING.md](/Users/carl/Repos/zelda3/BUILDING.md) - Complete build documentation
