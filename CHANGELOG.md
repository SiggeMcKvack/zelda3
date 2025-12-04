# Changelog

Notable changes, improvements, and additions to the Zelda3 project.

## December 2025

### Launcher: PCM to Opus Encoding

Added a button in the launcher's Sound tab to encode MSU PCM audio files to Opus format directly from the UI.

**Features:**
- "Encode PCM files to Opus..." button appears after selecting an MSU folder with PCM files
- Button is only enabled when PCM files are detected (disabled for Opus files)
- Confirmation dialog asks whether to keep or remove PCM files after encoding
- Progress dialog shows encoding progress with cancel option
- If keeping PCM files, they are moved to a `pcm_original/` subdirectory after successful encoding
- Files are only moved/deleted after all tracks encode successfully
- MSU dropdown auto-switches to Opus/Opus Deluxe format on completion

**Opus Encoder Library:**
- Added progress callback support via `OpusEncoder_EncodeFileEx()` function
- New `OpusEncoderOptionsEx` struct with callback and user data fields
- Progress reported every 10 frames during encoding

**Files Modified:**
- `src/opus_encoder/opus_encoder_lib.h` - Added progress callback types and `OpusEncoder_EncodeFileEx()`
- `src/opus_encoder/opus_encoder_lib.c` - Implemented progress callback support
- `src/launcher/launcher_ui.c` - Added encode button, dialogs, and encoding logic
- `CMakeLists.txt` - Link opus_encoder_lib to launcher

### Break Pots with Sword - Configurable Minimum Sword Level

The `BreakPotsWithSword` setting now accepts values 0-4 instead of boolean 0/1:
- `0` = Disabled (default, original game behavior)
- `1` = Wooden sword (any sword can break pots)
- `2` = Master sword or higher
- `3` = Tempered sword or higher
- `4` = Golden sword only

**Desktop Launcher:**
- Features tab reorganized with section headers (Save, Controls, Gameplay, Interface, Bug Fixes, Experimental)
- Break pots option changed from checkbox to dropdown selector

**Android:**
- Gameplay settings dialog updated with dropdown for sword level selection

**Files Modified:**
- `src/config.h` - Added `break_pots_min_sword` field
- `src/config.c`, `src/dungeon.c` - Use new integer config
- `src/features.h` - Deprecated `kFeatures0_BreakPotsWithSword` bit flag
- `src/launcher/launcher_ui.c` - Section headers + combo box
- `android/.../dialog_gameplay_settings.xml` - Spinner widget
- `android/.../MainActivity.kt` - Integer config handling

### Android Refactoring - Generic Utilities

**JNI Helper Library (C):**
- `src/platform/android/jni_helpers.h/c` - Generic JNI method callers, JSON builder, shared button name table
- Reduces ~300 lines of duplicated JNI boilerplate in `android_jni.c`

**Kotlin Utilities:**
- `util/ConfigManager.kt` - Generic INI read/write operations
- `util/UiExtensions.kt` - Toast/Dialog extension functions
- `util/FileUtils.kt` - File copy operations (URI, asset, SAF)
- Reduces ~400 lines across MainActivity, LauncherActivity, RomSelectionActivity

**Platform Save File API:**
- Added `Platform_OpenSaveFile()`, `Platform_ReadSaveFile()`, `Platform_WriteSaveFile()`, `Platform_CloseSaveFile()`
- Desktop: Uses `saves/` directory with FILE*
- Android: Uses SAF (Storage Access Framework) via JNI with fd→FILE* wrapper
- Unified API removes `#ifdef PLATFORM_ANDROID` branches from `zelda_rtl.c`

**Files Added:**
- `src/platform/android/jni_helpers.h` (~80 lines)
- `src/platform/android/jni_helpers.c` (~250 lines)
- `android/.../util/ConfigManager.kt` (~80 lines)
- `android/.../util/UiExtensions.kt` (~60 lines)
- `android/.../util/FileUtils.kt` (~50 lines)

**Files Modified:**
- `src/platform.h` - Save file API declarations
- `src/platform.c` - Desktop save file implementation
- `src/platform/android/android_jni.c` - Android save file implementation (-326 lines)
- `src/zelda_rtl.c` - Unified save operations (-22 lines)

## November 2025

### PNG Sprite Loading (--sprites-from-png)

**New Feature:** Load sprite graphics from PNG files instead of ROM

The C restool now supports the `--sprites-from-png` flag, matching the Python implementation's feature for ROM hacking and sprite modifications.

**Implementation:**
- Uses lodepng library for PNG decoding
- Loads sprite tilesets 0-102 from `assets/sprites/sprites_*.png` files
- Decodes embedded metadata tags (tileset ID, palette info, checksums)
- Converts 24-bit RGB back to indexed format using embedded palette swatches
- Encodes to SNES 3bpp planar format (1536 bytes per tileset)
- ~400 lines of C code in `sprite_loader.c`
- Byte-perfect match with Python output verified

**Files Added:**
- `src/restool/sprite_loader.h` - Header with SpriteSheetLoader API
- `src/restool/sprite_loader.c` - PNG loading and SNES encoding implementation

**Usage:**
```bash
./zelda3_restool --extract-from-rom zelda3.sfc --sprites-from-png --compile
```

### C Restool Completion

**Major Change:** Complete C reimplementation of asset extraction tool

The Python-based asset extraction has been fully reimplemented in C, providing:

**Features:**
- **Speed:** Faster than Python implementation
- **Portability:** No Python dependency required
- **Multi-language:** Supports all 11 ROM versions (US, DE, FR, FR-C, EN, ES, PL, PT, NL, SV, Redux)
- **PNG sprite loading:** `--sprites-from-png` flag for ROM hacking workflows
- **Embedded assets:** YAML templates embedded in binary for standalone operation
- **Launcher integration:** General tab creates assets via GUI

**Files Added:**
- `src/restool/` - Complete C restool implementation (15 source files)
- `docs/RESTOOL_C.md` - Comprehensive usage documentation

**Usage:**
```bash
./zelda3_restool --extract-from-rom zelda3.sfc --compile
```

See [C Restool Guide](docs/RESTOOL_C.md) for complete documentation.

### Launcher General Tab

**New Feature:** Asset file management directly in GTK3 launcher

- **Asset status indicator:** Shows if zelda3_assets.dat exists and is valid
- **Language dropdown:** Select from available languages extracted from ROM
- **ROM file browser:** Native GTK file chooser for ROM selection
- **Create Asset File:** Run C restool extraction directly from launcher
- **Error reporting:** Clear messages for ROM detection issues

### Vulkan Renderer Integration

**Major Changes:** Cross-platform Vulkan 1.0 renderer ported from zelda3-android fork

Successfully integrated the Vulkan renderer from the zelda3-android fork, making it work on both desktop (via MoltenVK on macOS, native on Linux/Windows) and Android (native Vulkan API). The renderer uses simple fullscreen quad rendering with SPIR-V shaders, maintaining Vulkan 1.0 compatibility for maximum device support.

**Vulkan Renderer Features:**
- Vulkan 1.0 API (maximum compatibility: Android API 24+, macOS via MoltenVK, Linux/Windows native)
- Pre-compiled SPIR-V shaders (vert.spv, frag.spv) with GLSL source for reference
- Cross-platform surface creation via SDL2's Vulkan support
- Automatic MoltenVK detection and portability extension handling on macOS
- Swap chain management with automatic recreation on window resize
- Double/triple buffering with proper synchronization (fences + semaphores)

**Platform-Specific Handling:**
- **macOS (MoltenVK):** Auto-enables VK_KHR_portability_enumeration and VK_KHR_portability_subset extensions
- **Android:** Loads shaders from APK assets via JNI, native Vulkan 1.0 support (API 24+)
- **Desktop (Linux/Windows):** Loads shaders from filesystem, native Vulkan drivers

**Code Changes:**
- Added `src/vulkan.c` (1,377 LOC) and `src/vulkan.h` - Vulkan 1.0 renderer implementation
- Updated `src/main.c` - Wire Vulkan renderer into output method selection
- Updated `android/app/jni/CMakeLists.txt` - Added platform/android include path for android_jni.h
- Added `shaders/` directory - Pre-compiled SPIR-V shaders and compilation script
- Updated `zelda3.ini` - Added Vulkan to OutputMethod documentation

**Testing:**
- ✅ Desktop (macOS): Apple M2 Pro via MoltenVK, 768x672 swap chain, 60 FPS
- ✅ Android: All ABIs (arm64-v8a, armeabi-v7a, x86, x86_64), 20MB APK with Vulkan support
- ✅ Shader loading: Filesystem (desktop) and APK assets (Android) both working

**Configuration:**
Set `OutputMethod = Vulkan` in zelda3.ini to use the Vulkan renderer. Falls back gracefully to SDL/OpenGL if Vulkan is unavailable.

### GTK3 Launcher UI Enhancements

**Major Updates:** Path selection browsers and comprehensive gamepad configuration UI

Significantly enhanced the GTK3 launcher with file/folder browsers for asset paths and a complete gamepad remapping interface organized into logical categories.

**Path Selection Features:**
- **MSU Folder Browser** (Sound tab) - GTK folder chooser for selecting MSU audio directory
  - Click "Browse..." button opens native folder selection dialog
  - Selected path automatically saved to zelda3.ini with `/alttp_msu-` suffix
  - Cross-platform compatibility (macOS, Linux, Windows)

- **Shader File Browser** (Graphics tab) - GTK file chooser for selecting GLSL shader files
  - Click "Browse..." button opens native file selection dialog
  - Filters for .glsl and .glslp files
  - Selected path automatically saved to zelda3.ini
  - Cross-platform file dialogs via GTK3

**Gamepad UI Restructure:**
- Reorganized gamepad tab into 4 logical subtabs matching keyboard layout:
  1. **Controls** - Core SNES buttons (D-pad, A/B/X/Y/L/R, Start/Select)
  2. **Save States** - Load/Save/Replay bindings for F1-F10 slots
  3. **Cheats** - CheatLife, CheatKeys, CheatWalkThroughWalls
  4. **System** - Fullscreen, Reset, Pause, Turbo, Window controls, Replay controls

- Full button detection UI for each binding:
  - Click binding button → "Press button..." state
  - Press gamepad button → Detected and displayed
  - Supports modifier combos (L2+A, R2+B, etc.)
  - Clear button for each binding to remove assignment

**Keyboard UI Enhancements:**
- Added clear buttons for all keyboard bindings
- Matches gamepad tab organization for consistency

**Configuration Updates:**
- Updated all INI files (zelda3.ini, android/.../zelda3.ini, platform/switch/zelda3.ini)
- Complete [GamepadMap] structure with all command categories
- Comprehensive inline comments explaining binding syntax

**UI Polish:**
- Left-aligned section headings for better readability
- Renamed sections for clarity ("SNES Controller Buttons" → "SNES Controller")
- Removed "Click button to remap" subtitle text
- Consistent widget spacing across all tabs

**Code Changes:**
- `src/launcher_ui.c` (+577 lines) - Path browsers, gamepad subtabs, clear buttons, UI refinements
- `src/launcher_ui.h` - Updated global variable declarations for gamepad bindings
- `src/config_writer.c` (+167 lines) - Enhanced INI generation with complete gamepad structure
- `src/config_reader.c` (+71 lines) - Improved parsing for new binding categories
- All INI files (+65-89 lines) - Complete gamepad binding documentation

**User Impact:**
- No more manual INI editing for MSU/Shader paths - use native file browsers
- Complete gamepad remapping without leaving launcher
- Organized UI makes finding specific bindings much easier
- Clear buttons provide quick way to unbind unwanted keys/buttons

**Configuration:**
All settings are immediately saved to zelda3.ini when clicking "Save" or "Save & Launch". The launcher provides visual feedback for all path selections and binding detections.

### Build System Modernization & Windows Compatibility

**Major Changes:** Switched from vendored Opus to system library, fixed Windows build failures

After 25+ failed build attempts over multiple sessions, root cause identified and fixed: RAM macro names (R10, R12, R14, R16, R18, R20) in `src/variables.h` collided with Windows x64 register names used in `setjmp.h`. When `audio.c` included `variables.h` before Windows headers, the macros polluted preprocessor state, causing parsing failures when `setjmp.h` was later included via SDL → `intrin.h`.

**Opus Library Migration:**
- Removed vendored `third_party/opus-1.3.1-stripped/` (-924KB, 74 files)
- Switched to system-installed Opus library via pkg-config
- Added `cmake/FindOpus.cmake` module for cross-platform detection
- Updated all CI platforms: apt (Ubuntu), pacman (Arch), vcpkg (Windows), brew (macOS)
- Updated BUILDING.md with Opus installation instructions

**Windows Build Fixes:**
- Renamed R10/R12/R14/R16/R18/R20 → g_r10/g_r12/g_r14/g_r16/g_r18/g_r20 (8 files, 192 replacements)
- Follows existing `g_` prefix convention (g_ram, g_zenv, etc.)
- Avoids collision with Windows x64 register names (R8-R15)
- Used Clang-CL for diagnosis (clearer error messages than MSVC)
- Removed experimental workarounds (DISABLE_PTR_CHECK, /Zc:preprocessor flags)

**CI/CD Improvements:**
- Updated vcpkg action to v11.5
- All 10 build jobs passing: Ubuntu Debug/Release, Arch Debug/Release, Windows Debug/Release, macOS ARM64 Debug/Release, macOS x86_64 Debug/Release
- Added Opus dependency to all platform workflows

**Files Modified:**
- `CMakeLists.txt` - Switched to find_package(Opus), removed vendored sources
- `src/audio.c` - Changed include from vendored to `<opus/opus.h>`
- `src/variables.h` - Renamed R* macros to g_r* (lines 1424-1429)
- `src/{player,dungeon,zelda_cpu_infra,tile_detect,overworld,ending,ancilla}.c` - Updated macro usage
- `.github/workflows/build.yml` - Added Opus deps for all platforms, vcpkg@v11.5
- `cmake/FindOpus.cmake` - New pkg-config based module
- `BUILDING.md` - Added Opus installation instructions

**Technical Notes:**
- Opus used only for MSU1 audio decoding (custom OPUZ format with savestate resume)
- No encoding, 4 functions used: opus_decoder_create, opus_decode, opus_decoder_ctl, opus_decoder_destroy
- System library adds ~200KB to binary vs 924KB vendored source
- Build time slightly faster without vendored compilation

### Pokemode & PrincessZeldaHelps Features

**New Experimental Features:** Pokemon-style monster capture and Zelda companion mode

Ported from the Android fork, these features add optional gameplay mechanics while preserving original behavior when disabled.

**Pokemode - Monster Capture System:**
- Capture enemies and NPCs with Bug Net (sprites taking Bug Net damage)
- Store captured sprites in bottles using extended bottle states (0xF3-0xFB + sprite IDs)
- Release captured sprites from bottles to spawn at Link's position
- Friendly AI mode: Some sprites (Guards, Ravens, Zelda) attack nearby enemies
- Bottles display flute icon instead of standard icons when Pokemode enabled
- Supported captures: Ravens, Vultures, Stalfos Heads, Guards, Princess Zelda, followers (Old Man, Kiki, Blind Maiden, etc.)

**PrincessZeldaHelps - Zelda Companion:**
- Princess Zelda becomes a permanent follower outside normal story sequence
- Triggers when released from bottle with AI state 10 (Pokemode integration)
- Triggers after healing Link at sanctuary (replaces normal despawn)
- Uses existing tagalong/follower system

**Sprite-to-Sprite Targeting System:**
- Added `Sprite_IsRightOfTarget()`, `Sprite_IsBelowTarget()` - Position checks between sprites
- Added `Sprite_ProjectSpeedTowardsTarget()` - Calculate velocity vector toward target sprite
- Added `Sprite_ApplySpeedTowardsTarget()` - Set sprite velocity toward another sprite
- Added `Sprite_DirectionToFaceTarget()` - Get direction for sprite to face another sprite
- Added `LinkItem_Net_endAnimation()` - Clean Bug Net animation end
- Enables friendly AI to properly chase and attack enemies instead of Link

**Implementation Details:**
- ~300 lines of new code across sprite system, player, HUD modules
- Fully gated behind `kFeatures0_Pokemode` and `kFeatures0_PrincessZeldaHelps` feature flags
- Disabled by default (preserves original behavior and replay compatibility)
- Added follower indicator constants (13 values) and bottle state constants (17 extended states)
- Friendly sprites use AI state 10 with bee-style enemy-seeking behavior

**Files Modified:**
- `src/sprite_main.c` - Core capture/release logic, friendly AI, Zelda follower triggers
- `src/sprite.c` - Sprite-to-sprite targeting utilities (6 functions, ~77 lines)
- `src/sprite.h` - Function declarations for targeting system
- `src/player.c` - Extended bottle usage logic, Bug Net animation cleanup
- `src/player.h` - Follower indicator and bottle state enums
- `src/hud.c` - Bottle icon display (flute icon for Pokemode)
- `zelda3.ini` - Comprehensive feature descriptions with warnings

**Configuration:**
```ini
[Features]
Pokemode = 0                # Pokemon-style capture system
PrincessZeldaHelps = 0      # Zelda companion mode
```

### Path Validation for Case-Sensitive Filesystems

**New Feature:** Case-insensitive path validation (`src/platform.c`, `src/config.c`)

Addresses cross-platform compatibility issues between case-insensitive (Windows/macOS) and case-sensitive (Linux) filesystems:

**Platform Layer Enhancement:**
- Added `Platform_FindFileWithCaseInsensitivity()` to `src/platform.h`/`src/platform.c`
- On Windows/macOS: Validates path existence (filesystems are case-insensitive)
- On Linux: Scans directories for case-insensitive matches
- Returns corrected path or NULL if not found

**Config Validation:**
- Validates MSU audio and shader paths at startup
- Detects case mismatches before runtime failures
- Provides helpful error messages:
  ```
  ERROR: Shader path 'glsl-shaders/crt.glslp' not found
    Note: On case-sensitive filesystems (Linux), the path must match exactly
    Check that the directory/file exists with the correct capitalization
  ```

**Impact:**
- Linux users get clear guidance when paths don't match exact capitalization
- Prevents silent failures (MSU audio) and runtime errors (shaders)
- No performance impact (validation only runs during config loading)
- Zero overhead on case-insensitive filesystems

**Files Modified:**
- `src/platform.h` - New API declaration
- `src/platform.c` - Case-insensitive path lookup implementation
- `src/config.c` - Path validation in `ParseConfigFile()`

## Android Port Integration (2024)

### Build System Migration

**CMake Build System** - Replaced Makefile with modern CMake
- Automatic dependency detection (SDL2, OpenGL, Vulkan)
- Cross-platform support: Linux, macOS, Windows
- Out-of-source builds (clean separation)
- IDE integration (VS Code, CLion, Visual Studio, Xcode)
- Platform-specific configuration
- Optional `-Werror` flag (disabled by default)
- Matches Android port build approach

**Breaking Change:** Makefile removed. Use CMake for all builds.

### Bug Fixes

#### OpenGL Rendering Fixes

**Viewport Calculation** (`src/opengl.c:211`)
- **Issue:** Incorrect vertical centering due to typo
- **Before:** `viewport_y = (viewport_height - viewport_height) >> 1;` (always 0)
- **After:** `viewport_y = (drawable_height - viewport_height) >> 1;`
- **Impact:** All platforms using OpenGL renderer

**OpenGL ES Format Compliance** (`src/opengl.c:223-226`)
- **Issue:** Internal format didn't match format parameter per OpenGL ES spec
- **Before:** `glTexImage2D(..., GL_RGBA, ..., GL_BGRA, ...)`
- **After:** `glTexImage2D(..., GL_BGRA, ..., GL_BGRA, ...)`
- **Impact:** Better OpenGL ES compatibility, fewer driver warnings

#### Memory Safety Improvements

**Null Pointer Checks** (`src/glsl_shader.c`)
- Added 6 critical null checks after `calloc()` and `strdup()` calls
- Prevents crashes on allocation failures
- **Affected functions:**
  - `ParseTextures()` - Texture allocation
  - `GlslShader_GetParam()` - Parameter allocation
  - `GlslShader_InitializePasses()` - Pass array allocation
  - `GlslShader_CreateFromFile()` - Shader filename duplication
- **Severity:** High - crash protection

### New Features

#### Platform Abstraction Layer

**New Files:** `src/platform.h`, `src/platform.c`

Provides platform-agnostic file I/O interface:
```c
PlatformFile *Platform_OpenFile(const char *filename, const char *mode);
size_t Platform_ReadFile(void *ptr, size_t size, size_t count, PlatformFile *file);
size_t Platform_WriteFile(const void *ptr, size_t size, size_t count, PlatformFile *file);
int Platform_CloseFile(PlatformFile *file);
uint8_t *Platform_ReadWholeFile(const char *filename, size_t *length_out);
```

**Purpose:**
- Easier platform porting
- Default implementation uses standard C `FILE*`
- Future platforms can override (e.g., Android uses SDL_RWops)

#### Frame Buffer Accessor API

**New Function:** `PpuGetFrameBuffer()` in `snes/ppu.h`, `snes/ppu.c`

```c
void PpuGetFrameBuffer(Ppu *ppu, uint8_t **buffer_out,
                       int *width_out, int *height_out, int *pitch_out);
```

**Use Cases:**
- Screenshot capture
- Video recording
- Frame analysis/debugging
- External rendering pipelines

**Returns:**
- Pointer to RGBA frame buffer
- Dimensions: 256×224 or 256×240
- Pitch (stride) for proper pixel access

#### Gamepad Binding API Extensions

**New Functions:** `src/config.h`, `src/config.c`

Runtime gamepad configuration:
```c
void GamepadMap_Add(int button, uint32 modifiers, uint16 cmd);
void GamepadMap_Clear(void);
int GamepadMap_GetBindingForCommand(int cmd, uint32 *modifiers_out);
const char* FindCmdName(int cmd);
int ParseGamepadButtonName(const char **value);
```

**Exposed Data:**
```c
extern const uint8 kDefaultGamepadCmds[];
```

**Purpose:**
- Runtime gamepad reconfiguration
- Command name lookups
- Custom button mappings
- JNI integration (Android) or GUI configuration

#### New Feature Flags

**Added to `src/features.h`:**
```c
kFeatures0_Pokemode = 131072              // Experimental game mode
kFeatures0_PrincessZeldaHelps = 262144    // Princess Zelda assists mode
```

**Usage:**
```c
if (enhanced_features0 & kFeatures0_PrincessZeldaHelps) {
  // Enable Princess Zelda assistance
}
```

**Configuration:** Add to `zelda3.ini` under features bitmask.

### Documentation Improvements

**New Documentation Files:**
- `BUILDING.md` - Comprehensive build instructions
- `ARCHITECTURE.md` - Detailed technical architecture
- `CHANGELOG.md` - This file

**Updated Documentation:**
- `CLAUDE.md` - Focused on AI-specific guidance
- `README.md` - Updated to use CMake

**Organization:**
- Separated concerns (build vs architecture vs changes)
- Easier navigation
- Better onboarding for new contributors

### Code Quality

**Improvements:**
- Added null pointer safety checks
- Fixed OpenGL compliance issues
- Better error handling in shader loading
- Cleaner platform abstraction

**Maintained:**
- Original behavior compatibility
- Replay determinism
- Frame-by-frame ROM verification support

## Source

These updates were selectively ported from the [zelda3-android](https://github.com/Waterdish/zelda3-android) fork, which added:
- Complete Android app layer (Kotlin)
- JNI integration
- Vulkan renderer (Android-specific, not ported)
- Touch overlay system (Android-specific)
- Hot-reload capabilities (Android-specific)

**Porting Strategy:**
- Bug fixes: All ported ✅
- Platform abstraction: Ported ✅
- New APIs: Ported ✅
- Build system: CMake ported ✅
- Vulkan renderer: Deferred (requires desktop shader loading infrastructure)

## Previous History

For changes before this major update, see:
- Git commit history: `git log`
- Original repository: https://github.com/snesrev/zelda3

## Future Considerations

**Potential Additions:**
- More platform abstractions (networking, threading)
- Additional feature flags from Android port
- Enhanced debugging tools
- iOS port

**Maintenance:**
- Keep CMake and documentation in sync
- Monitor upstream zelda3 for new changes
- Coordinate with Android port for shared improvements
