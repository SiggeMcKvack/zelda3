# Critical Fixes Quick Reference Guide

**Purpose:** Step-by-step instructions to fix the 5 critical issues identified in the code quality analysis.
**Estimated Time:** 4-6 hours total
**Difficulty:** Easy to Medium
**Impact:** Prevents crashes, memory corruption, and visual bugs

---

## 🚨 Before You Start

### Prerequisites
- Text editor or IDE
- C compiler (GCC, Clang, or MSVC)
- Git for version control
- Basic understanding of C

### Backup
```bash
git checkout -b fix/critical-issues
git commit -m "Starting critical fixes"
```

### Testing Setup
```bash
# Compile with sanitizers for testing
make clean
CFLAGS="-O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer" make
```

---

## Fix #1: Viewport Calculation Bug

### 📍 Location
**File:** `src/opengl.c`
**Line:** 211
**Severity:** 🔴 Critical
**Time:** 2 minutes

### 🐛 The Bug
The viewport Y position is calculated incorrectly, resulting in the game window not being centered vertically.

### 📝 Step-by-Step Fix

1. **Open the file:**
```bash
vim src/opengl.c +211
# or use your preferred editor
```

2. **Locate the buggy code** (around line 211):
```c
static void OpenGLRenderer_EndDraw() {
  int drawable_width, drawable_height;

  SDL_GL_GetDrawableSize(g_window, &drawable_width, &drawable_height);

  int viewport_width = drawable_width, viewport_height = drawable_height;

  if (!g_config.ignore_aspect_ratio) {
    if (viewport_width * g_draw_height < viewport_height * g_draw_width)
      viewport_height = viewport_width * g_draw_height / g_draw_width;
    else
      viewport_width = viewport_height * g_draw_width / g_draw_height;
  }

  int viewport_x = (drawable_width - viewport_width) >> 1;
  int viewport_y = (viewport_height - viewport_height) >> 1;  // ← BUG HERE
```

3. **Change line 211 from:**
```c
int viewport_y = (viewport_height - viewport_height) >> 1;
```

**To:**
```c
int viewport_y = (drawable_height - viewport_height) >> 1;
```

4. **Verify the fix:**
The corrected section should look like:
```c
  int viewport_x = (drawable_width - viewport_width) >> 1;
  int viewport_y = (drawable_height - viewport_height) >> 1;  // ✓ Fixed
```

### ✅ Testing

1. **Recompile:**
```bash
make clean && make
```

2. **Visual Test:**
```bash
./zelda3
```
- Window should now be centered vertically
- Resize window - content should stay centered

3. **Aspect Ratio Test:**
- Try different window sizes
- Check fullscreen mode
- Verify no black bars in wrong places

### 📊 Expected Result
- Window content properly centered in both directions
- No visual glitches when resizing

---

## Fix #2: Unchecked Memory Allocations

### 📍 Locations
**Multiple files** - 10+ occurrences
**Severity:** 🔴 Critical
**Time:** 2-3 hours

### 🐛 The Bug
Memory allocations with `malloc()`, `calloc()`, or `realloc()` don't check for NULL return values. If allocation fails (low memory), the program will crash.

### 📝 Files to Fix

#### 2.1 Fix `src/opengl.c:186-188`

**Open the file:**
```bash
vim src/opengl.c +186
```

**Locate this code:**
```c
static void OpenGLRenderer_BeginDraw(int width, int height, uint8 **pixels, int *pitch) {
  int size = width * height;

  if (size > g_screen_buffer_size) {
    g_screen_buffer_size = size;
    free(g_screen_buffer);
    g_screen_buffer = malloc(size * 4);  // ← No null check!
  }

  g_draw_width = width;
  g_draw_height = height;
  *pixels = g_screen_buffer;
  *pitch = width * 4;
}
```

**Change to:**
```c
static void OpenGLRenderer_BeginDraw(int width, int height, uint8 **pixels, int *pitch) {
  int size = width * height;

  if (size > g_screen_buffer_size) {
    g_screen_buffer_size = size;
    free(g_screen_buffer);
    g_screen_buffer = malloc(size * 4);
    if (!g_screen_buffer) {  // ✓ Added check
      Die("Failed to allocate screen buffer: %d bytes", size * 4);
    }
  }

  g_draw_width = width;
  g_draw_height = height;
  *pixels = g_screen_buffer;
  *pitch = width * 4;
}
```

#### 2.2 Fix `src/main.c:376`

**Open the file:**
```bash
vim src/main.c +376
```

**Locate:**
```c
g_audiobuffer = malloc(g_frames_per_block * have.channels * sizeof(int16));
```

**Change to:**
```c
g_audiobuffer = malloc(g_frames_per_block * have.channels * sizeof(int16));
if (!g_audiobuffer) {
  Die("Failed to allocate audio buffer");
}
```

#### 2.3 Fix `src/util.c:48`

**Open the file:**
```bash
vim src/util.c +48
```

**Locate:**
```c
uint8 *ReadWholeFile(const char *name, size_t *length) {
  FILE *f = fopen(name, "rb");
  if (f == NULL)
    return NULL;
  fseek(f, 0, SEEK_END);
  size_t size = ftell(f);
  rewind(f);
  uint8 *buffer = (uint8 *)malloc(size + 1);  // ← No check
  if (!buffer) Die("malloc failed");  // Actually, this one is checked!
```

**Status:** Already has check - no change needed ✓

#### 2.4 Fix `src/util.c:244`

**Locate:**
```c
uint8 *ApplyBps(...) {
  // ...
  uint8 *dst = malloc(dst_size);  // ← No check
  if (!dst)
    return NULL;
```

**Status:** Already has check - no change needed ✓

#### 2.5 Fix `src/glsl_shader.c:52`

**Open the file:**
```bash
vim src/glsl_shader.c +52
```

**Locate:**
```c
static void ParseTextures(GlslShader *gs, char *value) {
  char *id;
  GlslTexture **nextp = &gs->first_texture;
  for (int num = 0; (id = NextDelim(&value, ';')) != NULL && num < kGlslMaxTextures; num++) {
    GlslTexture *t = calloc(sizeof(GlslTexture), 1);  // ← No check
    t->id = strdup(id);
```

**Change to:**
```c
static void ParseTextures(GlslShader *gs, char *value) {
  char *id;
  GlslTexture **nextp = &gs->first_texture;
  for (int num = 0; (id = NextDelim(&value, ';')) != NULL && num < kGlslMaxTextures; num++) {
    GlslTexture *t = calloc(sizeof(GlslTexture), 1);
    if (!t) {
      Die("Failed to allocate texture structure");
    }
    t->id = strdup(id);
    if (!t->id) {
      free(t);
      Die("Failed to allocate texture ID string");
    }
```

#### 2.6 Fix `src/glsl_shader.c:87-89`

**Locate:**
```c
static GlslParam *GlslShader_GetParam(GlslShader *gs, const char *id) {
  GlslParam **pp = &gs->first_param;
  for (; (*pp) != NULL; pp = &(*pp)->next)
    if (!strcmp((*pp)->id, id))
      return *pp;
  GlslParam *p = (GlslParam *)calloc(1, sizeof(GlslParam));  // ← No check
  *pp = p;
  p->id = strdup(id);
  return p;
}
```

**Change to:**
```c
static GlslParam *GlslShader_GetParam(GlslShader *gs, const char *id) {
  GlslParam **pp = &gs->first_param;
  for (; (*pp) != NULL; pp = &(*pp)->next)
    if (!strcmp((*pp)->id, id))
      return *pp;
  GlslParam *p = (GlslParam *)calloc(1, sizeof(GlslParam));
  if (!p) {
    Die("Failed to allocate parameter structure");
  }
  *pp = p;
  p->id = strdup(id);
  if (!p->id) {
    free(p);
    *pp = NULL;
    Die("Failed to allocate parameter ID string");
  }
  return p;
}
```

#### 2.7 Fix `src/glsl_shader.c:112`

**Locate:**
```c
static void GlslShader_InitializePasses(GlslShader *gs, int passes) {
  gs->n_pass = passes;
  gs->pass = (GlslPass *)calloc(gs->n_pass + 1, sizeof(GlslPass));  // ← No check
  for (int i = 0; i < gs->n_pass; i++)
    GlslPass_Initialize(gs->pass + i + 1);
}
```

**Change to:**
```c
static void GlslShader_InitializePasses(GlslShader *gs, int passes) {
  gs->n_pass = passes;
  gs->pass = (GlslPass *)calloc(gs->n_pass + 1, sizeof(GlslPass));
  if (!gs->pass) {
    Die("Failed to allocate shader passes: %d passes", passes);
  }
  for (int i = 0; i < gs->n_pass; i++)
    GlslPass_Initialize(gs->pass + i + 1);
}
```

#### 2.8 Fix `snes/ppu.c:37`

**Open the file:**
```bash
vim snes/ppu.c +37
```

**Locate:**
```c
Ppu* ppu_init() {
  Ppu* ppu = (Ppu * )malloc(sizeof(Ppu));  // ← No check
  ppu->extraLeftRight = kPpuExtraLeftRight;
  return ppu;
}
```

**Change to:**
```c
Ppu* ppu_init() {
  Ppu* ppu = (Ppu *)malloc(sizeof(Ppu));
  if (!ppu) {
    Die("Failed to allocate PPU structure");
  }
  ppu->extraLeftRight = kPpuExtraLeftRight;
  return ppu;
}
```

### ✅ Testing

1. **Compile with sanitizers:**
```bash
make clean
CFLAGS="-O0 -g -fsanitize=address,undefined" make
```

2. **Run normally:**
```bash
./zelda3
```
Should start without errors

3. **Low memory stress test** (optional, requires special setup):
```bash
# Linux: Use ulimit to limit memory
ulimit -v 100000  # Limit to ~100MB
./zelda3
# Should exit gracefully with error message, not crash
```

4. **Valgrind memory check:**
```bash
valgrind --leak-check=full ./zelda3
# Exit after a few seconds
# Look for any "invalid write" or "invalid read" errors
```

### 📊 Expected Result
- All allocations checked
- Graceful error messages on allocation failure
- No crashes with sanitizers
- Clean valgrind output

---

## Fix #3: Use-After-Free in Shader System

### 📍 Location
**File:** `src/glsl_shader.c`
**Lines:** 643-658
**Severity:** 🔴 Critical
**Time:** 30 minutes

### 🐛 The Bug
When using shader frame history, there's a potential use-after-free if the store and load positions are the same.

### 📝 Step-by-Step Fix

1. **Open the file:**
```bash
vim src/glsl_shader.c +643
```

2. **Locate the buggy code:**
```c
void GlslShader_Render(GlslShader *gs, GlTextureWithSize *tex, int viewport_x, int viewport_y, int viewport_width, int viewport_height) {
  // ... earlier code ...

  glBindFramebuffer(GL_FRAMEBUFFER, previous_framebuffer);
  glUseProgram(0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // Store the input frame in the prev array, and extract the next one.
  if (gs->max_prev_frame != 0) {
    // 01234567
    //    43210
    // ^-- store pos
    //    ^-- load pos
    GlTextureWithSize *store_pos = &gs->prev_frame[gs->frame_count & 7];
    GlTextureWithSize *load_pos = &gs->prev_frame[gs->frame_count - gs->max_prev_frame & 7];
    assert(store_pos->gl_texture == 0);
    *store_pos = *tex;
    *tex = *load_pos;
    memset(load_pos, 0, sizeof(GlTextureWithSize));  // ← BUG: zeroes data that tex points to!
  }

  gs->frame_count++;
}
```

3. **Replace the entire section with:**
```c
  glBindFramebuffer(GL_FRAMEBUFFER, previous_framebuffer);
  glUseProgram(0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // Store the input frame in the prev array, and extract the next one.
  if (gs->max_prev_frame != 0) {
    // 01234567
    //    43210
    // ^-- store pos
    //    ^-- load pos
    GlTextureWithSize *store_pos = &gs->prev_frame[gs->frame_count & 7];
    GlTextureWithSize *load_pos = &gs->prev_frame[(gs->frame_count - gs->max_prev_frame) & 7];

    // Safety check: ensure store and load positions are different
    if (store_pos != load_pos) {
      assert(store_pos->gl_texture == 0);
      *store_pos = *tex;
      *tex = *load_pos;
      memset(load_pos, 0, sizeof(GlTextureWithSize));
    }
  }

  gs->frame_count++;
}
```

### ✅ Testing

1. **Recompile:**
```bash
make clean && make
```

2. **Test with shaders that use frame history:**
```bash
# If you have RetroArch shaders that use previous frames
./zelda3 --shader=path/to/motion-blur.glsl
```

3. **Run with sanitizers:**
```bash
CFLAGS="-fsanitize=address,undefined" make
./zelda3
# Load a game, play for a few minutes
# No "heap-use-after-free" errors should appear
```

### 📊 Expected Result
- No memory corruption
- Shaders with frame history work correctly
- Clean sanitizer output

---

## Fix #4: Integer Overflow in File I/O

### 📍 Location
**File:** `src/util.c`
**Lines:** 41-57
**Severity:** 🔴 Critical
**Time:** 45 minutes

### 🐛 The Bug
Using `ftell()` which returns `long` (32-bit on Windows) causes overflow for files larger than 2GB.

### 📝 Step-by-Step Fix

1. **Open the file:**
```bash
vim src/util.c +41
```

2. **Locate the function:**
```c
uint8 *ReadWholeFile(const char *name, size_t *length) {
  FILE *f = fopen(name, "rb");
  if(!file) Die("Failed to read file");
  fseek(f, 0, SEEK_END);
  size_t size = ftell(f);  // ← BUG: ftell returns long (32-bit on Win32)
  rewind(f);
  uint8 *buffer = (uint8 *)malloc(size + 1);
  if (!buffer) Die("malloc failed");
  buffer[size] = 0;
  if (fread(buffer, 1, size, f) != size)
    Die("fread failed");
  fclose(f);
  if (length) *length = size;
  return buffer;
}
```

3. **Replace the entire function with:**
```c
uint8 *ReadWholeFile(const char *name, size_t *length) {
  FILE *f = fopen(name, "rb");
  if (f == NULL)
    return NULL;

  // Use 64-bit file operations
#ifdef _WIN32
  _fseeki64(f, 0, SEEK_END);
  __int64 size64 = _ftelli64(f);
  _fseeki64(f, 0, SEEK_SET);
#else
  fseeko(f, 0, SEEK_END);
  off_t size64 = ftello(f);
  fseeko(f, 0, SEEK_SET);
#endif

  if (size64 < 0) {
    fclose(f);
    return NULL;
  }

  size_t size = (size_t)size64;
  uint8 *buffer = (uint8 *)malloc(size + 1);
  if (!buffer) {
    fclose(f);
    Die("malloc failed: %zu bytes", size + 1);
  }

  // Always zero terminate
  buffer[size] = 0;

  if (fread(buffer, 1, size, f) != size) {
    free(buffer);
    fclose(f);
    Die("fread failed");
  }

  fclose(f);
  if (length) *length = size;
  return buffer;
}
```

### ✅ Testing

1. **Recompile:**
```bash
make clean && make
```

2. **Test with normal files:**
```bash
./zelda3
# Should load assets normally
```

3. **Test with large file (optional):**
```bash
# Create a 3GB test file
dd if=/dev/zero of=large_test_file.dat bs=1M count=3000

# Try to load it (will fail gracefully, not crash)
# Modify code temporarily to call ReadWholeFile on this file
```

4. **Check for warnings:**
```bash
# Recompile with warnings
CFLAGS="-Wall -Wextra" make 2>&1 | grep -i "conversion\|overflow"
# Should see no conversion warnings
```

### 📊 Expected Result
- Can handle files up to system limit (not just 2GB)
- No integer overflow warnings
- Proper error handling

---

## Fix #5: Division by Zero

### 📍 Location
**File:** `src/main.c`
**Lines:** 203-208
**Severity:** 🔴 Critical
**Time:** 15 minutes

### 🐛 The Bug
Aspect ratio calculation divides by `g_draw_width` and `g_draw_height` without checking if they're zero.

### 📝 Step-by-Step Fix

1. **Open the file:**
```bash
vim src/main.c +203
```

2. **Locate the function:**
```c
static void SdlRenderer_BeginDraw(int width, int height, uint8 **pixels, int *pitch) {
  g_sdl_renderer_rect.w = width;
  g_sdl_renderer_rect.h = height;
  // ...
}

static void OpenGLRenderer_EndDraw() {
  int drawable_width, drawable_height;

  SDL_GL_GetDrawableSize(g_window, &drawable_width, &drawable_height);

  int viewport_width = drawable_width, viewport_height = drawable_height;

  if (!g_config.ignore_aspect_ratio) {
    if (viewport_width * g_draw_height < viewport_height * g_draw_width)
      viewport_height = viewport_width * g_draw_height / g_draw_width;  // ← Div by 0!
    else
      viewport_width = viewport_height * g_draw_width / g_draw_height;   // ← Div by 0!
  }
```

3. **Replace the aspect ratio section with:**
```c
static void OpenGLRenderer_EndDraw() {
  int drawable_width, drawable_height;

  SDL_GL_GetDrawableSize(g_window, &drawable_width, &drawable_height);

  int viewport_width = drawable_width, viewport_height = drawable_height;

  if (!g_config.ignore_aspect_ratio) {
    // Validate dimensions before division
    if (g_draw_width == 0 || g_draw_height == 0) {
      // Invalid dimensions - use full viewport
      fprintf(stderr, "Warning: Invalid draw dimensions (%dx%d), using full viewport\n",
              g_draw_width, g_draw_height);
    } else {
      // Safe to divide
      if (viewport_width * g_draw_height < viewport_height * g_draw_width)
        viewport_height = viewport_width * g_draw_height / g_draw_width;
      else
        viewport_width = viewport_height * g_draw_width / g_draw_height;
    }
  }
```

4. **Also add validation in BeginDraw:**

Find `OpenGLRenderer_BeginDraw` (around line 181) and add:
```c
static void OpenGLRenderer_BeginDraw(int width, int height, uint8 **pixels, int *pitch) {
  // Validate dimensions
  if (width <= 0 || height <= 0) {
    fprintf(stderr, "Error: Invalid dimensions %dx%d\n", width, height);
    width = 256;   // Fallback to SNES native
    height = 224;
  }
  if (width > 4096 || height > 4096) {
    fprintf(stderr, "Error: Dimensions too large %dx%d\n", width, height);
    width = 1024;
    height = 1024;
  }

  int size = width * height;
  // ... rest of function
```

### ✅ Testing

1. **Recompile:**
```bash
make clean && make
```

2. **Normal operation test:**
```bash
./zelda3
# Should work normally
```

3. **Edge case test** (requires code modification):
```c
// Temporarily in main.c after BeginDraw:
g_draw_width = 0;
g_draw_height = 0;
// Or call with negative values
```

4. **Test with debugger:**
```bash
gdb ./zelda3
(gdb) break OpenGLRenderer_EndDraw
(gdb) run
(gdb) print g_draw_width
(gdb) print g_draw_height
# Verify they're never 0 during operation
(gdb) continue
```

### 📊 Expected Result
- No division by zero crashes
- Graceful handling of invalid dimensions
- Warning messages logged

---

## 🧪 Complete Testing Suite

After applying all fixes, run this complete test:

### 1. Compilation Test
```bash
# Clean build
make clean

# Build with all warnings
CFLAGS="-Wall -Wextra -Werror -O2" make

# Should compile without errors or warnings
```

### 2. Sanitizer Test
```bash
# Clean and rebuild with sanitizers
make clean
CFLAGS="-O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer" make

# Run the game
./zelda3

# Play for 5-10 minutes
# Try different screens (menu, gameplay, different areas)
# Exit cleanly

# Check for any sanitizer errors in output
```

### 3. Memory Leak Test
```bash
# Run with Valgrind (Linux only)
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./zelda3

# Play for a few minutes, then exit

# Check the summary:
# - "definitely lost" should be 0 bytes
# - "possibly lost" should be minimal
# - "still reachable" is okay (some SDL/GL cleanup)
```

### 4. Visual Regression Test
```bash
# Run normally
./zelda3

# Check:
- [ ] Window centered properly
- [ ] Aspect ratio correct
- [ ] Resizing works smoothly
- [ ] Fullscreen works
- [ ] No graphical glitches
- [ ] Shaders work (if configured)
```

### 5. Platform Test
Test on multiple platforms if possible:
- [ ] Windows (MSVC build)
- [ ] Linux (GCC build)
- [ ] macOS (Clang build)
- [ ] Nintendo Switch (DevKitPro build)

---

## 📋 Checklist

Use this checklist to track your progress:

### Fix #1: Viewport Bug
- [ ] Located bug in `src/opengl.c:211`
- [ ] Fixed `viewport_y` calculation
- [ ] Compiled successfully
- [ ] Visual test passed
- [ ] Git commit made

### Fix #2: Memory Allocations
- [ ] Fixed `src/opengl.c:186`
- [ ] Fixed `src/main.c:376`
- [ ] Fixed `src/glsl_shader.c:52`
- [ ] Fixed `src/glsl_shader.c:87`
- [ ] Fixed `src/glsl_shader.c:112`
- [ ] Fixed `snes/ppu.c:37`
- [ ] Compiled with sanitizers
- [ ] Memory tests passed
- [ ] Git commit made

### Fix #3: Use-After-Free
- [ ] Located bug in `src/glsl_shader.c:643-658`
- [ ] Added safety check
- [ ] Compiled with sanitizers
- [ ] Shader tests passed
- [ ] Git commit made

### Fix #4: Integer Overflow
- [ ] Fixed `src/util.c:41-57`
- [ ] Added 64-bit file operations
- [ ] Tested with normal files
- [ ] No conversion warnings
- [ ] Git commit made

### Fix #5: Division by Zero
- [ ] Fixed `src/main.c:203-208`
- [ ] Added dimension validation
- [ ] Tested edge cases
- [ ] No crashes observed
- [ ] Git commit made

### Final Testing
- [ ] All fixes compiled without warnings
- [ ] Sanitizer tests clean
- [ ] Valgrind tests clean
- [ ] Visual regression tests passed
- [ ] Cross-platform tests passed (if applicable)
- [ ] Documentation updated
- [ ] Pull request created

---

## 🎯 Git Workflow

### Commit Messages

Use clear, descriptive commit messages for each fix:

```bash
# Fix #1
git add src/opengl.c
git commit -m "Fix viewport calculation bug

The viewport Y position was incorrectly calculated as
(viewport_height - viewport_height) which always equals 0.

Fixed by using (drawable_height - viewport_height) to properly
center the viewport vertically.

Fixes: Viewport not centered vertically
File: src/opengl.c:211"

# Fix #2
git add src/opengl.c src/main.c src/glsl_shader.c snes/ppu.c
git commit -m "Add null checks for memory allocations

Added null pointer checks after malloc/calloc calls in:
- src/opengl.c:186 (screen buffer)
- src/main.c:376 (audio buffer)
- src/glsl_shader.c:52, 87, 112 (shader structures)
- snes/ppu.c:37 (PPU structure)

All failures now result in graceful error messages via Die().

Fixes: Potential crashes on low-memory systems"

# Continue for other fixes...
```

### Creating Pull Request

```bash
# Push your branch
git push origin fix/critical-issues

# Create PR with this description:
```

**PR Title:** Fix 5 critical bugs (viewport, memory, use-after-free, overflow, div-by-zero)

**Description:**
This PR fixes 5 critical bugs identified in the code quality analysis:

1. **Viewport Calculation Bug** (`src/opengl.c:211`)
   - Fixed incorrect Y-axis centering
   - Viewport now properly centered vertically

2. **Unchecked Memory Allocations** (multiple files)
   - Added null checks after malloc/calloc
   - Graceful error messages on allocation failure
   - Fixes potential crashes on low-memory systems

3. **Use-After-Free in Shader System** (`src/glsl_shader.c:643-658`)
   - Added safety check for frame history
   - Prevents memory corruption with shader effects

4. **Integer Overflow in File I/O** (`src/util.c:41-57`)
   - Replaced ftell/fseek with 64-bit equivalents
   - Can now handle files larger than 2GB

5. **Division by Zero** (`src/main.c:203-208`)
   - Added dimension validation before division
   - Prevents crashes on invalid dimensions

**Testing:**
- ✅ Compiles without warnings (`-Wall -Wextra -Werror`)
- ✅ Sanitizers clean (`-fsanitize=address,undefined`)
- ✅ Valgrind clean (no leaks)
- ✅ Visual regression tests passed
- ✅ Cross-platform tested (Windows/Linux/macOS)

**Impact:**
- Prevents crashes and memory corruption
- Improves stability and reliability
- No performance impact
- No breaking changes

---

## 📞 Getting Help

If you encounter issues:

1. **Check the detailed analysis:**
   - `docs/code-quality-analysis.md` (full issue descriptions)
   - `docs/README.md` (overview and resources)

2. **Ask on Discord:**
   - https://discord.gg/AJJbJAzNNJ

3. **Create a GitHub Issue:**
   - https://github.com/snesrev/zelda3/issues
   - Include error messages and sanitizer output

4. **Reference this guide:**
   - Link to specific fix sections
   - Provide context on what you've tried

---

## ✅ Success Criteria

You've successfully completed the critical fixes when:

✅ All 5 fixes applied
✅ Compiles without warnings with `-Wall -Wextra -Werror`
✅ Runs without sanitizer errors
✅ Valgrind shows no memory leaks
✅ Visual tests show no regressions
✅ Game plays normally for 10+ minutes
✅ All tests in checklist completed
✅ Git commits made with clear messages
✅ Changes pushed to branch

**Congratulations!** You've made Zelda3 significantly more stable and reliable!

---

**Document Version:** 1.0
**Last Updated:** November 2025
**Estimated Total Time:** 4-6 hours
**Difficulty:** Easy to Medium
**Impact:** High - Prevents crashes and memory corruption
