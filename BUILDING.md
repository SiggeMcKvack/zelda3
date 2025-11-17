# Building Zelda3

Complete build instructions for all supported platforms.

## Prerequisites

### All Platforms
- **Python 3.x** with pip
- **SDL2** development libraries
- **Opus** audio codec library
- **CMake 3.10+**

### Platform-Specific Dependencies

**Linux (Ubuntu/Debian):**
```bash
sudo apt install libsdl2-dev libopus-dev cmake build-essential python3 python3-pip
python3 -m pip install -r requirements.txt
```

**Linux (Fedora):**
```bash
sudo dnf install SDL2-devel opus-devel cmake gcc python3 python3-pip
python3 -m pip install -r requirements.txt
```

**Linux (Arch):**
```bash
sudo pacman -S sdl2 opus cmake gcc python python-pip
python3 -m pip install -r requirements.txt
```

**macOS:**
```bash
brew install sdl2 opus cmake
python3 -m pip install -r requirements.txt
```

**Windows (vcpkg):**
```bash
vcpkg install sdl2:x64-windows opus:x64-windows
# Then build with CMake using vcpkg toolchain file
```
See below for detailed Windows build instructions.

## Asset Extraction (Required First Step)

You need a US region Zelda 3 ROM file.

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

### Build Configuration

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
| `CMAKE_BUILD_TYPE` | Release, Debug | Release | Build optimization level |
| `ENABLE_WERROR` | ON, OFF | OFF | Treat warnings as errors |
| `CMAKE_C_COMPILER` | clang, gcc, etc | System default | C compiler to use |

### Automatic Dependency Detection

CMake automatically detects:
- **SDL2** (required) - Game framework
- **OpenGL** (optional) - Hardware-accelerated rendering
- **Vulkan** (optional) - Modern graphics API (future support)

If a dependency is missing, CMake will report it during configuration.

### Installation

```bash
# Install to /usr/local/bin (requires sudo)
sudo cmake --install .

# Install to custom prefix
cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --install .
```

### Cleaning

```bash
# Clean build directory
cd .. && rm -rf build

# Or rebuild from scratch
cd build
cmake --build . --clean-first
```

## Platform-Specific Builds

### Nintendo Switch

Requires [DevKitPro](https://devkitpro.org/wiki/Getting_Started) and [Atmosphere](https://github.com/Atmosphere-NX/Atmosphere).

```bash
# Install DevKitPro tools
(dkp-)pacman -S git switch-dev switch-sdl2 switch-tools

# Build
cd src/platform/switch
make -j$(nproc)

# Deploy to Switch (optional)
nxlink -s zelda3.nro
```

**Note:** Switch uses its own Makefile (devkitPro toolchain), not CMake.

## Troubleshooting

### "SDL2 not found"
- **Linux:** Install `libsdl2-dev` or `SDL2-devel`
- **macOS:** Run `brew install sdl2`
- **Windows:** Download SDL2 development libraries

### "zelda3_assets.dat not found"
Run asset extraction (see above). The `.dat` file must be in the same directory as the executable.

### "Invalid ROM file"
Ensure you have the USA region ROM with correct SHA256 hash. Use `sha256sum zelda3.sfc` to verify.

### Build Warnings
Pre-existing warnings in the codebase are normal. To treat them as errors, use `-DENABLE_WERROR=ON`.

### Permission Denied (macOS)
After building, macOS may quarantine the binary. Run:
```bash
xattr -d com.apple.quarantine zelda3
```

## Advanced Usage

### Cross-Compilation
```bash
# Example: Build for ARM64 on x86_64
cmake .. -DCMAKE_SYSTEM_PROCESSOR=arm64
```

### Custom SDL2 Location
```bash
cmake .. -DSDL2_INCLUDE_DIR=/path/to/include -DSDL2_LIBRARY=/path/to/lib
```

### Parallel Builds
CMake uses all cores by default with `-j$(nproc)`. Limit cores:
```bash
cmake --build . -j4  # Use 4 cores
```

## IDE Integration

CMake generates project files for popular IDEs:

**Visual Studio Code:**
- Install "CMake Tools" extension
- Open project folder
- Select kit (compiler)
- Build with F7

**CLion:**
- Open project (CLion detects CMakeLists.txt)
- Build → Build Project

**Visual Studio:**
```bash
cmake .. -G "Visual Studio 17 2022"
```

**Xcode:**
```bash
cmake .. -G Xcode
open zelda3.xcodeproj
```

## CI/CD

Example GitHub Actions workflow:
```yaml
- name: Install Dependencies
  run: sudo apt install libsdl2-dev cmake

- name: Extract Assets
  run: python3 assets/restool.py --extract-from-rom -r zelda3.sfc

- name: Build
  run: |
    mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build . -j$(nproc)
```
