# Zelda3 Refactoring TODO List

Generated: 2025-11-16

This document tracks all identified issues and improvements from comprehensive codebase analysis.

## 🔴 CRITICAL - Security & Crash Fixes ✅ PHASE 1 COMPLETE

### [X] 1. Fix BPS patch integer overflow (SECURITY RISK)
- **File**: `src/util.c:223-280`
- **Issue**: Unchecked arithmetic on patch offsets, `dst[outputOffset++]` and `src[sourceRelativeOffset++]` lack bounds checking
- **Risk**: Buffer overflow from malicious BPS patches
- **Fix**: Add bounds validation before all array accesses

### [X] 2. Fix OpenGL buffer overflow (memory corruption)
- **File**: `src/opengl.c:181-194`
- **Issue**: Allocates `size * 4` bytes where `size = width * height`, could overflow on extreme dimensions
- **Risk**: Integer overflow → undersized buffer → heap corruption
- **Fix**: ✅ Added overflow detection and safe size_t arithmetic

### [X] 3. Fix Platform.c integer overflow check
- **File**: `src/platform.c:75-112` (Platform_ReadWholeFile)
- **Issue**: Allocates `size + 1` without checking if cast from `long` to `size_t` could overflow
- **Risk**: Integer overflow on 32-bit systems with files > 2GB
- **Fix**: ✅ Added SIZE_MAX validation before allocation

### [X] 4. Fix config.c unbounded realloc crash
- **File**: `src/config.c:74-78`
- **Issue**: `keymap_hash` realloc check `> 10000` uses `Die()` instead of graceful handling
- **Risk**: Crashes on malformed config files
- **Severity**: Medium (user-controlled input)
- **Fix**: ✅ Changed Die() to fprintf + return false

### [X] 5. Fix main.c unsafe float-to-int conversion
- **File**: `src/main.c:756`
- **Issue**: `(uint8)(int)(ApproximateAtan2(...) * 64.0f + 0.5f)` without range validation
- **Risk**: Out-of-bounds array access in `kSegmentToButtons`
- **Fix**: ✅ Added clamping to [0, 255] range

### [X] 6. Fix glsl_shader.c resource leaks on error paths
- **File**: `src/glsl_shader.c:361-464`
- **Issue**: Multiple `goto FAIL` paths don't clean up all allocated resources
- **Example**: Line 387 - shader filename strdup not freed if later allocations fail
- **Fix**: ✅ Made InitializePasses return bool, added null checks to Destroy

### [X] 7. Fix audio.c array bounds checking
- **File**: `src/audio.c:88-99`
- **Issue**: Line 94 checks `which_entrance` against `sizeof()` which could be wrong if malformed
- **Fix**: ✅ Changed to countof() and added bounds check at line 127

### [X] 8. Fix Windows-only pause dimming (cross-platform)
- **File**: `src/main.c:632-638`
- **Issue**: Feature only works on Windows, comment says "SDL_RenderPresent may not be called more than once per frame"
- **Risk**: Feature broken on Linux/macOS
- **Fix**: ✅ Documented limitation with FIXME comment and #else clause

### [X] 9. Fix inconsistent path separator handling
- **Files**: `src/main.c:855-878`, `src/util.c:100`
- **Issue**: Uses `/` as separator but also checks for `\\`, inconsistent across platforms
- **Fix**: ✅ Use platform-specific separator with #ifdef _WIN32

### [X] 10. Fix config.c memory leak
- **File**: `src/config.c:522`
- **Issue**: `g_config.memory_buffer` set to `filedata` but never freed
- **Fix**: ✅ Added Config_Shutdown() function, called from main()

---

## 🔄 MULTI-FILE REFACTORING ✅ PHASE 2 COMPLETE (Logging)

### [X] 11. Create unified logging system (SM-style)
- **Files**: `src/logging.h` and `src/logging.c`
- **Approach**: Copied SM's proven simple design (YAGNI/KISS)
- **Features**:
  - 4 log levels: ERROR, WARN, INFO, DEBUG
  - Environment variable: `ZELDA3_LOG_LEVEL`
  - ANSI colors (auto-detected TTY support)
  - Platform abstraction: Windows (_isatty) vs Unix (isatty)
  - No modules, no files - just stderr with colors
- **Fix**: ✅ Uses fprintf(stderr) - portable to Android/Switch via their stdio

### [X] 12. Replace fprintf in config.c with logging
- **Count**: 11 instances in config.c replaced
- **Macros**: `LogError()`, `LogWarn()` - simple, no module parameter
- **Decision**: Only replaced config.c (YAGNI - don't need all 96+ calls)
- **Fix**: ✅ Added InitializeLogging() to main.c

### [ ] 13. Consolidate ReadWholeFile implementations
- **Issue**: Two implementations - `platform.c:75-112` and `util.c:41-57`
- **Action**:
  - Keep platform.c version as canonical
  - Remove util.c duplicate
  - Update all call sites in main.c (lines 768, 813, 820)

### [ ] 14. Create memory allocation helpers
- **Create**: `src/memory.h`
- **Issue**: Duplicated growing array patterns:
  - config.c keymaps (lines 74-96) and joymap (lines 168-186)
  - glsl_shader.c linked lists (lines 48-95)
  - util.c ByteArray (lines 147-173)
- **Provide**: Standard dynamic array and pool allocators

### [ ] 15. Split sprite_main.c (808KB → category files)
- **Current**: 25,878 lines, all enemy AI in one file
- **Split into**:
  - `src/sprites/basic_enemies.c` - Simple enemies (soldiers, keese, etc.)
  - `src/sprites/bosses.c` - Boss AI
  - `src/sprites/overworld.c` - Overworld-specific sprites
  - `src/sprites/dungeon.c` - Dungeon-specific sprites
  - Keep `sprite_main.c` as dispatcher with function pointer tables

### [ ] 16. Split player.c (211KB → modules)
- **Current**: 6,664 lines, mix of state management and actions
- **Split into**:
  - `src/player/player_state.c` - State machine, movement, physics
  - `src/player/player_actions.c` - Item usage, attacks, interactions
  - `src/player/player_input.c` - Input handling and response
  - Keep `player.c` as coordinator

### [ ] 17. Split config.c (parsing + mapping)
- **Current**: Mix of gamepad/keyboard mapping (lines 1-285) and INI parsing (lines 286-562)
- **Split into**:
  - `src/input_mapping.c` - Keyboard and gamepad binding logic
  - `src/config_parser.c` - INI parsing utilities
  - Keep `config.c` minimal for config storage

### [ ] 18. Create centralized math utilities
- **Create**: `src/math_util.h`
- **Consolidate**:
  - types.h: abs8, abs16, IntMin, IntMax (lines 48-53)
  - main.c: ApproximateAtan2 (lines 712-728)
  - Various bit counting, fixed-point math scattered across files

### [ ] 19. Create platform detection header
- **Create**: `src/platform_detect.h`
- **Issue**: `#ifdef _WIN32`, `#ifdef __APPLE__`, `#ifdef __SWITCH__` scattered throughout
- **Provide**:
  ```c
  #define PLATFORM_WINDOWS
  #define PLATFORM_MACOS
  #define PLATFORM_LINUX
  #define PLATFORM_SWITCH
  ```

---

## 📝 INDIVIDUAL FILE REFACTORING

### [ ] 20. Refactor config.c HandleIniConfig
- **File**: `src/config.c:346-513`
- **Issue**: 168 lines, ~50+ cyclomatic complexity, 6 config sections mixed
- **Action**: Extract section handlers:
  ```c
  static bool HandleGraphicsConfig(const char *key, char *value);
  static bool HandleSoundConfig(const char *key, char *value);
  static bool HandleFeaturesConfig(const char *key, char *value);
  // etc.
  ```

### [ ] 21. Refactor main.c ChangeWindowScale
- **File**: `src/main.c:79-117` (39 lines)
- **Issue**: Does too much - bounds calc, window size, position, mouse tracking
- **Action**: Extract:
  ```c
  static int CalculateMaxScale(int screen_index);
  static void CenterWindowOnMouse(SDL_Window *window, int w, int h, SDL_Rect bounds);
  ```

### [ ] 22. Refactor main.c HandleGamepadAxisInput
- **File**: `src/main.c:709-764` (56 lines)
- **Issue**: Complex deadzone + angle calculation with magic numbers
- **Action**: Extract:
  ```c
  static float CalculateAnalogAngle(int x, int y);
  ```
- Define constants for: `10000 * 10000`, `64.0f`, `16`, `0.5f`

### [ ] 23. Refactor glsl_shader.c GlslShader_CreateFromFile
- **File**: `src/glsl_shader.c:361-464` (104 lines)
- **Issue**: Initialization + compilation + linking all in one function
- **Action**: Break into:
  ```c
  static void GlslShader_Initialize(const char *filename);
  static bool GlslShader_CompileShaders(GlslShader *gs);
  static bool GlslShader_LinkPrograms(GlslShader *gs);
  static void GlslShader_LoadTextures(GlslShader *gs);
  ```

### [ ] 24. Extract magic numbers in main.c
- **Locations**:
  - Lines 318-327: Audio sample rates → `kMinAudioFreq`, `kMaxAudioFreq`, `kDefaultSamples`
  - Line 742: `10000 * 10000` → `kGamepadDeadzoneSquared`
  - Line 756: `64.0f` → `kAngleSegments`

### [ ] 25. Extract magic numbers in config.c
- **Location**: Lines 455-460
- **Issue**: Aspect ratio formulas hardcoded
- **Action**: Create const table:
  ```c
  static const AspectRatio kAspectRatios[] = {
    {"16:9", 16, 9}, {"16:10", 16, 10}, {"4:3", 4, 3}, ...
  };
  ```

---

## 🖥️ PLATFORM ABSTRACTION

### [ ] 26. Extend platform.h with path utilities
- **File**: `src/platform.h`
- **Add**:
  ```c
  char *Platform_JoinPath(const char *base, const char *rel);
  const char *Platform_GetFileName(const char *path);
  bool Platform_PathExists(const char *path);
  bool Platform_IsDirectory(const char *path);
  void Platform_NormalizePath(char *path);  // Convert to platform separators
  ```

### [ ] 27. Create input abstraction layer
- **Create**: `src/input.h`
- **Issue**: All code directly calls SDL, no abstraction for native platform APIs
- **API**:
  ```c
  typedef struct Input {
    void (*Init)(void);
    int (*GetGamepadCount)(void);
    bool (*GetButton)(int gamepad, int button);
    void (*GetAnalog)(int gamepad, int axis, int *x, int *y);
    uint32 (*GetKeyState)(void);
  } InputAPI;

  extern const InputAPI g_input_sdl;
  #ifdef _WIN32
  extern const InputAPI g_input_xinput;
  #endif
  ```

### [ ] 28. Create audio platform abstraction
- **Create**: `src/audio_platform.h`
- **Issue**: All audio through SDL, no native API support (WASAPI, CoreAudio)
- **Action**: Create audio mixer interface with platform implementations

### [ ] 29. Migrate MSU audio to Platform_* functions
- **File**: `src/audio.c:30-42`
- **Issue**: Uses stdio `FILE*` directly instead of `Platform_OpenFile`
- **Action**: Convert to platform abstraction

### [ ] 30. Add threading/mutex abstractions
- **File**: `src/platform.h`
- **Issue**: Only SDL_mutex used (main.c lines 174, 180, 360, etc.)
- **Add**:
  ```c
  typedef struct PlatformMutex PlatformMutex;
  PlatformMutex *Platform_CreateMutex(void);
  void Platform_LockMutex(PlatformMutex *m);
  void Platform_UnlockMutex(PlatformMutex *m);
  void Platform_DestroyMutex(PlatformMutex *m);
  ```

### [ ] 31. Add time/sleep abstractions
- **File**: `src/platform.h`
- **Issue**: SDL-only timing (SDL_GetTicks, SDL_Delay, etc.)
- **Add**:
  ```c
  uint64 Platform_GetTicks(void);  // milliseconds
  uint64 Platform_GetPerformanceCounter(void);
  uint64 Platform_GetPerformanceFrequency(void);
  void Platform_Sleep(uint32 ms);
  ```

---

## 🐛 TECHNICAL DEBT

### [ ] 32. Review and fix known OOB array access bugs
- **Count**: 18+ instances with "bug" comments
- **Examples**:
  - `src/sprite_main.c:1013` - "zelda bug: carry" (signed integer issue)
  - `src/player.c:5292` - "Bug in zelda, might read index 15"
  - `src/sprite_main.c:24594` - "zelda bug: OOB read"
- **Action**: Audit each, add bounds checking or fix logic

### [ ] 33. Add type-safe accessors for SNES variables
- **Issue**: Memory-mapped names unclear: `byte_7FFE01`, `word_7FFE00`, `byte_7E004E`
- **Action**: Add wrapper functions while preserving compatibility:
  ```c
  static inline uint8 GetSpriteTemp1() { return byte_7FFE01; }
  static inline void SetSpriteTemp1(uint8 v) { byte_7FFE01 = v; }
  ```

---

## 📊 STATISTICS

- **Total Items**: 33
- **Critical Security**: 10
- **Multi-file Refactoring**: 9
- **Individual File Refactoring**: 6
- **Platform Abstraction**: 6
- **Technical Debt**: 2

## 🎯 RECOMMENDED ORDER

1. **Phase 1 - Security** (Items 1-10): Fix critical bugs first
2. **Phase 2 - Infrastructure** (Items 11-14): Logging + shared utilities
3. **Phase 3 - Organization** (Items 15-19): File splitting + module boundaries
4. **Phase 4 - Quality** (Items 20-25): Refactor complex functions
5. **Phase 5 - Portability** (Items 26-31): Platform abstraction completeness
6. **Phase 6 - Cleanup** (Items 32-33): Technical debt

## 📝 NOTES

- This analysis covers ~70-80kLOC of game logic code
- Codebase is well-structured for reverse-engineered code
- Original behavior must be preserved (replay compatibility)
- Enhanced features must remain optional and off by default
- All changes should be testable with `zelda3_assets.dat`
