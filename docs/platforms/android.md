# Building Zelda3 on Android

[← Back to Installation Guide](../installation.md)

This guide covers building and deploying Zelda3 on Android devices using Gradle and Android Studio.

**Attribution:** The Android port is based on the [Waterdish/zelda3-android](https://github.com/Waterdish/zelda3-android) fork with touchpad controls from the [Eden emulator](https://git.eden-emu.dev/eden-emu/eden) project.

## Prerequisites

- **Android SDK** (API level 26+)
- **Android NDK** (r21e or later)
- **Java 17+** (for Gradle 8.x)
- **Python 3.x** with pip (for asset extraction)
- `ANDROID_HOME` environment variable set

## Installing Android Studio

Android Studio is the recommended way to install SDK and NDK.

1. Download from [developer.android.com/studio](https://developer.android.com/studio)
2. Install Android Studio
3. Open SDK Manager (Tools → SDK Manager)
4. Install:
   - **SDK Platform**: Android 13.0 (API 33) or higher
   - **SDK Tools**: Android NDK (check "Show Package Details" for version)
5. Set `ANDROID_HOME` environment variable:

**Linux/macOS:**
```bash
export ANDROID_HOME=$HOME/Android/Sdk
# Add to ~/.bashrc or ~/.zshrc for persistence
```

**Windows:**
```cmd
setx ANDROID_HOME "%LOCALAPPDATA%\Android\Sdk"
```

## Command-Line Tools Installation (Alternative)

If not using Android Studio:

```bash
# Linux/macOS
wget https://dl.google.com/android/repository/commandlinetools-linux-9477386_latest.zip
unzip commandlinetools-*.zip -d $HOME/android-sdk
export ANDROID_HOME=$HOME/android-sdk

# Install platform and NDK
$ANDROID_HOME/cmdline-tools/bin/sdkmanager --sdk_root=$ANDROID_HOME \
  "platform-tools" "platforms;android-33" "ndk;25.2.9519653"
```

**Windows:** Download [Command Line Tools](https://developer.android.com/studio#command-tools) and follow similar steps.

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

## Building APK with Gradle

### Debug Build (Development)

```bash
cd android

# Build debug APK
./gradlew assembleDebug

# Output location:
# android/app/build/outputs/apk/debug/app-debug.apk
```

**On Windows:**
```cmd
cd android
gradlew.bat assembleDebug
```

### Release Build (Production)

```bash
cd android

# Build release APK (signed and optimized)
./gradlew assembleRelease

# Output location:
# android/app/build/outputs/apk/release/app-release.apk
```

**Note:** Release builds require signing configuration in `android/app/build.gradle` or via keystore.

### Build Configuration

Edit `android/app/build.gradle` to configure:

```gradle
android {
    compileSdk 33

    defaultConfig {
        applicationId "com.yourpackage.zelda3"  // Change package name
        minSdk 26      // Android 8.0+
        targetSdk 33   // Android 13+
        versionCode 1
        versionName "1.0"
    }

    ndkVersion "25.2.9519653"  // NDK version
}
```

## Installing APK to Device

### Enable USB Debugging

1. On Android device: Settings → About Phone
2. Tap "Build Number" 7 times to enable Developer Options
3. Settings → Developer Options → Enable "USB Debugging"
4. Connect device via USB
5. Authorize debugging when prompted

### Install via Gradle

```bash
# Connect device and verify
adb devices

# Install debug build
cd android
./gradlew installDebug

# Or manually install
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

### Install Release Build

```bash
adb install -r app/build/outputs/apk/release/app-release.apk
```

## Deploying Assets to Device

The game requires `zelda3_assets.dat` in app-specific storage.

**Package name:** `com.dishii.zelda3`

### Method 1: ADB Push (for developers)

```bash
# Copy to app-specific storage
adb push zelda3_assets.dat /sdcard/Android/data/com.dishii.zelda3/files/

# Verify
adb shell ls -lh /sdcard/Android/data/com.dishii.zelda3/files/zelda3_assets.dat
```

### Method 2: In-App File Picker (recommended for users)

1. Place `zelda3_assets.dat` somewhere accessible on device (e.g., Downloads folder)
2. Launch the app
3. When prompted, tap "Select File"
4. Navigate to the file and select it
5. The app automatically copies it to app-specific storage

**Note:** On Android 11+, the `Android/data/` directory is not directly accessible via file managers for security reasons. Use ADB or the in-app file picker.

## Using the Android App

### First Run Setup

**On first launch, the app will guide you through setup:**

1. **Grant storage permissions** when prompted
2. **Select Zelda3 folder** for MSU audio:
   - Dialog: "Choose where to store game files. MSU audio files will be stored here."
   - Create or select a folder (recommended: `/sdcard/Zelda3/`)
   - This folder is **only** used for MSU audio files
3. **Select assets file:**
   - The app will prompt you to select `zelda3_assets.dat`
   - Assets are copied to app-specific storage (see below)

### File Structure

**Android uses a split storage structure:**

**App-Specific Storage** (`/sdcard/Android/data/com.dishii.zelda3/files/`):
- `zelda3_assets.dat` - Game assets (required)
- `zelda3.ini` - Configuration file (auto-generated)
- `saves/` - Save files directory
  - `sram.dat` - In-game saves (Link's file)
  - `save1.sav` through `save10.sav` - Snapshot files (F1-F10)
  - `save1.png` through `save10.png` - Snapshot thumbnails

**User-Selected Folder** (e.g., `/sdcard/Zelda3/`):
- `MSU/` - MSU audio files only
  - `alttp_msu-1.pcm`, `alttp_msu-2.pcm`, etc.

**Why the split?**
- Android 11+ restricts direct access to `/sdcard/Android/data/` for security
- MSU files (1-2 GB) are stored in accessible location for easy management
- Assets, config, and saves remain in protected app-specific storage

### In-App Settings

**Android uses an in-app Settings UI instead of manual zelda3.ini editing.**

**To access Settings:**
1. Launch the app
2. Tap the **menu button** (three dots) or **back button** during gameplay
3. Select **Settings** from the menu
4. Configure options through the UI

**Available Settings:**
- **Graphics:** Renderer (OpenGL ES/Vulkan), window scale, aspect ratio, sprite limits
- **Audio:** MSU audio enable/disable, volume, sample rate
- **Controls:** Gamepad configuration, button mapping
- **Features:** Enhanced features (L/R switching, bug fixes, experimental features)
- **Language:** Select from available language packs

**Settings are saved to `zelda3.ini` automatically** - no manual editing required.

### Adding MSU Audio Files

MSU audio provides high-quality soundtrack replacement.

**Step 1: Create MSU folder in your Zelda3 folder**

Using a file manager app on your device:
1. Navigate to your Zelda3 folder (the one you selected on first launch)
2. Create a subfolder named `MSU` (case-sensitive)

Or via ADB:
```bash
# Replace /sdcard/Zelda3 with your selected folder path
adb shell mkdir -p /sdcard/Zelda3/MSU
```

**Step 2: Copy MSU files**

MSU files must follow the naming convention: `alttp_msu-1.pcm`, `alttp_msu-2.pcm`, etc.

**Via file manager (recommended):**
1. Connect device to computer
2. Navigate to `YourZeldaFolder/MSU/`
3. Copy all `.pcm` or `.opuz` files

**Via ADB:**
```bash
# Example: Copy to /sdcard/Zelda3/MSU/
adb push msu/alttp_msu-1.pcm /sdcard/Zelda3/MSU/
adb push msu/alttp_msu-2.pcm /sdcard/Zelda3/MSU/
# ... (repeat for all tracks)
```

**Step 3: Enable in Settings**
1. Launch Zelda3 app
2. Open Settings → Audio
3. Enable **MSU Audio**
4. The app automatically detects format (PCM/Opus) and configures paths
5. Adjust **MSU Volume** if needed

**Supported MSU formats:**
- **PCM** (`.pcm` files) - Standard format, larger files (~20-30 MB per track)
- **Opus** (`.opuz` files) - Compressed format (~2-3 MB per track, ~10% file size)
- **Deluxe** variants - Area-specific music tracks

**Note:** Full MSU soundtrack can be 500 MB - 2 GB. Ensure sufficient storage space.

### Deploying Assets to Device

The `zelda3_assets.dat` file goes to app-specific storage.

**Method 1: ADB Push (for developers)**

```bash
adb push zelda3_assets.dat /sdcard/Android/data/com.dishii.zelda3/files/

# Verify
adb shell ls -lh /sdcard/Android/data/com.dishii.zelda3/files/zelda3_assets.dat
```

**Method 2: Select File (recommended for users)**

1. Place `zelda3_assets.dat` somewhere accessible on your device (e.g., Downloads)
2. Launch the app
3. When prompted, tap "Select File"
4. Navigate to where you placed the file
5. The app copies it to app-specific storage automatically

### Save Files

Save files are stored in app-specific storage (accessible only via ADB or root):

```
/sdcard/Android/data/com.dishii.zelda3/files/saves/
```

**Backup save files:**
```bash
# Pull all saves from device
adb pull /sdcard/Android/data/com.dishii.zelda3/files/saves/ ./saves_backup/

# Restore saves to device
adb push ./saves_backup/ /sdcard/Android/data/com.dishii.zelda3/files/saves/
```

**Transfer saves between desktop and Android:**
```bash
# Desktop → Android
adb push saves/sram.dat /sdcard/Android/data/com.dishii.zelda3/files/saves/

# Android → Desktop
adb pull /sdcard/Android/data/com.dishii.zelda3/files/saves/sram.dat ./saves/
```

### Configuration File Access

**Manual zelda3.ini editing (advanced users only):**

```bash
# Pull config from device
adb pull /sdcard/Android/data/com.dishii.zelda3/files/zelda3.ini

# Edit locally with text editor

# Push back to device
adb push zelda3.ini /sdcard/Android/data/com.dishii.zelda3/files/
```

**⚠️ Note:** In-app Settings UI will override manual changes on next save. Use Settings UI whenever possible.

### Platform Differences

**Android vs Desktop:**

| Feature | Desktop | Android |
|---------|---------|---------|
| Configuration | Edit `zelda3.ini` manually | In-app Settings UI |
| Assets location | Same as executable | `/sdcard/Android/data/com.dishii.zelda3/files/` |
| Saves location | Same as executable | `/sdcard/Android/data/com.dishii.zelda3/files/saves/` |
| MSU location | `msu/` subfolder | User-selected folder + `/MSU/` |
| Renderer default | SDL | OpenGL ES |
| Aspect ratio default | 4:3 | extend_y, 16:10 (mobile-friendly) |
| Controls | Keyboard/gamepad | Touch controls + gamepad |

### Touch Controls

Android includes on-screen touch controls (virtual gamepad).

**Default layout:**
- Left side: D-pad (directional)
- Right side: A/B/X/Y buttons
- Top corners: L/R shoulder buttons
- Bottom: Start/Select buttons

**Customize touch controls:**
1. Settings → Controls → Touch Layout
2. Drag buttons to reposition
3. Resize with pinch gesture
4. Save layout

**Hide touch controls:**
- Connect physical gamepad (Bluetooth or USB)
- Touch controls auto-hide when gamepad detected

## Debugging with Logcat

### View All Logs

```bash
# Real-time log output
adb logcat

# Filter for Zelda3-related logs
adb logcat | grep Zelda3
```

### Filter by Tag

```bash
# Android-specific Zelda3 logs
adb logcat -s Zelda3Main Zelda3Platform

# SDL logs
adb logcat -s SDL

# All together
adb logcat -s Zelda3Main:D Zelda3Platform:D SDL:D
```

### Clear Logs

```bash
# Clear logcat buffer
adb logcat -c
```

### Save Logs to File

```bash
# Save all logs
adb logcat > zelda3_android.log

# Save filtered logs
adb logcat | grep Zelda3 > zelda3_android.log
```

### Android Studio Logcat

1. Open Android Studio
2. Run → Select Device
3. View → Tool Windows → Logcat
4. Filter by "Zelda3" in search box

## Renderer Configuration

Zelda3 on Android supports:

- **OpenGL ES** (default) - Best compatibility and performance
- **Vulkan 1.0** (optional) - Modern graphics API

### Configure Renderer

After deploying app, edit `zelda3.ini` on device:

```bash
# Pull config from device
adb pull /sdcard/Android/data/com.yourpackage.zelda3/files/zelda3.ini

# Edit locally
# Change OutputMethod = opengl_es or OutputMethod = vulkan

# Push back to device
adb push zelda3.ini /sdcard/Android/data/com.yourpackage.zelda3/files/
```

**Or edit directly:**
```bash
adb shell
cd /sdcard/Android/data/com.yourpackage.zelda3/files/
vi zelda3.ini  # or use text editor app
```

## Performance Optimization

### Build Settings

For maximum performance, use release builds:

```gradle
buildTypes {
    release {
        minifyEnabled false
        proguardFiles getDefaultProguardFile('proguard-android-optimize.txt')

        ndk {
            abiFilters 'arm64-v8a'  // 64-bit ARM only (fastest)
        }
    }
}
```

### Runtime Configuration

Edit `zelda3.ini` on device:

```ini
[Display]
OutputMethod = opengl_es  # Faster than SDL software
WindowScale = 2           # Lower = better performance

[Audio]
AudioSamples = 1024       # Lower = less latency, higher = less CPU
```

### CPU Affinity

Zelda3 runs at 60 FPS. For best performance:
- Use release builds (optimized)
- Close background apps
- Enable "High Performance" mode in Android settings
- Use devices with 4+ CPU cores

### GPU Compatibility

**OpenGL ES:**
- Supported on all Android devices (API 26+)
- Best compatibility
- Recommended for most devices

**Vulkan:**
- Requires Vulkan 1.0+ support (most devices since 2016)
- Better performance on supported hardware
- Check device compatibility: `adb shell dumpsys | grep vulkan`

## Troubleshooting

### "SDK not found"

**Issue:** Gradle cannot find Android SDK.

**Solution:**
```bash
# Set ANDROID_HOME environment variable
export ANDROID_HOME=$HOME/Android/Sdk

# Or create android/local.properties
echo "sdk.dir=$HOME/Android/Sdk" > android/local.properties
```

### "NDK not found"

**Issue:** Gradle cannot find Android NDK.

**Solution:**
```bash
# Install NDK via SDK Manager in Android Studio
# Or via command line:
$ANDROID_HOME/cmdline-tools/bin/sdkmanager "ndk;25.2.9519653"

# Specify NDK version in build.gradle
# android { ndkVersion "25.2.9519653" }
```

### Java Version Mismatch

**Issue:** Gradle 8.x requires Java 17+.

**Solution:**
```bash
# Check Java version
java -version

# Install Java 17 (Ubuntu)
sudo apt install openjdk-17-jdk

# Set JAVA_HOME
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
```

**Windows:** Download Java 17 from [adoptium.net](https://adoptium.net/).

### Asset Loading Fails

**Issue:** "zelda3_assets.dat not found" error in logcat.

**Solution:**
```bash
# Verify asset file exists on device
adb shell ls /sdcard/Android/data/com.yourpackage.zelda3/files/

# If missing, push again
adb push zelda3_assets.dat /sdcard/Android/data/com.yourpackage.zelda3/files/

# Check permissions (should be readable)
adb shell ls -l /sdcard/Android/data/com.yourpackage.zelda3/files/
```

### Build Fails: "Undefined Reference"

**Issue:** Native code linker errors.

**Solution:**
```bash
# Clean build and retry
cd android
./gradlew clean
./gradlew assembleDebug
```

### App Crashes on Launch

**Issue:** App crashes immediately after opening.

**Solution:**
```bash
# Check logcat for crash details
adb logcat | grep -A 20 AndroidRuntime

# Common causes:
# 1. Missing zelda3_assets.dat (see above)
# 2. Incompatible NDK version (use 21e+)
# 3. Wrong ABI (ensure arm64-v8a or armeabi-v7a)
```

### OpenGL ES Renderer Fails

**Issue:** Black screen or renderer initialization failure.

**Solution:**
```bash
# Check device GL version
adb shell dumpsys | grep GLES

# Ensure SDL_WINDOW_OPENGL flag is set (code-level fix)
# Fallback to SDL software renderer in zelda3.ini:
# OutputMethod = sdl
```

### Vulkan Renderer Not Available

**Issue:** Vulkan renderer fails to initialize.

**Solution:**
```bash
# Check device Vulkan support
adb shell dumpsys | grep -i vulkan

# If not supported, use OpenGL ES:
# Edit zelda3.ini: OutputMethod = opengl_es
```

### Audio Latency Issues

**Issue:** Audio delay or crackling.

**Solution:**
Edit `zelda3.ini` on device:
```ini
[Audio]
AudioSamples = 2048  # Higher = more latency but smoother
AudioFreq = 44100    # Match device native rate
```

### Storage Permission Denied

**Issue:** Cannot write to external storage.

**Solution:**
1. Settings → Apps → Zelda3 → Permissions
2. Grant "Files and Media" permission
3. Restart app

## Advanced Configuration

### Custom Package Name

Edit `android/app/build.gradle`:
```gradle
defaultConfig {
    applicationId "com.yourname.zelda3"  // Change this
}
```

After changing:
1. Update asset path when pushing files
2. Update logcat filters
3. Reinstall app (uninstall old one first)

### Multi-ABI Builds

Build for multiple architectures:
```gradle
ndk {
    abiFilters 'arm64-v8a', 'armeabi-v7a', 'x86', 'x86_64'
}
```

**Note:** Larger APK size but better compatibility.

### Signing Release Builds

Create keystore:
```bash
keytool -genkey -v -keystore zelda3-release.keystore -alias zelda3 \
  -keyalg RSA -keysize 2048 -validity 10000
```

Configure in `build.gradle`:
```gradle
signingConfigs {
    release {
        storeFile file('zelda3-release.keystore')
        storePassword 'your-password'
        keyAlias 'zelda3'
        keyPassword 'your-password'
    }
}
buildTypes {
    release {
        signingConfig signingConfigs.release
    }
}
```

### Gradle Daemon

Speed up builds by enabling Gradle daemon:
```bash
# Create/edit gradle.properties
echo "org.gradle.daemon=true" >> android/gradle.properties
echo "org.gradle.parallel=true" >> android/gradle.properties
```

## Build System Details

**Android build system:**
- Uses Gradle with Android Gradle Plugin
- JNI integration for native C code
- SDL2 integration via external SDL library
- Independent from desktop CMake build

**Source files:**
- Native code: `android/app/jni/` (JNI glue)
- Core game logic: `src/` (shared with desktop)
- Build config: `android/app/build.gradle`
- Gradle wrapper: `android/gradlew`

**Build process:**
1. Gradle compiles Java/Kotlin code
2. NDK compiles C/C++ code to native libraries
3. Packages APK with libraries and resources
4. Signs APK (debug or release)

## See Also

- [Installation Guide](../installation.md) - Overview of all platforms
- [Troubleshooting](../troubleshooting.md) - Common issues and solutions
- [Usage Guide](../usage.md) - Running and configuring the game
- [BUILDING.md](/Users/carl/Repos/zelda3/BUILDING.md) - Complete build documentation
- [Android Developer Docs](https://developer.android.com/studio/build) - Official Android build documentation
