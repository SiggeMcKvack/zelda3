# Changelog

Notable changes, improvements, and additions to the Zelda3 project.

## Recent Updates (November 2025)

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
- Vulkan renderer for desktop (requires shader file loading)
- More platform abstractions (networking, threading)
- Additional feature flags from Android port
- Enhanced debugging tools

**Maintenance:**
- Keep CMake and documentation in sync
- Monitor upstream zelda3 for new changes
- Coordinate with Android port for shared improvements
