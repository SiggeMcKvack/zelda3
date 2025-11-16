# Changelog

Notable changes, improvements, and additions to the Zelda3 project.

## Recent Updates - Android Port Integration (2024)

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
