#ifndef ZELDA3_SLANG_SHADER_H_
#define ZELDA3_SLANG_SHADER_H_

// Slang shader support for Vulkan renderer
// Implements RetroArch-compatible slang shader loading and multi-pass rendering

#include "types.h"

// Forward declarations - actual Vulkan types only used in implementation
struct SlangShader;
typedef struct SlangShader SlangShader;

// Opaque handle for Vulkan context passed to shader system
// Note: VkDevice and VkQueue are dispatchable handles (always pointers)
// VkCommandPool is non-dispatchable (uint64_t on 32-bit platforms)
typedef struct SlangVulkanContext {
  void *device;              // VkDevice (dispatchable)
  void *physical_device;     // VkPhysicalDevice (dispatchable)
  uint64_t command_pool;     // VkCommandPool (non-dispatchable)
  void *graphics_queue;      // VkQueue (dispatchable)
  uint32_t graphics_family;  // Queue family index
  uint32_t swapchain_format; // VkFormat of swapchain for final pass
} SlangVulkanContext;

// Input texture info for rendering
// VkImage and VkImageView are non-dispatchable handles
typedef struct SlangInputImage {
  uint64_t image;       // VkImage (non-dispatchable)
  uint64_t image_view;  // VkImageView (non-dispatchable)
  uint16_t width;
  uint16_t height;
} SlangInputImage;

// Output target info (swapchain framebuffer)
// VkFramebuffer and VkRenderPass are non-dispatchable handles
typedef struct SlangOutputTarget {
  uint64_t framebuffer;  // VkFramebuffer (non-dispatchable)
  uint64_t render_pass;  // VkRenderPass (non-dispatchable)
  uint16_t width;        // Framebuffer width
  uint16_t height;       // Framebuffer height
  int16_t viewport_x;    // Viewport offset X (for aspect ratio)
  int16_t viewport_y;    // Viewport offset Y (for aspect ratio)
  uint16_t viewport_w;   // Viewport width (may be < width for aspect ratio)
  uint16_t viewport_h;   // Viewport height (may be < height for aspect ratio)
} SlangOutputTarget;

// Public API
SlangShader *SlangShader_CreateFromFile(const char *filename, const SlangVulkanContext *vk_ctx);
void SlangShader_Destroy(SlangShader *ss);

// Render the shader chain
// cmd: VkCommandBuffer to record commands into (must be in recording state)
// input: Source texture (game framebuffer)
// output: Final render target (swapchain)
void SlangShader_Render(SlangShader *ss, void *cmd,
                        const SlangInputImage *input,
                        const SlangOutputTarget *output);

// Check if a filename is a slang preset
bool IsSlangPreset(const char *filename);

#endif  // ZELDA3_SLANG_SHADER_H_
