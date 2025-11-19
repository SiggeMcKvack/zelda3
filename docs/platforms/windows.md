# Building Zelda3 on Windows

[← Back to Installation Guide](../installation.md)

This guide covers building Zelda3 on Windows using Visual Studio and vcpkg.

## Prerequisites

- **Python 3.x** with pip
- **Visual Studio 2019 or later** (Community Edition is free)
- **CMake 3.10+**
- **vcpkg** (for dependency management)

## Installing Visual Studio

1. Download [Visual Studio Community](https://visualstudio.microsoft.com/downloads/)
2. During installation, select:
   - "Desktop development with C++"
   - "C++ CMake tools for Windows"
3. Install and restart your computer

## Installing CMake

CMake is included with Visual Studio, but you can also install standalone:

1. Download from [cmake.org/download](https://cmake.org/download/)
2. Run installer and select "Add CMake to system PATH"
3. Verify installation:
   ```cmd
   cmake --version
   ```

## Installing vcpkg

vcpkg is Microsoft's C++ package manager. It simplifies installing SDL2 and Opus.

```powershell
# Clone vcpkg
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

# Bootstrap vcpkg
.\bootstrap-vcpkg.bat

# Add to PATH (optional, for convenience)
setx PATH "%PATH%;C:\vcpkg"
```

## Installing Dependencies

```powershell
# Navigate to vcpkg directory
cd C:\vcpkg

# Install SDL2 and Opus (64-bit)
.\vcpkg install sdl2:x64-windows opus:x64-windows

# Install Python requirements
python -m pip install -r requirements.txt
```

**Note:** Installation may take 10-15 minutes as vcpkg builds from source.

## Asset Extraction

Before building, you must extract game assets from a USA region ROM.

**Required ROM:**
- Filename: `zelda3.sfc`
- Region: USA
- SHA256: `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`

**Extract assets:**
```cmd
REM Place zelda3.sfc in project root, then:
python assets\restool.py --extract-from-rom

REM Or specify ROM path:
python assets\restool.py --extract-from-rom -r C:\path\to\zelda3.sfc
```

This creates `zelda3_assets.dat` (~2MB) containing all game graphics, text, and maps.

## Building with CMake

### Command Line Build

```cmd
REM Create build directory
mkdir build
cd build

REM Configure with vcpkg toolchain
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake

REM Build (Release)
cmake --build . --config Release

REM Run
Release\zelda3.exe
```

### Visual Studio Build

```cmd
REM Generate Visual Studio solution
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake

REM Open solution in Visual Studio
start zelda3.sln
```

Then in Visual Studio:
1. Select "Release" or "Debug" configuration
2. Build → Build Solution (F7)
3. Run → Start Without Debugging (Ctrl+F5)

### Build Types

**Release Build (optimized, default):**
```cmd
cmake --build . --config Release
```

**Debug Build (symbols, no optimization):**
```cmd
cmake --build . --config Debug
```

### CMake Options

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | Release, Debug | Release | Build optimization level |
| `ENABLE_WERROR` | ON, OFF | OFF | Treat warnings as errors |
| `CMAKE_TOOLCHAIN_FILE` | Path | - | vcpkg toolchain file path |

**Enable warnings as errors:**
```cmd
cmake .. -DENABLE_WERROR=ON -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

## DLL Management

After building, you need SDL2 DLLs in the same directory as the executable.

### Automatic Method (vcpkg)

vcpkg automatically copies DLLs to the build output directory:

```
build/Release/
├── zelda3.exe
├── SDL2.dll          (copied by vcpkg)
├── opus.dll          (copied by vcpkg)
└── zelda3_assets.dat (copy manually)
```

### Manual Method

If DLLs are missing, copy them manually:

```cmd
REM Copy SDL2 DLL
copy C:\vcpkg\installed\x64-windows\bin\SDL2.dll build\Release\

REM Copy Opus DLL
copy C:\vcpkg\installed\x64-windows\bin\opus.dll build\Release\

REM Copy assets
copy zelda3_assets.dat build\Release\
```

## Renderer Support

Zelda3 on Windows supports multiple rendering backends:

- **SDL Software** - Always available, CPU-based
- **OpenGL** - Hardware-accelerated (automatically detected)
- **Vulkan 1.0** - Modern graphics API (requires Vulkan drivers)

Configure renderer in `zelda3.ini`:
```ini
[Display]
OutputMethod = opengl  # or: sdl, vulkan
```

**Vulkan Requirements:**
- Graphics drivers with Vulkan support (NVIDIA, AMD, Intel)
- Vulkan SDK not required for running (only for development)

## Installation

```cmd
REM Install to C:\Program Files\Zelda3 (requires admin)
cd build
cmake --install . --config Release

REM Install to custom directory
cmake --install . --config Release --prefix C:\Games\Zelda3
```

## Cleaning Build Directory

```cmd
REM Remove build directory completely
rmdir /s /q build

REM Or clean and rebuild
cd build
cmake --build . --clean-first --config Release
```

## IDE Integration

### Visual Studio Code

1. Install "CMake Tools" extension
2. Install "C/C++" extension
3. Open project folder
4. Configure CMake with vcpkg toolchain:
   - Open settings (Ctrl+,)
   - Search "CMake: Configure Args"
   - Add: `-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake`
5. Build with F7

### Visual Studio

See "Visual Studio Build" section above.

### CLion

1. Open project (CLion auto-detects CMakeLists.txt)
2. Configure vcpkg toolchain in Settings → Build → CMake:
   - CMake options: `-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake`
3. Build → Build Project

## Troubleshooting

### "SDL2 not found"

**Issue:** CMake cannot find SDL2.

**Solution:**
```powershell
# Install SDL2 via vcpkg
cd C:\vcpkg
.\vcpkg install sdl2:x64-windows

# Ensure you use vcpkg toolchain file
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

### "Opus not found"

**Issue:** CMake cannot find Opus.

**Solution:**
```powershell
# Install Opus via vcpkg
cd C:\vcpkg
.\vcpkg install opus:x64-windows
```

### "zelda3_assets.dat not found"

**Issue:** Game assets not extracted or not in correct location.

**Solution:**
```cmd
REM Run asset extraction
python assets\restool.py --extract-from-rom

REM Copy to executable directory
copy zelda3_assets.dat build\Release\
copy zelda3_assets.dat build\Debug\
```

### "Invalid ROM file"

**Issue:** ROM file is wrong region or corrupted.

**Solution:**
```powershell
# Verify ROM hash (should match USA version)
certutil -hashfile zelda3.sfc SHA256
# Expected: 66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb
```

### "SDL2.dll not found" or "opus.dll not found"

**Issue:** DLL not in same directory as executable.

**Solution:**
```cmd
REM Copy DLLs to executable directory
copy C:\vcpkg\installed\x64-windows\bin\SDL2.dll build\Release\
copy C:\vcpkg\installed\x64-windows\bin\opus.dll build\Release\
```

**Or add to PATH:**
```cmd
setx PATH "%PATH%;C:\vcpkg\installed\x64-windows\bin"
```

### Build Warnings

Pre-existing warnings in the codebase are normal and expected. To treat warnings as compilation errors:

```cmd
cmake .. -DENABLE_WERROR=ON -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

### "Cannot open include file: 'SDL.h'"

**Issue:** SDL2 headers not found by compiler.

**Solution:**
```cmd
REM Ensure vcpkg toolchain is used
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake

REM Or manually specify SDL2 paths
cmake .. -DSDL2_INCLUDE_DIR=C:\vcpkg\installed\x64-windows\include ^
         -DSDL2_LIBRARY=C:\vcpkg\installed\x64-windows\lib\SDL2.lib
```

### vcpkg Integration Issues

**Issue:** vcpkg packages not found even after installation.

**Solution:**
```powershell
# Integrate vcpkg with Visual Studio (run once)
cd C:\vcpkg
.\vcpkg integrate install

# Verify installation
.\vcpkg list
# Should show: sdl2:x64-windows and opus:x64-windows
```

### 32-bit vs 64-bit Mismatch

**Issue:** Building 64-bit but have 32-bit libraries (or vice versa).

**Solution:**
```powershell
# For 64-bit (recommended)
.\vcpkg install sdl2:x64-windows opus:x64-windows
cmake .. -A x64 -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake

# For 32-bit
.\vcpkg install sdl2:x86-windows opus:x86-windows
cmake .. -A Win32 -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

### Python Not Found

**Issue:** `python` command not recognized.

**Solution:**
```cmd
REM Check if Python is installed
python --version

REM If not found, download from python.org and install
REM During installation, check "Add Python to PATH"
```

## Advanced Usage

### Static Linking

To create standalone executable without DLL dependencies:

```cmd
REM Install static libraries
cd C:\vcpkg
.\vcpkg install sdl2:x64-windows-static opus:x64-windows-static

REM Configure for static linking
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ^
         -DVCPKG_TARGET_TRIPLET=x64-windows-static
```

### Custom SDL2 Location

If not using vcpkg:

```cmd
cmake .. -DSDL2_INCLUDE_DIR=C:\SDL2\include ^
         -DSDL2_LIBRARY=C:\SDL2\lib\x64\SDL2.lib
```

### Parallel Builds

Speed up compilation by using multiple cores:

```cmd
REM Build with 8 cores
cmake --build . --config Release -j 8

REM Or use all available cores
cmake --build . --config Release -j %NUMBER_OF_PROCESSORS%
```

### Cross-Compilation

Build for different architectures from same machine:

```cmd
REM Build for ARM64 (Windows 11 on ARM)
cmake .. -A ARM64 -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

## Performance Optimization

For maximum performance:

1. Use Release build configuration
2. Enable OpenGL or Vulkan renderer
3. Build with Visual Studio (better optimizations than GCC/Clang on Windows)
4. Enable compiler optimizations:
   ```cmd
   cmake .. -DCMAKE_C_FLAGS="/O2 /arch:AVX2"
   ```

## Creating Redistributable Package

To distribute Zelda3 to other users:

```cmd
REM Create distribution directory
mkdir dist
cd dist

REM Copy executable
copy ..\build\Release\zelda3.exe .

REM Copy DLLs
copy C:\vcpkg\installed\x64-windows\bin\SDL2.dll .
copy C:\vcpkg\installed\x64-windows\bin\opus.dll .

REM Copy assets (user must provide their own)
echo "Place zelda3_assets.dat here" > README.txt

REM Copy config template
copy ..\zelda3.ini .

REM Package as ZIP
REM Users can now run zelda3.exe after adding their zelda3_assets.dat
```

## See Also

- [Installation Guide](../installation.md) - Overview of all platforms
- [Troubleshooting](../troubleshooting.md) - Common issues and solutions
- [Usage Guide](../usage.md) - Running and configuring the game
- [BUILDING.md](/Users/carl/Repos/zelda3/BUILDING.md) - Complete build documentation
