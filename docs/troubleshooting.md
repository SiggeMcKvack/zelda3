# Troubleshooting

[← Back to Documentation Index](index.md)

Solutions to common build and runtime issues.

## Build Issues

### SDL2 Not Found

**Symptoms:**
```
CMake Error: Could not find SDL2
```

**Solutions:**

**Linux:**
```bash
# Ubuntu/Debian
sudo apt install libsdl2-dev

# Fedora
sudo dnf install SDL2-devel

# Arch
sudo pacman -S sdl2
```

**macOS:**
```bash
brew install sdl2
```

**Windows:**
- Download SDL2 development libraries from https://www.libsdl.org/
- Or use vcpkg: `vcpkg install sdl2:x64-windows`
- See [Windows Build Guide](platforms/windows.md) for details

### Opus Not Found

**Symptoms:**
```
CMake Error: Could not find Opus
```

**Solutions:**

**Linux:**
```bash
# Ubuntu/Debian
sudo apt install libopus-dev

# Fedora
sudo dnf install opus-devel

# Arch
sudo pacman -S opus
```

**macOS:**
```bash
brew install opus
```

**Windows:**
```bash
vcpkg install opus:x64-windows
```

### CMake Version Too Old

**Symptoms:**
```
CMake 3.10 or higher is required. You are running version X.X.X
```

**Solutions:**

**Linux:**
```bash
# Ubuntu (add Kitware APT repository for latest CMake)
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc | sudo apt-key add -
sudo apt-add-repository 'deb https://apt.kitware.com/ubuntu/ focal main'
sudo apt update && sudo apt install cmake

# Or use pip
python3 -m pip install --upgrade cmake
```

**macOS:**
```bash
brew upgrade cmake
```

**Windows:**
Download latest CMake installer from https://cmake.org/download/

### Compiler Not Found

**Symptoms:**
```
No CMAKE_C_COMPILER could be found
```

**Solutions:**

**Linux:**
```bash
# Ubuntu/Debian
sudo apt install build-essential

# Fedora
sudo dnf install gcc

# Arch
sudo pacman -S base-devel
```

**macOS:**
```bash
xcode-select --install
```

**Windows:**
- Install Visual Studio 2019 or later (Community Edition is free)
- Or install MinGW-w64

## Asset Issues

### zelda3_assets.dat Not Found

**Symptoms:**
```
Error: Could not open zelda3_assets.dat
```

**Solutions:**

1. **Extract assets from ROM:**
   ```bash
   python3 -m pip install -r requirements.txt
   python3 assets/restool.py --extract-from-rom
   ```

2. **Verify asset file exists:**
   ```bash
   ls -lh zelda3_assets.dat
   ```

3. **Ensure asset file is in the same directory as executable:**
   ```bash
   cp zelda3_assets.dat build/
   ```

### Invalid ROM File

**Symptoms:**
```
Error: ROM file has incorrect hash
```

**Solutions:**

1. **Verify you have the US region ROM:**
   ```bash
   sha256sum zelda3.sfc
   ```
   Expected: `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`

2. **Ensure ROM is named correctly:**
   - Must be named `zelda3.sfc`
   - Must be US region (not Japan or Europe)

3. **Try re-downloading the ROM from a verified source**

### Python Dependencies Missing

**Symptoms:**
```
ModuleNotFoundError: No module named 'PIL'
```

**Solutions:**

```bash
python3 -m pip install -r requirements.txt

# Or install manually
python3 -m pip install Pillow PyYAML
```

## Runtime Issues

### Game Crashes on Startup

**Solutions:**

1. **Verify assets exist:**
   ```bash
   ls -lh zelda3_assets.dat
   ```

2. **Check console output for error messages**

3. **Try software renderer:**
   Edit `zelda3.ini`:
   ```ini
   [Display]
   Renderer = SDL
   ```

4. **Verify SDL2 is properly installed:**
   ```bash
   # Linux
   ldconfig -p | grep SDL2

   # macOS
   brew list sdl2
   ```

### No Audio

**Symptoms:**
- Game runs but no sound

**Solutions:**

1. **Increase audio buffer size:**
   Edit `zelda3.ini`:
   ```ini
   [Audio]
   BufferSamples = 4096
   ```

2. **Test SDL audio driver:**
   ```bash
   # Linux - try different audio drivers
   SDL_AUDIODRIVER=pulse ./zelda3
   SDL_AUDIODRIVER=alsa ./zelda3
   SDL_AUDIODRIVER=pulseaudio ./zelda3
   ```

3. **Check system audio:**
   - Ensure speakers/headphones are connected
   - Verify system volume is not muted
   - Test with other applications

### Poor Performance / Low FPS

**Solutions:**

1. **Use hardware rendering:**
   Edit `zelda3.ini`:
   ```ini
   [Display]
   Renderer = OpenGL
   ```

2. **Lower window scale:**
   ```ini
   [Display]
   WindowScale = 1
   ```

3. **Close resource-intensive applications**

4. **Build with Release configuration:**
   ```bash
   cmake .. -DCMAKE_BUILD_TYPE=Release
   cmake --build .
   ```

5. **Check CPU usage:**
   - Debug builds are significantly slower
   - Verification mode (running with ROM path) is CPU-intensive

### Graphics Glitches / Artifacts

**Solutions:**

1. **Update graphics drivers**

2. **Try different renderer:**
   ```ini
   [Display]
   Renderer = SDL   # Or OpenGL, Vulkan
   ```

3. **Disable pixel shaders if enabled**

4. **Verify OpenGL/Vulkan support:**
   ```bash
   # Linux
   glxinfo | grep "OpenGL version"
   vulkaninfo | grep "Vulkan Instance Version"

   # macOS (requires MoltenVK for Vulkan)
   system_profiler SPDisplaysDataType
   ```

## Platform-Specific Issues

### Linux: Permission Denied

**Symptoms:**
```
bash: ./zelda3: Permission denied
```

**Solution:**
```bash
chmod +x zelda3
```

### Linux: Case-Sensitive Filesystem Warnings

**Symptoms:**
```
Warning: MSU audio path 'MSU/' not found (case mismatch?)
Did you mean: 'msu/'?
```

**Solution:**

Update `zelda3.ini` to match exact capitalization:
```ini
[Audio]
MSUPath = msu/   # Must match actual folder name
```

Linux filesystems are case-sensitive. Use the suggested path from the warning message.

### macOS: "zelda3 cannot be opened because the developer cannot be verified"

**Solution:**

**Option 1 (Recommended):**
```bash
xattr -d com.apple.quarantine zelda3
```

**Option 2:**
1. Right-click zelda3 executable
2. Select "Open"
3. Click "Open" in the security dialog

**Option 3:**
System Preferences → Security & Privacy → Allow "zelda3"

### macOS: Vulkan Not Available

**Symptoms:**
```
Vulkan renderer not available
```

**Solution:**

Install MoltenVK (Vulkan wrapper for Metal):
```bash
brew install molten-vk
```

Or use OpenGL renderer:
```ini
[Display]
Renderer = OpenGL
```

### Windows: Missing DLL Errors

**Symptoms:**
```
The code execution cannot proceed because SDL2.dll was not found
```

**Solutions:**

**Option 1 - Copy DLLs:**
```cmd
# Copy SDL2.dll and other DLLs to same directory as zelda3.exe
copy C:\path\to\SDL2.dll zelda3.exe_directory\
```

**Option 2 - Static Linking:**
Build with static libraries (requires rebuilding with specific CMake flags)

**Option 3 - vcpkg Integration:**
```bash
vcpkg integrate install
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### Windows: MSBuild Not Found

**Solution:**

Add MSBuild to PATH, or use Visual Studio Developer Command Prompt:
- Start Menu → Visual Studio 2022 → Developer Command Prompt
- Or: Tools → Command Line → Developer Command Prompt in Visual Studio

### Android: SDK Not Found

**Symptoms:**
```
Error: ANDROID_HOME environment variable not set
```

**Solution:**

```bash
# Linux/macOS
export ANDROID_HOME=$HOME/Android/Sdk
echo 'export ANDROID_HOME=$HOME/Android/Sdk' >> ~/.bashrc

# Windows (PowerShell)
$env:ANDROID_HOME = "C:\Users\YourName\AppData\Local\Android\Sdk"
setx ANDROID_HOME "C:\Users\YourName\AppData\Local\Android\Sdk"
```

### Android: NDK Not Found

**Solution:**

Install NDK via Android Studio SDK Manager:
1. Tools → SDK Manager
2. SDK Tools tab
3. Check "NDK (Side by side)"
4. Click "Apply"

Or set `ndk.dir` in `android/local.properties`:
```properties
ndk.dir=/path/to/android-sdk/ndk/25.2.9519653
```

### Android: Asset Loading Fails

**Solution:**

1. **Verify assets are in correct location:**
   ```bash
   adb shell ls /sdcard/Android/data/com.yourpackage.zelda3/files/zelda3_assets.dat
   ```

2. **Push assets to device:**
   ```bash
   adb push zelda3_assets.dat /sdcard/Android/data/com.yourpackage.zelda3/files/
   ```

3. **Check permissions:**
   Grant storage permissions in Android Settings → Apps → Zelda3 → Permissions

See [Android Build Guide](platforms/android.md) for more details.

### Switch: NRO Won't Launch

**Solutions:**

1. **Verify Atmosphere is installed and up-to-date**

2. **Check NRO is in correct location:**
   ```
   /switch/zelda3/zelda3.nro
   ```

3. **Ensure assets are alongside NRO:**
   ```
   /switch/zelda3/zelda3_assets.dat
   ```

4. **Check hbmenu logs for errors**

## Debug Build Issues

### Excessive Debug Output

**Symptoms:**
- Console spam with debug messages

**Solution:**

1. **Build in Release mode:**
   ```bash
   cmake .. -DCMAKE_BUILD_TYPE=Release
   ```

2. **Or set log level:**
   ```bash
   ZELDA3_LOG_LEVEL=INFO ./zelda3
   ```

### Debug Build is Very Slow

This is expected behavior. Debug builds:
- Have no optimization
- Include extensive logging
- Perform additional checks

**Solution:**
Use Release builds for normal play:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

## Getting More Help

If you're still experiencing issues:

1. **Check the [Wiki](https://github.com/snesrev/zelda3/wiki)** for additional troubleshooting

2. **Search [GitHub Issues](https://github.com/snesrev/zelda3/issues)** for similar problems

3. **Join the [Discord server](https://discord.gg/AJJbJAzNNJ)** for community support

4. **Create a new issue** on GitHub with:
   - Platform and OS version
   - Build configuration (Debug/Release)
   - Complete error messages
   - Steps to reproduce

---

[← Back to Documentation Index](index.md)
