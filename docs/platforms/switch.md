# Building Zelda3 on Nintendo Switch

[← Back to Installation Guide](../installation.md)

This guide covers building and deploying Zelda3 on Nintendo Switch using DevKitPro and Atmosphere custom firmware.

## Prerequisites

- **Nintendo Switch** with custom firmware (Atmosphere)
- **DevKitPro** toolchain with Switch support
- **Python 3.x** with pip (for asset extraction)
- **SD card** for file transfer

## Important Legal Notice

Building homebrew for Nintendo Switch requires:
- A Switch console capable of running custom firmware
- Legal ownership of the console
- Understanding that installing custom firmware may void warranty

This guide assumes you have a legally obtained Switch console and ROM file.

## Installing DevKitPro

DevKitPro provides the toolchain for Switch development.

### Linux/macOS

```bash
# Download DevKitPro Pac-Man package manager
wget https://github.com/devkitPro/pacman/releases/latest/download/devkitpro-pacman.pkg.tar.xz

# Extract and install (method varies by distro)
# Debian/Ubuntu
sudo dpkg -i devkitpro-pacman.deb

# Arch Linux
sudo pacman -U devkitpro-pacman.pkg.tar.xz

# macOS
sudo installer -pkg devkitpro-pacman.pkg -target /
```

### Windows

1. Download installer from [devkitpro.org/wiki/Getting_Started](https://devkitpro.org/wiki/Getting_Started)
2. Run installer
3. Select "Switch Development" components
4. Complete installation

### Install Switch Development Tools

```bash
# Install required packages
sudo dkp-pacman -S switch-dev switch-sdl2 switch-tools

# Verify installation
which aarch64-none-elf-gcc
# Should output: /opt/devkitpro/devkitA64/bin/aarch64-none-elf-gcc
```

### Set Environment Variables

Add to your shell profile (`~/.bashrc`, `~/.zshrc`, etc.):

```bash
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=/opt/devkitpro/devkitARM
export DEVKITPPC=/opt/devkitpro/devkitPPC
export PATH=$DEVKITPRO/tools/bin:$PATH
```

**Windows:** Environment variables are set automatically by installer.

## Installing Atmosphere

Atmosphere is custom firmware that enables homebrew on Switch.

1. Download latest [Atmosphere release](https://github.com/Atmosphere-NX/Atmosphere/releases)
2. Extract to SD card root
3. Insert SD card into Switch
4. Boot Switch into custom firmware (method varies by exploit)

**Resources:**
- [Atmosphere Setup Guide](https://nh-server.github.io/switch-guide/)
- [RCM Boot Guide](https://switch.homebrew.guide/)

**Note:** This guide does not cover initial CFW setup. Consult community guides.

## Asset Extraction

Before building, extract game assets from a USA region ROM.

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

## Building with Make

**Note:** Switch uses Make (not CMake) due to DevKitPro toolchain requirements.

### Build NRO File

```bash
# Navigate to Switch platform directory
cd src/platform/switch

# Build
make -j$(nproc)

# Output: zelda3.nro
```

**Windows:**
```cmd
cd src\platform\switch
make -j%NUMBER_OF_PROCESSORS%
```

### Build Configuration

Edit `src/platform/switch/Makefile` to configure:

```makefile
# Application info
APP_TITLE   := Zelda3
APP_AUTHOR  := snesrev
APP_VERSION := 1.0.0

# Build options
ARCH    := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS  := -g -Wall -O2 -ffunction-sections $(ARCH)
```

### Clean Build

```bash
# Clean object files and rebuild
cd src/platform/switch
make clean
make -j$(nproc)
```

## Deploying to Switch

### Method 1: SD Card Transfer

```bash
# Copy NRO to Switch SD card
cp src/platform/switch/zelda3.nro /path/to/sdcard/switch/zelda3/

# Copy assets
cp zelda3_assets.dat /path/to/sdcard/switch/zelda3/

# Eject SD card and insert into Switch
```

**Directory structure on SD card:**
```
/switch/zelda3/
├── zelda3.nro
├── zelda3_assets.dat
└── zelda3.ini (optional)
```

### Method 2: NXLink (Development)

Transfer over network (requires Switch running homebrew menu with network enabled):

```bash
# From build directory
cd src/platform/switch
nxlink -s zelda3.nro

# Or specify IP address
nxlink -a 192.168.1.XXX -s zelda3.nro
```

**Prerequisites:**
- Switch and PC on same network
- Homebrew menu running on Switch with network enabled

## Running on Switch

1. Boot Switch into custom firmware (Atmosphere)
2. Launch Homebrew Menu (hold R while launching a game/album)
3. Navigate to Zelda3
4. Launch app

**Controls:**
- Default controller mapping matches SNES layout
- Customize in `zelda3.ini` if needed

## Configuration

### Edit zelda3.ini on Switch

Create or edit `/switch/zelda3/zelda3.ini` on SD card:

```ini
[Display]
OutputMethod = sdl  # Switch uses SDL renderer
WindowScale = 1     # Fullscreen on Switch

[Controls]
# Customize button mapping
# (See zelda3.ini template for options)

[Audio]
AudioSamples = 1024
AudioFreq = 48000
```

### Transfer Config to Switch

```bash
# Edit locally
cp zelda3.ini /path/to/sdcard/switch/zelda3/

# Or create on PC and transfer
```

## Troubleshooting

### "devkitA64 not found"

**Issue:** DevKitPro toolchain not installed or not in PATH.

**Solution:**
```bash
# Install Switch development tools
sudo dkp-pacman -S switch-dev

# Set environment variables
export DEVKITPRO=/opt/devkitpro
export PATH=$DEVKITPRO/tools/bin:$PATH
```

### "SDL not found"

**Issue:** SDL2 library for Switch not installed.

**Solution:**
```bash
sudo dkp-pacman -S switch-sdl2
```

### "zelda3_assets.dat not found"

**Issue:** Assets not on SD card or in wrong location.

**Solution:**
```bash
# Ensure assets are in correct directory on SD card
# /switch/zelda3/zelda3_assets.dat

# Copy to SD card
cp zelda3_assets.dat /path/to/sdcard/switch/zelda3/
```

### "Invalid ROM file"

**Issue:** ROM file is wrong region or corrupted.

**Solution:**
```bash
# Verify ROM hash (should match USA version)
sha256sum zelda3.sfc
# Expected: 66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb
```

### Build Errors: "undefined reference"

**Issue:** Missing libraries or incorrect linking.

**Solution:**
```bash
# Clean and rebuild
cd src/platform/switch
make clean
make -j$(nproc)

# Verify DevKitPro packages are installed
dkp-pacman -Q | grep switch
```

### App Won't Launch on Switch

**Issue:** NRO file corrupted or incompatible.

**Symptoms:**
- App crashes immediately
- "Unable to start software" error
- Black screen

**Solution:**
1. Rebuild with `make clean && make`
2. Verify file copied correctly to SD card
3. Check Atmosphere is up to date
4. Try different SD card (test for corruption)

### Performance Issues

**Issue:** Game runs slowly or stutters.

**Solution:**
- Ensure you're using release build (not debug)
- Close other homebrew apps
- Check SD card speed (use Class 10 or UHS-I)
- Update Atmosphere to latest version

### Audio Crackling

**Issue:** Audio distortion or crackling.

**Solution:**
Edit `zelda3.ini`:
```ini
[Audio]
AudioSamples = 2048  # Higher = smoother but more latency
AudioFreq = 48000    # Match Switch audio rate
```

### Controls Not Working

**Issue:** Controller input not recognized.

**Solution:**
- Test with different controller (Joy-Cons vs. Pro Controller)
- Check controller battery
- Recalibrate controllers in Switch settings
- Customize mapping in `zelda3.ini`

## Advanced Configuration

### Custom Icon and Metadata

Edit `src/platform/switch/Makefile`:

```makefile
APP_TITLE   := Zelda3
APP_AUTHOR  := snesrev
APP_VERSION := 1.0.0

# Custom icon (PNG, 256x256)
ICON := icon.png
```

### Optimization Flags

For better performance, edit Makefile:

```makefile
CFLAGS := -g -Wall -O3 -ffast-math -ffunction-sections $(ARCH)
```

### Debug Logging

Enable debug output to Switch console:

```bash
# Build debug version
cd src/platform/switch
make DEBUG=1

# View logs via nxlink
nxlink -s zelda3.nro
# Logs will appear in terminal
```

## Build System Details

**Switch build system:**
- Uses DevKitPro Make-based toolchain
- Independent from desktop CMake build
- Outputs `.nro` files (Switch homebrew format)
- Cross-compiles for ARM64 (Cortex-A57)

**Source files:**
- Platform code: `src/platform/switch/`
- Core game logic: `src/` (shared with desktop)
- Makefile: `src/platform/switch/Makefile`

**Build process:**
1. DevKitA64 compiles C code to ARM64
2. Links with Switch SDL2 and homebrew libraries
3. Packages as NRO file with metadata
4. Ready for deployment to Switch

## Performance Notes

- Switch runs at 1920x1080 docked, 1280x720 handheld
- 60 FPS target (matches original SNES)
- SDL software renderer (no hardware acceleration yet)
- Battery life: ~3-4 hours handheld

## File Transfer Tips

### SD Card Access

- Use SD card reader on PC for fastest transfers
- Or use FTP homebrew app on Switch for wireless transfer
- Ensure SD card is FAT32 formatted

### Backup Saves

Game saves are stored in same directory as NRO:
```
/switch/zelda3/save_*.sram
```

Backup regularly by copying to PC.

## Community Resources

- [DevKitPro Documentation](https://devkitpro.org/wiki/Getting_Started)
- [Atmosphere Docs](https://github.com/Atmosphere-NX/Atmosphere/tree/master/docs)
- [Switch Homebrew Discord](https://discord.gg/switchbrew)
- [Zelda3 Project Discord](https://discord.gg/AJJbJAzNNJ)

## Legal Considerations

- Homebrew development is legal
- Custom firmware is legal (in most jurisdictions)
- Piracy is illegal - use legally obtained ROMs
- Online play with CFW may result in console ban
- Use emuMMC/emuNAND to protect online access

## See Also

- [Installation Guide](../installation.md) - Overview of all platforms
- [Troubleshooting](../troubleshooting.md) - Common issues and solutions
- [Usage Guide](../usage.md) - Running and configuring the game
- [BUILDING.md](/Users/carl/Repos/zelda3/BUILDING.md) - Complete build documentation
