# Renderers

[Home](../index.md) > [Architecture](../architecture.md) > Renderers

## Overview

Zelda3 uses a renderer abstraction layer to support multiple graphics backends. The PPU generates a platform-independent RGB frame buffer, which renderers upload to the GPU for display. This design separates emulation accuracy (PPU) from output performance (renderers).

## Renderer Interface

All renderers implement the `RendererFuncs` interface defined in `src/util.h`:

```c
struct RendererFuncs {
  bool (*Initialize)(SDL_Window *window);
  void (*Destroy)();
  void (*BeginDraw)(int width, int height, uint8 **pixels, int *pitch);
  void (*EndDraw)();
  void (*OnResize)(int width, int height);
};
```

### Method Descriptions

**`Initialize(SDL_Window *window)`**
- Called once at startup
- Creates rendering context (OpenGL context, Vulkan swapchain, etc.)
- Allocates GPU resources (textures, buffers, shaders)
- Returns `true` on success, `false` on failure

**`Destroy()`**
- Called at shutdown
- Frees all GPU resources
- Destroys rendering context

**`BeginDraw(int width, int height, uint8 **pixels, int *pitch)`**
- Called at start of each frame
- Provides a CPU buffer for PPU to render into
- `pixels`: Pointer to RGB(A) buffer (output parameter)
- `pitch`: Bytes per scanline (output parameter)
- `width`/`height`: Frame dimensions (256×224 or wider for widescreen)

**`EndDraw()`**
- Called after PPU finishes rendering
- Uploads pixel buffer to GPU
- Applies shaders (if enabled)
- Presents frame to screen

**`OnResize(int width, int height)`**
- Called when window is resized
- Updates viewport/projection
- Recreates swapchain (Vulkan) or framebuffers

## Available Renderers

### SDL Software Renderer

**Platform:** All (fallback)
**Performance:** Slowest
**Compatibility:** 100% (no GPU required)

Pure CPU rendering using SDL2's software renderer:

```c
// main.c
static uint8 *g_screen_buffer;

static bool SdlSoftwareRenderer_Init(SDL_Window *window) {
  g_renderer_sdl = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  g_screen_texture = SDL_CreateTexture(g_renderer_sdl,
                                       SDL_PIXELFORMAT_ARGB8888,
                                       SDL_TEXTUREACCESS_STREAMING,
                                       width, height);
  return true;
}

static void SdlSoftwareRenderer_BeginDraw(int width, int height,
                                          uint8 **pixels, int *pitch) {
  SDL_LockTexture(g_screen_texture, NULL, (void**)pixels, pitch);
}

static void SdlSoftwareRenderer_EndDraw() {
  SDL_UnlockTexture(g_screen_texture);
  SDL_RenderCopy(g_renderer_sdl, g_screen_texture, NULL, NULL);
  SDL_RenderPresent(g_renderer_sdl);
}
```

**Pros:**
- Works everywhere (no GPU required)
- Simple implementation
- No driver issues

**Cons:**
- Slow (CPU scaling)
- No shaders
- No hardware acceleration

**Use case:** Debugging, headless servers, ancient hardware

---

### OpenGL Renderer

**File:** `src/opengl.c`
**Platform:** Desktop (Linux, macOS, Windows)
**Requirements:** OpenGL 3.3+ or OpenGL ES 3.0+
**Performance:** Fast

Hardware-accelerated rendering using modern OpenGL:

#### Initialization

```c
static bool OpenGLRenderer_Init(SDL_Window *window) {
  // Create OpenGL context
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

  SDL_GLContext context = SDL_GL_CreateContext(window);
  if (!context) {
    LogError("Failed to create OpenGL context");
    return false;
  }

  // Enable VSync
  SDL_GL_SetSwapInterval(1);

  // Load OpenGL functions
  ogl_LoadFunctions();

  // Check version
  if (!ogl_IsVersionGEQ(3, 3)) {
    LogError("OpenGL 3.3+ required");
    return false;
  }

  // Create resources
  CreateTexture();
  CreateShaders();
  CreateGeometry();

  return true;
}
```

#### Texture Creation

```c
static void CreateTexture() {
  glGenTextures(1, &g_texture.gl_texture);
  glBindTexture(GL_TEXTURE_2D, g_texture.gl_texture);

  // Nearest-neighbor filtering (pixel-perfect)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // Clamp to edge (no wrapping)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}
```

#### Shader Compilation

```c
// Vertex shader (transforms quad to fullscreen)
const char *vertex_shader = R"(
  #version 330 core
  layout(location = 0) in vec3 aPos;
  layout(location = 1) in vec2 aTexCoord;
  out vec2 TexCoord;

  void main() {
    gl_Position = vec4(aPos, 1.0);
    TexCoord = aTexCoord;
  }
)";

// Fragment shader (samples texture)
const char *fragment_shader = R"(
  #version 330 core
  in vec2 TexCoord;
  out vec4 FragColor;
  uniform sampler2D screenTexture;

  void main() {
    FragColor = texture(screenTexture, TexCoord);
  }
)";

// Compile and link
GLuint vs = glCreateShader(GL_VERTEX_SHADER);
glShaderSource(vs, 1, &vertex_shader, NULL);
glCompileShader(vs);

GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
glShaderSource(fs, 1, &fragment_shader, NULL);
glCompileShader(fs);

g_program = glCreateProgram();
glAttachShader(g_program, vs);
glAttachShader(g_program, fs);
glLinkProgram(g_program);
```

#### Geometry Setup

```c
// Fullscreen quad vertices (NDC coordinates)
static const float vertices[] = {
  // positions          // texture coords
  -1.0f,  1.0f, 0.0f,   0.0f, 0.0f,  // top left
  -1.0f, -1.0f, 0.0f,   0.0f, 1.0f,  // bottom left
   1.0f,  1.0f, 0.0f,   1.0f, 0.0f,  // top right
   1.0f, -1.0f, 0.0f,   1.0f, 1.0f,  // bottom right
};

// Create VBO and VAO
glGenBuffers(1, &vbo);
glGenVertexArrays(1, &g_VAO);

glBindVertexArray(g_VAO);
glBindBuffer(GL_ARRAY_BUFFER, vbo);
glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

// Position attribute
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
glEnableVertexAttribArray(0);

// Texture coord attribute
glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                      (void*)(3 * sizeof(float)));
glEnableVertexAttribArray(1);
```

#### Rendering

```c
static void OpenGLRenderer_BeginDraw(int width, int height,
                                     uint8 **pixels, int *pitch) {
  // Allocate CPU buffer
  size_t needed = width * height * 4;  // RGBA
  if (g_screen_buffer_size < needed) {
    free(g_screen_buffer);
    g_screen_buffer = malloc(needed);
    g_screen_buffer_size = needed;
  }

  *pixels = g_screen_buffer;
  *pitch = width * 4;

  g_draw_width = width;
  g_draw_height = height;
}

static void OpenGLRenderer_EndDraw() {
  // Upload texture from CPU buffer
  glBindTexture(GL_TEXTURE_2D, g_texture.gl_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
               g_draw_width, g_draw_height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, g_screen_buffer);

  // Clear screen
  glClear(GL_COLOR_BUFFER_BIT);

  // Draw fullscreen quad
  if (g_glsl_shader) {
    // Custom shader (CRT effect, etc.)
    GlslShader_Render(g_glsl_shader, &g_texture);
  } else {
    // Basic shader
    glUseProgram(g_program);
    glBindVertexArray(g_VAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }

  // Present
  SDL_GL_SwapWindow(g_window);
}
```

#### OpenGL ES Support (Android/iOS)

```c
// Differences for OpenGL ES:
// 1. Version check
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

// 2. Shader headers
#ifdef OPENGL_ES
const char *version_string = "#version 300 es\nprecision mediump float;\n";
#else
const char *version_string = "#version 330 core\n";
#endif

// 3. GLSL differences
// OpenGL:   texture(sampler, coord)
// OpenGL ES: texture2D(sampler, coord)  [GLSL ES 100]
//            texture(sampler, coord)     [GLSL ES 300+]
```

**Pros:**
- Fast (GPU accelerated)
- Shader support (CRT effects, filters)
- Wide compatibility (most desktop systems)

**Cons:**
- Driver bugs on some systems
- Version fragmentation (Core vs Compatibility profiles)
- macOS deprecation warnings

---

### Vulkan Renderer

**File:** `src/vulkan.c`
**Platform:** Linux, Windows, macOS (via MoltenVK), Android
**Requirements:** Vulkan 1.0+
**Performance:** Fastest

Modern low-level graphics API with explicit control:

#### Key Features

- **Vulkan 1.0 compatibility** (widest device support)
- **Cross-platform** (desktop + mobile)
- **MoltenVK support** (macOS via Metal backend)
- **Pre-compiled shaders** (SPIR-V bytecode)
- **Explicit synchronization** (fences, semaphores)
- **Swapchain** (double/triple buffering)

#### Initialization Flow

```c
static bool VulkanRenderer_Init(SDL_Window *window) {
  // 1. Create Vulkan instance
  VkInstanceCreateInfo instance_info = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &app_info,
    .enabledExtensionCount = extension_count,
    .ppEnabledExtensionNames = extensions,
  };

  // MoltenVK requires portability enumeration
  #ifdef __APPLE__
  instance_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
  #endif

  vkCreateInstance(&instance_info, NULL, &vk.instance);

  // 2. Create surface via SDL (cross-platform)
  SDL_Vulkan_CreateSurface(window, vk.instance, &vk.surface);

  // 3. Select physical device (GPU)
  uint32_t device_count = 0;
  vkEnumeratePhysicalDevices(vk.instance, &device_count, NULL);
  VkPhysicalDevice devices[device_count];
  vkEnumeratePhysicalDevices(vk.instance, &device_count, devices);

  // Pick discrete GPU if available
  vk.physical_device = SelectBestDevice(devices, device_count);

  // 4. Create logical device
  VkDeviceCreateInfo device_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .queueCreateInfoCount = 1,
    .pQueueCreateInfos = &queue_info,
    .enabledExtensionCount = 1,
    .ppEnabledExtensionNames = (const char*[]){"VK_KHR_swapchain"},
  };

  vkCreateDevice(vk.physical_device, &device_info, NULL, &vk.device);
  vkGetDeviceQueue(vk.device, vk.graphics_queue_family, 0, &vk.graphics_queue);

  // 5. Create swapchain
  CreateSwapchain();

  // 6. Load shaders (SPIR-V)
  CreateShaderModules();

  // 7. Create pipeline
  CreateGraphicsPipeline();

  // 8. Create framebuffers
  CreateFramebuffers();

  // 9. Create command buffers
  CreateCommandBuffers();

  // 10. Create synchronization objects
  CreateSyncObjects();

  return true;
}
```

#### Swapchain Creation

```c
static void CreateSwapchain() {
  // Query surface capabilities
  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.physical_device, vk.surface,
                                            &capabilities);

  // Choose format (prefer BGRA8888)
  uint32_t format_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(vk.physical_device, vk.surface,
                                       &format_count, NULL);
  VkSurfaceFormatKHR formats[format_count];
  vkGetPhysicalDeviceSurfaceFormatsKHR(vk.physical_device, vk.surface,
                                       &format_count, formats);

  vk.swapchain_format = formats[0].format;  // Usually VK_FORMAT_B8G8R8A8_UNORM

  // Choose present mode (prefer mailbox for low latency)
  VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;  // VSync fallback

  // Create swapchain
  VkSwapchainCreateInfoKHR swapchain_info = {
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface = vk.surface,
    .minImageCount = 2,  // Double buffering
    .imageFormat = vk.swapchain_format,
    .imageExtent = vk.swapchain_extent,
    .imageArrayLayers = 1,
    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .presentMode = present_mode,
  };

  vkCreateSwapchainKHR(vk.device, &swapchain_info, NULL, &vk.swapchain);

  // Get swapchain images
  vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &vk.swapchain_image_count, NULL);
  vk.swapchain_images = malloc(vk.swapchain_image_count * sizeof(VkImage));
  vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &vk.swapchain_image_count,
                          vk.swapchain_images);
}
```

#### Shader Loading

```c
// Shaders are pre-compiled to SPIR-V (vert.spv, frag.spv)
static VkShaderModule LoadShader(const char *filename) {
  size_t code_size;
  uint8_t *code = Platform_ReadWholeFile(filename, &code_size);

  VkShaderModuleCreateInfo create_info = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = code_size,
    .pCode = (const uint32_t*)code,
  };

  VkShaderModule shader_module;
  vkCreateShaderModule(vk.device, &create_info, NULL, &shader_module);

  free(code);
  return shader_module;
}

// Android: Load from APK assets via JNI
#ifdef __ANDROID__
uint8_t *shader_data = LoadAssetFromAPK("vert.spv", &size);
#else
uint8_t *shader_data = Platform_ReadWholeFile("vert.spv", &size);
#endif
```

#### Graphics Pipeline

```c
static void CreateGraphicsPipeline() {
  // Load shaders
  VkShaderModule vert_module = LoadShader("vert.spv");
  VkShaderModule frag_module = LoadShader("frag.spv");

  VkPipelineShaderStageCreateInfo shader_stages[] = {
    {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = vert_module,
      .pName = "main",
    },
    {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = frag_module,
      .pName = "main",
    }
  };

  // Vertex input (position + UV)
  VkVertexInputBindingDescription binding = {
    .binding = 0,
    .stride = sizeof(Vertex),  // sizeof(float[4])
    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };

  VkVertexInputAttributeDescription attributes[] = {
    {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 0},
    {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 8},
  };

  // ... (many more pipeline state structs)

  VkGraphicsPipelineCreateInfo pipeline_info = {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .stageCount = 2,
    .pStages = shader_stages,
    .pVertexInputState = &vertex_input_info,
    .pInputAssemblyState = &input_assembly,
    .pViewportState = &viewport_state,
    .pRasterizationState = &rasterizer,
    .pMultisampleState = &multisampling,
    .pColorBlendState = &color_blending,
    .layout = vk.pipeline_layout,
    .renderPass = vk.render_pass,
    .subpass = 0,
  };

  vkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &pipeline_info,
                            NULL, &vk.graphics_pipeline);
}
```

#### Frame Rendering

```c
static void VulkanRenderer_EndDraw() {
  uint32_t image_index;

  // Wait for previous frame to finish
  vkWaitForFences(vk.device, 1, &vk.in_flight_fences[vk.current_frame],
                  VK_TRUE, UINT64_MAX);
  vkResetFences(vk.device, 1, &vk.in_flight_fences[vk.current_frame]);

  // Acquire next swapchain image
  vkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX,
                        vk.image_available_semaphores[vk.current_frame],
                        VK_NULL_HANDLE, &image_index);

  // Upload texture from staging buffer
  CopyBufferToImage(vk.staging_buffer, vk.texture_image);

  // Record command buffer
  VkCommandBuffer cmd = vk.command_buffers[vk.current_frame];
  vkResetCommandBuffer(cmd, 0);

  VkCommandBufferBeginInfo begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };
  vkBeginCommandBuffer(cmd, &begin_info);

  // Begin render pass
  VkRenderPassBeginInfo render_pass_info = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass = vk.render_pass,
    .framebuffer = vk.framebuffers[image_index],
    .renderArea = {{0, 0}, vk.swapchain_extent},
  };
  vkCmdBeginRenderPass(cmd, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

  // Bind pipeline and draw
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.graphics_pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          vk.pipeline_layout, 0, 1, &vk.descriptor_set, 0, NULL);
  vkCmdBindVertexBuffers(cmd, 0, 1, &vk.vertex_buffer, &(VkDeviceSize){0});
  vkCmdBindIndexBuffer(cmd, vk.index_buffer, 0, VK_INDEX_TYPE_UINT16);
  vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);  // 6 indices (2 triangles)

  vkCmdEndRenderPass(cmd);
  vkEndCommandBuffer(cmd);

  // Submit to GPU
  VkSubmitInfo submit_info = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &vk.image_available_semaphores[vk.current_frame],
    .pWaitDstStageMask = &(VkPipelineStageFlags){VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
    .commandBufferCount = 1,
    .pCommandBuffers = &cmd,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &vk.render_finished_semaphores[vk.current_frame],
  };
  vkQueueSubmit(vk.graphics_queue, 1, &submit_info,
                vk.in_flight_fences[vk.current_frame]);

  // Present
  VkPresentInfoKHR present_info = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &vk.render_finished_semaphores[vk.current_frame],
    .swapchainCount = 1,
    .pSwapchains = &vk.swapchain,
    .pImageIndices = &image_index,
  };
  vkQueuePresentKHR(vk.present_queue, &present_info);

  vk.current_frame = (vk.current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}
```

**Pros:**
- Best performance (lowest overhead)
- Explicit control (no hidden state)
- Modern API (future-proof)
- Excellent mobile support

**Cons:**
- Complex (verbosity)
- Requires more code
- Debugging harder (validation layers needed)

---

## Renderer Selection

**Configuration:** `zelda3.ini`

```ini
[Display]
# OutputMethod options:
#   sdl       - SDL software renderer (slowest, most compatible)
#   opengl    - OpenGL 3.3+ (desktop)
#   opengles  - OpenGL ES 3.0+ (mobile, Raspberry Pi)
#   vulkan    - Vulkan 1.0+ (fastest, modern systems)
OutputMethod = opengl
```

**Runtime Selection:**

```c
// main.c
RendererFuncs *SelectRenderer(const char *method) {
  if (strcmp(method, "opengl") == 0 || strcmp(method, "opengles") == 0) {
    return &g_opengl_renderer;
  } else if (strcmp(method, "vulkan") == 0) {
    #ifdef VULKAN_AVAILABLE
    return &g_vulkan_renderer;
    #else
    LogWarn("Vulkan not available, falling back to OpenGL");
    return &g_opengl_renderer;
    #endif
  } else {
    return &g_sdl_software_renderer;  // Fallback
  }
}
```

## Performance Comparison

| Renderer | Frame Time | CPU Usage | GPU Usage | Notes |
|----------|------------|-----------|-----------|-------|
| SDL Software | ~16ms | High | None | Pure CPU, no acceleration |
| OpenGL | ~1ms | Low | Low | VSync bottleneck |
| OpenGL ES | ~1ms | Low | Low | Mobile optimized |
| Vulkan | ~0.5ms | Very Low | Low | Lowest overhead |

*Measured on: Intel i5, NVIDIA GTX 1060, 1920×1080@60Hz*

## See Also

- **[Architecture](../architecture.md)** - Overall system architecture
- **[Graphics Pipeline](graphics-pipeline.md)** - PPU rendering details
- **Code References:**
  - `src/util.h` - RendererFuncs interface
  - `src/opengl.c` - OpenGL renderer
  - `src/vulkan.c` - Vulkan renderer
  - `src/glsl_shader.c` - Shader support
  - `src/main.c` - Renderer selection

**External References:**
- [OpenGL Documentation](https://www.opengl.org/)
- [Vulkan Documentation](https://www.vulkan.org/)
- [SDL Documentation](https://wiki.libsdl.org/)
