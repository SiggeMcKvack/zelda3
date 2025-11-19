# Developer Guide

[Home](index.md) > Development

Comprehensive guide to developing Zelda3, including workflows, patterns, and best practices for implementing features and platform support.

## Table of Contents

- [Development Environment Setup](#development-environment-setup)
- [Common Development Tasks](#common-development-tasks)
- [Adding Features](#adding-features)
- [Adding Platform Support](#adding-platform-support)
- [Adding Renderers](#adding-renderers)
- [Performance Considerations](#performance-considerations)
- [Memory Management Patterns](#memory-management-patterns)
- [Platform Abstraction Layer](#platform-abstraction-layer)
- [Debugging During Development](#debugging-during-development)
- [Code Review Preparation](#code-review-preparation)

## Development Environment Setup

### IDE Configuration

**Visual Studio Code:**
```json
// .vscode/settings.json
{
  "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
  "cmake.buildDirectory": "${workspaceFolder}/build",
  "cmake.configureOnOpen": true,
  "files.associations": {
    "*.h": "c",
    "*.c": "c"
  }
}
```

**Visual Studio (Windows):**
```bash
# Generate Visual Studio solution
cmake .. -G "Visual Studio 17 2022"
# Open zelda3.sln
```

**CLion:**
- Open project directory
- CLion auto-detects CMake configuration
- Set build type to Debug in CMake settings

**Xcode (macOS):**
```bash
# Generate Xcode project
cmake .. -G Xcode
open zelda3.xcodeproj
```

### Build Configurations

**Debug Build:**
```bash
mkdir build-debug && cd build-debug
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j$(nproc)
```

**Release Build:**
```bash
mkdir build-release && cd build-release
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

**RelWithDebInfo (Optimized + Symbols):**
```bash
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build . -j$(nproc)
```

### Development Tools

**Recommended:**
- **GDB/LLDB:** Debugger
- **Valgrind:** Memory debugging (Linux)
- **AddressSanitizer:** Memory error detection
- **Git:** Version control
- **ccache:** Speed up rebuilds

**Optional:**
- **gprof:** Profiling (Linux)
- **Instruments:** Profiling (macOS)
- **Visual Studio Profiler:** Profiling (Windows)
- **perf:** Performance analysis (Linux)

**Installing ccache:**
```bash
# Linux
sudo apt install ccache
cmake .. -DCMAKE_C_COMPILER_LAUNCHER=ccache

# macOS
brew install ccache
cmake .. -DCMAKE_C_COMPILER_LAUNCHER=ccache
```

### Asset Management

Keep `zelda3_assets.dat` up to date:
```bash
# Re-extract after asset tool changes
python3 assets/restool.py --extract-from-rom

# Check asset file is current
ls -lh zelda3_assets.dat
# Should be ~2MB
```

## Common Development Tasks

### Rapid Development Workflow

**1. Build and Test Loop:**
```bash
# One-liner rebuild and test
cmake --build . -j$(nproc) && ./zelda3
```

**2. Watch for Changes (Linux/macOS):**
```bash
# Install inotify-tools or fswatch
# Then create a watch script
while inotifywait -r -e modify src/; do
  cmake --build . && ./zelda3
done
```

**3. Quick Compile Check:**
```bash
# Just compile, don't link
cmake --build . -j$(nproc) --target zelda3.c.o
```

### Debugging Print Statements

**Temporary Debug Output:**
```c
#include "logging.h"

// Quick debug print (remove before commit)
LogDebug("DEBUG: link_x_coord = %d", link_x_coord);
```

**Conditional Debug Output:**
```c
#if kDebugFlag
  LogDebug("Module: %d, Submodule: %d", main_module_index, submodule_index);
#endif
```

**Debug State Tracking:**
```c
#if kDebugFlag
  #include "debug_state.h"
  DebugState_LogSnapshot("Before boss fight");
#endif
```

### Code Navigation

**Finding Function Definitions:**
```bash
# Using grep
grep -r "void Player_HandleMovement" src/

# Using ctags
ctags -R .
# Then use Ctrl+] in vim or similar in other editors
```

**Finding Variable Usage:**
```bash
# Find all uses of a variable
grep -r "link_x_coord" src/

# Find RAM macro definition
grep "define link_x_coord" src/variables.h
```

**Understanding Module Flow:**
```bash
# Find module entry points
grep -r "main_module_index" src/

# Find submodule handlers
grep -r "submodule_index" src/
```

## Adding Features

Complete step-by-step guide for implementing new features.

### Step 1: Define Feature Flag

Edit `src/features.h`:

```c
// Find kRam_Features0 enum
enum {
  kRam_Features0 = 0x64c,  // Feature bitmask
};

// Add your feature to the feature flags enum
enum {
  // ... existing flags
  kFeatures0_MyFeature = 131072,  // Next power of 2 after last flag
};
```

**Power of 2 values:**
```
1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192,
16384, 32768, 65536, 131072, 262144, 524288, 1048576, ...
```

### Step 2: Add RAM Storage (if needed)

If your feature needs persistent state:

```c
// In features.h, after the enums
#define my_feature_state (*(uint8*)(g_ram+0x659))  // Next unused offset
```

**Finding next unused offset:**
```bash
# Look at last defined macro in features.h
grep "g_ram+" src/features.h | tail -1
# Use next available offset
```

### Step 3: Add Configuration Option

Edit `zelda3.ini` template:

```ini
[Features]
; ... existing features
; Enable my new feature (0 = off, 1 = on)
EnableMyFeature = 0
```

### Step 4: Parse Configuration

Edit `src/config.c`, find the Features section handler:

```c
// In the [Features] section parsing
if (strcmp(key, "EnableMyFeature") == 0) {
  if (value[0] == '1') {
    g_wanted_zelda_features |= kFeatures0_MyFeature;
  }
}
```

### Step 5: Implement Feature

In the relevant module (e.g., `src/player.c`):

```c
#include "features.h"

void Player_HandleMovement(void) {
  // Original behavior
  link_x_coord += link_velocity_x;

  // New feature behavior (gated)
  if (enhanced_features0 & kFeatures0_MyFeature) {
    // Enhanced movement code
    link_x_coord += link_velocity_x * 2;  // Example: move faster
  }
}
```

**Best Practices:**
- Feature must be optional (check the flag)
- Original behavior preserved when flag is disabled
- No performance impact when disabled
- Document the feature in comments

### Step 6: Test Feature

```bash
# Build with feature disabled
cmake --build .
./zelda3
# Verify original behavior

# Edit zelda3.ini, set EnableMyFeature = 1
./zelda3
# Verify new behavior

# Test with snapshots
# Save snapshots with feature on/off, compare behavior
```

### Step 7: Document Feature

Update documentation:
- Add to `zelda3.ini` with clear comment
- Add to README.md feature list (if user-facing)
- Add to CHANGELOG.md

### Example: Complete Feature Implementation

**Feature:** Infinite arrows when low on ammo

```c
// 1. features.h - Define flag
enum {
  kFeatures0_InfiniteArrows = 131072,
};

// 2. features.h - Add state (if needed)
// #define arrow_refill_timer (*(uint8*)(g_ram+0x659))

// 3. zelda3.ini
[Features]
EnableInfiniteArrows = 0

// 4. config.c - Parse
if (strcmp(key, "EnableInfiniteArrows") == 0) {
  if (value[0] == '1') {
    g_wanted_zelda_features |= kFeatures0_InfiniteArrows;
  }
}

// 5. player.c or ancilla.c - Implement
void Ancilla_CheckArrowsAvailable(void) {
  if (link_arrow_count == 0) {
    if (enhanced_features0 & kFeatures0_InfiniteArrows) {
      link_arrow_count = 10;  // Refill
    }
  }
}

// 6. Test both modes
// 7. Document in CHANGELOG.md
```

## Adding Platform Support

Guide for porting Zelda3 to new platforms.

### Step 1: Platform Detection

Edit `src/platform_detect.h`:

```c
// Add platform detection
#if defined(__MYPLATFORM__)
  #define PLATFORM_MYPLATFORM
#endif

// Add to conditional checks
#if defined(PLATFORM_LINUX) || defined(PLATFORM_MYPLATFORM)
  // Unix-like platforms
#endif
```

### Step 2: Create Platform Directory

```bash
mkdir -p src/platform/myplatform
```

Create platform-specific files:
- `src/platform/myplatform/platform_myplatform.c`
- `src/platform/myplatform/platform_myplatform.h`

### Step 3: Implement Platform Overrides

```c
// platform_myplatform.c
#include "platform.h"

#ifdef PLATFORM_MYPLATFORM

// Override file I/O if needed
PlatformFile *Platform_OpenFile(const char *filename, const char *mode) {
  // Platform-specific implementation
  return native_file_open(filename, mode);
}

// Override other functions as needed

#endif  // PLATFORM_MYPLATFORM
```

### Step 4: Update Build System

**For Unix-like platforms (CMake):**

Edit `CMakeLists.txt`:
```cmake
# Add platform-specific sources
if(MYPLATFORM)
  list(APPEND SOURCES
    src/platform/myplatform/platform_myplatform.c
  )

  # Add platform-specific libraries
  target_link_libraries(zelda3 myplatform_lib)
endif()
```

**For Embedded/Console platforms:**

Create separate build system (like `android/` or `src/platform/switch/Makefile`)

### Step 5: Handle Platform Specifics

**Initialization Order:**
```c
// In main.c or SDL_main
int main(int argc, char *argv[]) {
#ifdef PLATFORM_MYPLATFORM
  // Platform-specific early initialization
  MyPlatform_EarlyInit();
#endif

  // Standard initialization
  SDL_Init(SDL_INIT_EVERYTHING);
  // ...
}
```

**Path Handling:**
```c
#ifdef PLATFORM_MYPLATFORM
  // Get platform-specific asset path
  const char *asset_path = MyPlatform_GetAssetPath();
  chdir(asset_path);
#endif
```

**Logging:**
```c
#ifdef PLATFORM_MYPLATFORM
  // Use platform-specific logging before logging system is ready
  myplatform_log("Zelda3", "Initializing...");
#endif
```

### Step 6: Test Platform

Test all subsystems:
- Asset loading
- Input handling (keyboard/gamepad/touch)
- Rendering (SDL/OpenGL/Vulkan)
- Audio playback
- Save file I/O
- Performance

### Step 7: Document Platform

Add to `BUILDING.md`:
```markdown
## MyPlatform

Building for MyPlatform requires...

**Prerequisites:**
- MyPlatform SDK
- ...

**Build Steps:**
```bash
# Platform-specific build steps
```
```

### Example: Android Platform Integration

Android is a complex example showing full platform integration:

**Platform-specific aspects:**
1. **Entry point:** `SDL_main` instead of `main`
2. **Asset path:** `SDL_AndroidGetExternalStoragePath()`
3. **Logging:** `__android_log_print()` for early logs
4. **Build system:** Gradle (separate from CMake)
5. **Initialization order:** Audio mutex before SDL_Init
6. **File I/O:** Enhanced error logging in `Platform_ReadWholeFile()`

See `android/` directory and `src/main.c` for implementation details.

## Adding Renderers

Zelda3 supports multiple rendering backends through the `RendererFuncs` interface.

### Renderer Interface

Defined in `src/util.h`:

```c
struct RendererFuncs {
  bool (*Init)(SDL_Window *window);
  void (*Destroy)();
  void (*BeginDraw)(int width, int height, uint8 **pixels, int *pitch);
  void (*EndDraw)();
  void (*OnResize)(int width, int height);
};
```

### Step 1: Implement Renderer Interface

Create `src/myrenderer.c` and `src/myrenderer.h`:

```c
// myrenderer.h
#ifndef ZELDA3_MYRENDERER_H_
#define ZELDA3_MYRENDERER_H_

#include "types.h"
#include "util.h"
#include <SDL.h>

// Get renderer interface
const RendererFuncs *MyRenderer_GetFuncs(void);

#endif  // ZELDA3_MYRENDERER_H_
```

```c
// myrenderer.c
#include "myrenderer.h"

static bool MyRenderer_Init(SDL_Window *window) {
  // Initialize your renderer
  // Set up graphics API context
  return true;
}

static void MyRenderer_Destroy(void) {
  // Clean up resources
}

static void MyRenderer_BeginDraw(int width, int height,
                                  uint8 **pixels, int *pitch) {
  // Prepare for drawing
  // *pixels = pointer to pixel buffer
  // *pitch = bytes per row
}

static void MyRenderer_EndDraw(void) {
  // Finalize and present frame
}

static void MyRenderer_OnResize(int width, int height) {
  // Handle window resize
  // Update viewport, framebuffers, etc.
}

static const RendererFuncs kMyRendererFuncs = {
  .Init = MyRenderer_Init,
  .Destroy = MyRenderer_Destroy,
  .BeginDraw = MyRenderer_BeginDraw,
  .EndDraw = MyRenderer_EndDraw,
  .OnResize = MyRenderer_OnResize,
};

const RendererFuncs *MyRenderer_GetFuncs(void) {
  return &kMyRendererFuncs;
}
```

### Step 2: Register Renderer

Edit `src/main.c`:

```c
#include "myrenderer.h"

// In renderer initialization code
const RendererFuncs *g_renderer_funcs;

void InitRenderer(SDL_Window *window, int output_method) {
  switch (output_method) {
    case kOutputMethod_SDL:
      g_renderer_funcs = NULL;  // Use SDL software rendering
      break;
    case kOutputMethod_OpenGL:
      g_renderer_funcs = OpenGL_GetFuncs();
      break;
    case kOutputMethod_MyRenderer:
      g_renderer_funcs = MyRenderer_GetFuncs();
      break;
  }

  if (g_renderer_funcs && !g_renderer_funcs->Init(window)) {
    LogError("Failed to initialize renderer");
    // Fall back to SDL
  }
}
```

### Step 3: Add Configuration Option

Edit `zelda3.ini`:

```ini
[Output]
; OutputMethod: 0 = SDL, 1 = OpenGL, 2 = MyRenderer
OutputMethod = 0
```

Edit `src/config.c`:

```c
enum {
  kOutputMethod_SDL = 0,
  kOutputMethod_OpenGL = 1,
  kOutputMethod_MyRenderer = 2,
};

// In config parsing
if (strcmp(key, "OutputMethod") == 0) {
  int method = atoi(value);
  if (method == 2) {
    g_output_method = kOutputMethod_MyRenderer;
  }
}
```

### Step 4: Handle Rendering

The renderer receives frames from the PPU:

```c
// In main rendering loop (main.c)
void RenderFrame(void) {
  uint8 *pixels;
  int pitch;

  if (g_renderer_funcs) {
    // Custom renderer
    g_renderer_funcs->BeginDraw(kPpuWidth, kPpuHeight, &pixels, &pitch);

    // Copy PPU output to pixel buffer
    // pixels now points to your renderer's buffer

    g_renderer_funcs->EndDraw();
  } else {
    // SDL software rendering
    // ... default SDL code
  }
}
```

### Step 5: Test Renderer

```bash
# Build with new renderer
cmake --build .

# Test in zelda3.ini
[Output]
OutputMethod = 2

# Run and verify
./zelda3

# Test window resize
# Test with different resolutions
# Test with shaders (if applicable)
```

### Example: OpenGL Renderer

See `src/opengl.c` for a complete example:

```c
// Simplified OpenGL renderer structure
static bool OpenGL_Init(SDL_Window *window) {
  // Create OpenGL context
  context = SDL_GL_CreateContext(window);

  // Load OpenGL functions
  gladLoadGL();

  // Create textures, shaders, etc.
  glGenTextures(1, &texture);

  return true;
}

static void OpenGL_BeginDraw(int width, int height,
                              uint8 **pixels, int *pitch) {
  // Map texture or PBO
  *pixels = mapped_buffer;
  *pitch = width * 4;
}

static void OpenGL_EndDraw(void) {
  // Upload texture
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, ...);

  // Render quad
  glDrawArrays(GL_TRIANGLES, 0, 6);

  // Present
  SDL_GL_SwapWindow(window);
}
```

## Performance Considerations

### Hot Paths

Code executed every frame (60 FPS) must be optimized:

**Critical hot paths:**
1. `ZeldaRunFrame()` - Main game loop
2. `Ppu_RenderFrame()` - Graphics rendering
3. `Sprite_Main()` - Enemy AI updates
4. `Player_Main()` - Player updates

**Optimization strategies:**
- Avoid dynamic allocation
- Use lookup tables instead of calculations
- Inline small, frequently-called functions
- Minimize branching in tight loops
- Cache frequently-accessed values

### Memory Allocation

**Don't allocate in game loop:**
```c
// Bad - allocates every frame
void Player_Update(void) {
  int *temp = malloc(100 * sizeof(int));
  // ...
  free(temp);
}

// Good - use stack or static storage
void Player_Update(void) {
  int temp[100];  // Stack allocation
  // ...
}

// Or use static for persistent data
void Player_Update(void) {
  static int temp[100];  // One-time allocation
  // ...
}
```

### Lookup Tables

**Use precomputed tables:**
```c
// Bad - expensive calculation every call
uint16 GetAngleToPlayer(int sprite_x, int sprite_y) {
  int dx = link_x_coord - sprite_x;
  int dy = link_y_coord - sprite_y;
  return atan2(dy, dx) * 256 / (2 * PI);
}

// Good - use precomputed table
static const uint8 kAngleTable[256] = { /* precomputed values */ };
uint8 GetAngleToPlayer(int sprite_x, int sprite_y) {
  int dx = link_x_coord - sprite_x;
  int dy = link_y_coord - sprite_y;
  int index = ((dy << 4) + dx) & 0xff;
  return kAngleTable[index];
}
```

### Inlining

**Inline hot functions:**
```c
// In header file
static FORCEINLINE uint16 GetLinkXCoord(void) {
  return link_x_coord;
}

// Or use inline
static inline void UpdatePosition(int *x, int velocity) {
  *x += velocity;
}
```

### Profiling

**Find actual bottlenecks:**
```bash
# Linux - perf
perf record ./zelda3
perf report

# macOS - Instruments
instruments -t "Time Profiler" ./zelda3

# gprof (if compiled with -pg)
./zelda3
gprof ./zelda3 gmon.out > analysis.txt
```

**Focus optimization:**
- Profile first - don't guess
- Optimize hot paths identified by profiler
- Measure improvement
- Don't sacrifice readability for minimal gains

## Memory Management Patterns

### RAM Access

**Use macros, not direct access:**
```c
// Correct
link_x_coord = 100;
uint16 x = link_x_coord;

// Incorrect
*(uint16*)(g_ram+0x22) = 100;
```

### Dynamic Arrays

Use `DYNARR_*` macros from `src/dynamic_array.h`:

```c
#include "dynamic_array.h"

// Define array structure
typedef struct {
  uint8 *data;
  size_t size;
  size_t capacity;
} MyArray;

// Initialize
MyArray array = {0};

// Append
uint8 value = 42;
DYNARR_PUSH(array, value);

// Access
uint8 first = array.data[0];

// Iterate
for (size_t i = 0; i < array.size; i++) {
  uint8 v = array.data[i];
  // ...
}

// Cleanup
DYNARR_FREE(array);
```

### String Handling

**Use fixed buffers when possible:**
```c
// Good - fixed buffer
char filename[256];
snprintf(filename, sizeof(filename), "snapshot%d.sav", slot);

// Avoid - dynamic allocation
char *filename = malloc(256);
sprintf(filename, "snapshot%d.sav", slot);
free(filename);
```

### Resource Management

**RAII pattern (resource acquisition):**
```c
// Acquire resource
SDL_Texture *texture = SDL_CreateTexture(...);
if (!texture) {
  LogError("Failed to create texture");
  return false;
}

// Use resource
// ...

// Release resource
SDL_DestroyTexture(texture);
```

**Error handling with cleanup:**
```c
bool LoadLevel(const char *filename) {
  uint8 *data = NULL;
  bool success = false;

  // Allocate
  data = Platform_ReadWholeFile(filename, NULL);
  if (!data) {
    goto cleanup;
  }

  // Process
  if (!ProcessLevelData(data)) {
    goto cleanup;
  }

  success = true;

cleanup:
  free(data);
  return success;
}
```

## Platform Abstraction Layer

### File I/O

Always use Platform_* functions from `src/platform.h`:

```c
#include "platform.h"

// Read entire file
size_t length;
uint8 *data = Platform_ReadWholeFile("zelda3_assets.dat", &length);
if (!data) {
  LogError("Failed to load assets");
  return false;
}
// Use data...
free(data);

// Open file for reading
PlatformFile *file = Platform_OpenFile("config.ini", "r");
if (file) {
  char line[256];
  while (Platform_GetLine(line, sizeof(line), file)) {
    // Process line
  }
  Platform_CloseFile(file);
}

// Write file
PlatformFile *file = Platform_OpenFile("snapshot.sav", "wb");
if (file) {
  Platform_WriteFile(data, 1, size, file);
  Platform_CloseFile(file);
}
```

### Path Handling

**Case-insensitive path lookup:**
```c
// Linux has case-sensitive filesystem
// This helps find files with wrong case
char *actual_path = Platform_FindFileWithCaseInsensitivity("Assets/MUSIC/track01.ogg");
if (!actual_path) {
  LogError("File not found (check capitalization)");
} else {
  // Use actual_path
  free(actual_path);
}
```

### Logging

Use centralized logging from `src/logging.h`:

```c
#include "logging.h"

// Different severity levels
LogError("Critical error: %s", error_msg);     // Always shown
LogWarn("Warning: %s", warning_msg);           // Shown by default
LogInfo("Info: %s", info_msg);                 // Shown by default
LogDebug("Debug: %s", debug_msg);              // Hidden by default

// Control with environment variable
// ZELDA3_LOG_LEVEL=DEBUG ./zelda3
```

## Debugging During Development

### Adding Debug Output

**Temporary debugging:**
```c
#if kDebugFlag
  LogDebug("TEMP DEBUG: variable = %d", variable);
#endif
// Remember to remove before commit
```

**Permanent debug tracking:**
```c
#if kDebugFlag
  #include "debug_state.h"
  DebugState_LogSnapshot("Entering boss room");
#endif
```

### Using Assertions

```c
#include <assert.h>

void UpdateSprite(int sprite_id) {
  assert(sprite_id >= 0 && sprite_id < 16);
  assert(sprite_state[sprite_id] != 0);

  // Implementation...
}
```

**Assertions are disabled in release builds (NDEBUG defined)**

### Snapshot-Based Debugging

1. **Reproduce issue and save snapshot**
2. **Add debug output around problem area**
3. **Rebuild and load snapshot**
4. **Observe debug output**
5. **Iterate until bug found**

See [debugging.md](debugging.md) for comprehensive debugging guide.

## Code Review Preparation

### Before Submitting PR

**Self-review checklist:**
- [ ] Code follows style guidelines
- [ ] No compiler warnings
- [ ] Debug code removed or gated with `kDebugFlag`
- [ ] Documentation updated
- [ ] Commit messages are clear
- [ ] Tested in debug and release builds
- [ ] No performance regressions
- [ ] Original behavior preserved (or feature is optional)

### Code Quality

**Run static analysis:**
```bash
# Warnings as errors
cmake .. -DENABLE_WERROR=ON
cmake --build .

# Should compile without warnings
```

**Check for common issues:**
```bash
# Find TODO/FIXME comments
grep -r "TODO\|FIXME" src/

# Find debug printf
grep -r "printf" src/

# Find potential memory issues
grep -r "malloc\|free" src/
```

### Testing Checklist

- [ ] Manual testing in debug build
- [ ] Manual testing in release build
- [ ] Snapshot replay testing
- [ ] Cross-platform testing (if possible)
- [ ] Performance testing (F key)
- [ ] With feature enabled (if applicable)
- [ ] With feature disabled (if applicable)
- [ ] Edge cases tested

### Documentation

**Update relevant files:**
- `zelda3.ini` - New config options
- `README.md` - User-facing features
- `CHANGELOG.md` - All changes
- `docs/` - Technical documentation
- Code comments - Complex logic

## Additional Resources

- **[contributing.md](contributing.md)** - Contribution guidelines and workflow
- **[debugging.md](debugging.md)** - Debugging tools and techniques
- **[architecture.md](architecture.md)** - Technical architecture details
- **[ARCHITECTURE.md](../ARCHITECTURE.md)** - High-level architecture
- **[BUILDING.md](../BUILDING.md)** - Build instructions and troubleshooting

---

**Questions?** Join us on Discord: https://discord.gg/AJJbJAzNNJ
