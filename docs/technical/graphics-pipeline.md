# Graphics Pipeline

[Home](../index.md) > [Architecture](../architecture.md) > Graphics Pipeline

## Overview

Zelda3's graphics pipeline emulates the SNES Picture Processing Unit (PPU) to render authentic graphics, then outputs through modern renderers (SDL, OpenGL, OpenGL ES, or Vulkan). This design preserves pixel-perfect accuracy while enabling modern features like shaders and high-resolution output.

## Pipeline Flow

```
Frame Start (60 FPS)
  │
  ├─ Load Graphics (load_gfx.c)
  │   ├─ Decompress tile data from assets
  │   ├─ Upload to VRAM (g_zenv.vram)
  │   └─ Update palette (CGRAM)
  │
  ├─ Update OAM (Object Attribute Memory)
  │   ├─ player_oam.c - Link sprites
  │   ├─ sprite.c - Enemy/NPC sprites
  │   └─ ancilla.c - Projectile sprites
  │
  ├─ PPU Rendering (snes/ppu.c)
  │   ├─ Clear frame buffer
  │   ├─ Render background layers (BG1-BG4)
  │   ├─ Render sprites (64 max)
  │   ├─ Apply Mode 7 (rotation/scaling)
  │   ├─ Color math (transparency, brightness)
  │   └─ Generate RGB frame buffer
  │
  └─ Renderer Output
      ├─ BeginDraw(width, height, &pixels, &pitch)
      ├─ Copy PPU buffer to renderer texture
      ├─ Optional: Apply GLSL shaders
      ├─ Scale to window size
      └─ EndDraw() / Present
```

## PPU Rendering (snes/ppu.c)

The PPU emulates the SNES graphics chip with pixel-perfect accuracy.

### PPU State Structure

```c
typedef struct Ppu {
  // VRAM - Tile graphics storage
  uint8 vram[0x10000];  // 64KB

  // CGRAM - Color palette (512 colors, 15-bit RGB)
  uint16 cgram[256];

  // OAM - Sprite attribute memory
  uint8 oam[544];       // 128 sprites × 4 bytes + extension

  // Registers
  uint8 brightness;     // Screen brightness (0-15)
  uint8 bgMode;         // Background mode (0-7)
  uint8 mosaic;         // Mosaic effect size

  // Layer state
  uint8 screenEnabled[2];   // Main/sub screen layer enable
  uint8 screenWindowed[2];  // Window masks per layer

  // Output buffer (with widescreen support)
  uint8 *pixelBuffer;   // RGB pixels
  int extraLeftRight;   // Widescreen padding (kPpuExtraLeftRight)
} Ppu;
```

### Background Layers

The SNES supports 4 background layers (BG1-BG4) with different modes:

**Mode 0:** 4 layers, 2bpp each (4 colors per tile)
**Mode 1:** 2 layers 4bpp (16 colors), 1 layer 2bpp
**Mode 2:** 2 layers 4bpp with per-tile scrolling (offset-per-tile)
**Mode 7:** Single layer with rotation/scaling (affine transformations)

```c
// Render background layer
static int ppu_getPixelForBgLayer(Ppu *ppu, int x, int y, int layer, bool priority) {
  // 1. Calculate tile position
  int tile_x = x / 8;
  int tile_y = y / 8;

  // 2. Fetch tile from tilemap (in VRAM)
  uint16 tile_data = ReadTilemap(ppu, layer, tile_x, tile_y);

  // 3. Extract tile properties
  int tile_num = tile_data & 0x3FF;
  int palette = (tile_data >> 10) & 7;
  bool h_flip = tile_data & 0x4000;
  bool v_flip = tile_data & 0x8000;

  // 4. Fetch pixel from tile graphics
  int pixel_x = x % 8;
  int pixel_y = y % 8;
  if (h_flip) pixel_x = 7 - pixel_x;
  if (v_flip) pixel_y = 7 - pixel_y;

  uint8 color_index = GetTilePixel(ppu, tile_num, pixel_x, pixel_y);

  // 5. Look up color in palette
  if (color_index == 0) return -1;  // Transparent
  return ppu->cgram[palette * 16 + color_index];
}
```

### Sprite Rendering

Sprites (objects) are stored in OAM and composited over backgrounds:

```c
// Sprite evaluation (per scanline)
static bool ppu_evaluateSprites(Ppu* ppu, int line) {
  int sprite_count = 0;

  // Iterate through all 128 OAM entries
  for (int i = 0; i < 128; i++) {
    uint8 *oam_entry = &ppu->oam[i * 4];

    int spr_y = oam_entry[1];
    int spr_x = oam_entry[0] | ((oam_entry[2] & 1) << 8);

    uint8 tile = oam_entry[2];
    uint8 flags = oam_entry[3];

    // Check if sprite is on this scanline
    int height = kSpriteSizes[ppu->objSize][1];
    if (line >= spr_y && line < spr_y + height) {
      // Render this sprite
      RenderSprite(ppu, i, line);

      if (++sprite_count >= 32) break;  // SNES limit: 32 sprites per line
    }
  }

  return sprite_count > 0;
}
```

### Mode 7 (Rotation/Scaling)

Mode 7 enables 2D rotation and scaling for special effects:

```c
// Mode 7 transformation matrix
static void ppu_calculateMode7Starts(Ppu* ppu, int y) {
  // Matrix parameters (set by game)
  int16 a = ppu->m7matrix[0];  // X scale
  int16 b = ppu->m7matrix[1];  // X rotation
  int16 c = ppu->m7matrix[2];  // Y rotation
  int16 d = ppu->m7matrix[3];  // Y scale

  int16 center_x = ppu->m7center[0];
  int16 center_y = ppu->m7center[1];

  // Calculate starting X position for this scanline
  ppu->m7startx = ((a * (x - center_x)) & ~63) + ((b * (y - center_y)) & ~63);
  ppu->m7starty = ((c * (x - center_x)) & ~63) + ((d * (y - center_y)) & ~63);
}

// Fetch Mode 7 pixel
static int ppu_getPixelForMode7(Ppu* ppu, int x, int layer, bool priority) {
  // Transform screen coordinates to texture coordinates
  int tex_x = ppu->m7startx + (ppu->m7matrix[0] * x);
  int tex_y = ppu->m7starty + (ppu->m7matrix[2] * x);

  // Clamp/wrap coordinates
  tex_x = (tex_x >> 8) & 0x3FF;
  tex_y = (tex_y >> 8) & 0x3FF;

  // Fetch from 128×128 tile map
  return FetchMode7Pixel(ppu, tex_x, tex_y);
}
```

### Color Math

The SNES supports color addition/subtraction and transparency:

```c
// Blend two colors
static uint16 BlendColors(uint16 main_color, uint16 sub_color, int mode) {
  int r1 = main_color & 0x1F;
  int g1 = (main_color >> 5) & 0x1F;
  int b1 = (main_color >> 10) & 0x1F;

  int r2 = sub_color & 0x1F;
  int g2 = (sub_color >> 5) & 0x1F;
  int b2 = (sub_color >> 10) & 0x1F;

  if (mode == 0) {  // Addition
    r1 = min(31, r1 + r2);
    g1 = min(31, g1 + g2);
    b1 = min(31, b1 + b2);
  } else {  // Subtraction
    r1 = max(0, r1 - r2);
    g1 = max(0, g1 - g2);
    b1 = max(0, b1 - b2);
  }

  return r1 | (g1 << 5) | (b1 << 10);
}
```

### Widescreen Support

Zelda3 extends the PPU to render extra pixels for widescreen:

```c
// types.h
enum { kPpuExtraLeftRight = 32 };  // Default: 32 pixels on each side

// Render width calculation
int render_width = 256 + (kPpuExtraLeftRight * 2);  // 256 + 64 = 320 pixels

// The PPU renders beyond the original 256-pixel SNES width
// This is gated by kFeatures0_ExtendScreen64 feature flag
```

## Renderer Abstraction

Modern output is handled through a `RendererFuncs` interface defined in `src/util.h`:

```c
struct RendererFuncs {
  bool (*Initialize)(SDL_Window *window);
  void (*Destroy)();
  void (*BeginDraw)(int width, int height, uint8 **pixels, int *pitch);
  void (*EndDraw)();
  void (*OnResize)(int width, int height);
};
```

### BeginDraw/EndDraw Pattern

```c
// Rendering loop (main.c)
void RenderFrame() {
  uint8 *pixels;
  int pitch;

  // Request frame buffer from renderer
  g_renderer->BeginDraw(width, height, &pixels, &pitch);

  // PPU writes directly to pixel buffer
  PpuRenderFrame(&g_zenv.ppu, pixels, pitch);

  // Renderer uploads to GPU and presents
  g_renderer->EndDraw();
}
```

## OpenGL/OpenGL ES Renderer

**File:** `src/opengl.c`

The OpenGL renderer uploads the PPU frame buffer as a texture and renders a fullscreen quad.

### Initialization

```c
static bool OpenGLRenderer_Init(SDL_Window *window) {
  SDL_GLContext context = SDL_GL_CreateContext(window);
  SDL_GL_SetSwapInterval(1);  // VSync
  ogl_LoadFunctions();

  // Require OpenGL 3.3+ or OpenGL ES 3.0+
  if (!g_opengl_es) {
    if (!ogl_IsVersionGEQ(3, 3))
      Die("You need OpenGL 3.3");
  } else {
    // OpenGL ES (Android, iOS)
    if (majorVersion < 3)
      Die("You need OpenGL ES 3.0");
  }

  // Create texture for frame buffer
  glGenTextures(1, &g_texture.gl_texture);
  glBindTexture(GL_TEXTURE_2D, g_texture.gl_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // Setup fullscreen quad geometry
  CreateFullscreenQuad();

  // Compile shaders (vertex + fragment)
  CompileShaders();

  return true;
}
```

### Frame Rendering

```c
static void OpenGLRenderer_BeginDraw(int width, int height, uint8 **pixels, int *pitch) {
  // Allocate/resize CPU buffer
  size_t needed = width * height * 4;  // RGBA
  if (g_screen_buffer_size < needed) {
    free(g_screen_buffer);
    g_screen_buffer = malloc(needed);
    g_screen_buffer_size = needed;
  }

  // Return buffer to PPU
  *pixels = g_screen_buffer;
  *pitch = width * 4;

  g_draw_width = width;
  g_draw_height = height;
}

static void OpenGLRenderer_EndDraw() {
  // Upload texture from PPU buffer
  glBindTexture(GL_TEXTURE_2D, g_texture.gl_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
               g_draw_width, g_draw_height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, g_screen_buffer);

  // Apply shader (if enabled)
  if (g_glsl_shader) {
    GlslShader_Render(g_glsl_shader, &g_texture);
  } else {
    // Basic shader: render fullscreen quad
    glUseProgram(g_program);
    glBindVertexArray(g_VAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }

  SDL_GL_SwapWindow(g_window);
}
```

### Shader Support

**File:** `src/glsl_shader.c`

Custom GLSL shaders can be loaded for CRT effects, scanlines, etc:

```c
// Load shader from file
GlslShader *GlslShader_Load(const char *fragment_shader_path);

// Render with shader
void GlslShader_Render(GlslShader *shader, GlTextureWithSize *texture);

// Example fragment shader (CRT effect)
// shaders/crt.frag
uniform sampler2D tex;
varying vec2 TexCoord;

void main() {
  vec2 uv = TexCoord;

  // Scanline effect
  float scanline = sin(uv.y * 240.0 * 3.14159) * 0.1;

  // Sample texture
  vec4 color = texture2D(tex, uv);
  color.rgb *= (1.0 - scanline);

  gl_FragColor = color;
}
```

## Vulkan Renderer

**File:** `src/vulkan.c`

The Vulkan renderer provides a modern, cross-platform GPU backend with better performance and lower overhead than OpenGL.

### Key Features

- **Vulkan 1.0 compatibility** (widest device support)
- **MoltenVK support** (macOS via Metal)
- **Pre-compiled SPIR-V shaders** (faster startup)
- **Explicit synchronization** (better frame pacing)

### Initialization

```c
static bool VulkanRenderer_Init(SDL_Window *window) {
  // 1. Create Vulkan instance
  CreateInstance(&vk.instance, window);

  // 2. Create window surface via SDL
  SDL_Vulkan_CreateSurface(window, vk.instance, &vk.surface);

  // 3. Select physical device (GPU)
  SelectPhysicalDevice(vk.instance, &vk.physical_device);

  // 4. Create logical device and queues
  CreateLogicalDevice(vk.physical_device, &vk.device, &vk.graphics_queue);

  // 5. Create swapchain (double/triple buffering)
  CreateSwapchain(vk.device, vk.surface, &vk.swapchain);

  // 6. Load shaders (SPIR-V bytecode)
  LoadShaders("vert.spv", "frag.spv");

  // 7. Create graphics pipeline
  CreateGraphicsPipeline();

  // 8. Create frame buffers and command buffers
  CreateFramebuffers();
  CreateCommandBuffers();

  return true;
}
```

### Frame Rendering

```c
static void VulkanRenderer_BeginDraw(int width, int height, uint8 **pixels, int *pitch) {
  // Wait for previous frame
  vkWaitForFences(vk.device, 1, &vk.in_flight_fences[vk.current_frame], VK_TRUE, UINT64_MAX);

  // Acquire next swapchain image
  vkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX,
                        vk.image_available_semaphores[vk.current_frame],
                        VK_NULL_HANDLE, &image_index);

  // Provide staging buffer for PPU
  *pixels = vk.staging_buffer;
  *pitch = width * 4;
}

static void VulkanRenderer_EndDraw() {
  // Upload texture from staging buffer
  UploadTexture(vk.staging_buffer, vk.texture_image);

  // Record command buffer
  vkResetCommandBuffer(cmd_buffer, 0);
  vkBeginCommandBuffer(cmd_buffer, &begin_info);
  vkCmdBeginRenderPass(cmd_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.graphics_pipeline);
  vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ...);
  vkCmdDrawIndexed(cmd_buffer, 6, 1, 0, 0, 0);  // Draw fullscreen quad
  vkCmdEndRenderPass(cmd_buffer);
  vkEndCommandBuffer(cmd_buffer);

  // Submit to GPU
  vkQueueSubmit(vk.graphics_queue, 1, &submit_info, vk.in_flight_fences[vk.current_frame]);

  // Present
  vkQueuePresentKHR(vk.present_queue, &present_info);

  vk.current_frame = (vk.current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}
```

### Shader Compilation

Vulkan uses pre-compiled SPIR-V shaders:

```bash
# Compile GLSL to SPIR-V
glslc shader.vert -o vert.spv
glslc shader.frag -o frag.spv

# Shaders are loaded as binary blobs at runtime
uint8_t *vert_spv = Platform_ReadWholeFile("vert.spv", &vert_size);
uint8_t *frag_spv = Platform_ReadWholeFile("frag.spv", &frag_size);
```

## Frame Buffer API

**File:** `snes/ppu.c`

Direct access to the PPU frame buffer for screenshots, etc:

```c
// Get raw frame buffer (after rendering)
const uint8 *PpuGetFrameBuffer(int *width_out, int *height_out, int *pitch_out);

// Usage example
int width, height, pitch;
const uint8 *pixels = PpuGetFrameBuffer(&width, &height, &pitch);

// pixels is in RGBA format (4 bytes per pixel)
for (int y = 0; y < height; y++) {
  for (int x = 0; x < width; x++) {
    int offset = y * pitch + x * 4;
    uint8 r = pixels[offset + 0];
    uint8 g = pixels[offset + 1];
    uint8 b = pixels[offset + 2];
    uint8 a = pixels[offset + 3];
  }
}
```

## Performance Notes

**PPU Rendering:**
- Scanline-based (224 scanlines @ 60Hz = 13,440 scanlines/sec)
- Per-pixel operations (256×224 = 57,344 pixels/frame)
- Optimized for cache locality (process one scanline at a time)

**OpenGL Renderer:**
- Single texture upload per frame (~256KB @ 256×224×4)
- Zero-copy when possible (PBO extension)
- VSync enabled by default (prevents tearing)

**Vulkan Renderer:**
- Lower CPU overhead than OpenGL
- Explicit synchronization (better frame pacing)
- Staging buffer for texture uploads
- Double/triple buffering via swapchain

## See Also

- **[Architecture](../architecture.md)** - Overall system architecture
- **[Memory Layout](memory-layout.md)** - VRAM and memory organization
- **[Renderers](renderers.md)** - Detailed renderer implementations
- **Code References:**
  - `snes/ppu.c` - PPU emulation
  - `src/opengl.c` - OpenGL renderer
  - `src/vulkan.c` - Vulkan renderer
  - `src/glsl_shader.c` - Shader support
  - `src/load_gfx.c` - VRAM management

**External References:**
- [SNES PPU Documentation](https://wiki.superfamicom.org/ppu)
- [OpenGL Documentation](https://www.opengl.org/)
- [Vulkan Documentation](https://www.vulkan.org/)
