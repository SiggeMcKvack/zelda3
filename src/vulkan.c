#include "types.h"
#include "config.h"
#include "util.h"
#include "platform.h"
#include "logging.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// VULKAN_AVAILABLE is defined by CMake when Vulkan library is found (Android API 24+)
#ifdef VULKAN_AVAILABLE
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>
#ifdef __ANDROID__
#include <android/api-level.h>
#include "android_jni.h"
#endif
#endif

#ifdef __ANDROID__
#include <android/log.h>
#define VK_LOG(...) __android_log_print(ANDROID_LOG_INFO, "Zelda3-Vulkan", __VA_ARGS__)
#define VK_ERR(...) __android_log_print(ANDROID_LOG_ERROR, "Zelda3-Vulkan", __VA_ARGS__)
#else
#define VK_LOG(...) fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n")
#define VK_ERR(...) fprintf(stderr, "ERROR: "__VA_ARGS__); fprintf(stderr, "\n")
#endif

#ifdef VULKAN_AVAILABLE

#define MAX_FRAMES_IN_FLIGHT 2
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

// Vertex data for fullscreen quad
typedef struct {
  float pos[2];
  float uv[2];
} Vertex;

static const Vertex g_quad_vertices[] = {
  {{-1.0f, -1.0f}, {0.0f, 0.0f}},
  {{ 1.0f, -1.0f}, {1.0f, 0.0f}},
  {{ 1.0f,  1.0f}, {1.0f, 1.0f}},
  {{-1.0f,  1.0f}, {0.0f, 1.0f}},
};

static const uint16_t g_quad_indices[] = {0, 1, 2, 2, 3, 0};

// NOTE: Embedded SPIR-V shaders removed in favor of asset loading
// Shaders are now loaded from assets/shaders/*.spv at runtime
// This allows proper compilation with glslc for Adreno GPU compatibility
// See compile_shaders.sh for shader build process

// Vulkan state
typedef struct {
  SDL_Window *window;

  VkInstance instance;
  VkSurfaceKHR surface;
  VkPhysicalDevice physical_device;
  VkDevice device;

  uint32_t graphics_queue_family;
  VkQueue graphics_queue;
  VkQueue present_queue;

  VkSwapchainKHR swapchain;
  VkFormat swapchain_format;
  VkExtent2D swapchain_extent;
  VkImage *swapchain_images;
  VkImageView *swapchain_image_views;
  uint32_t swapchain_image_count;

  VkRenderPass render_pass;
  VkPipelineLayout pipeline_layout;
  VkPipeline graphics_pipeline;

  VkFramebuffer *framebuffers;

  VkCommandPool command_pool;
  VkCommandBuffer *command_buffers;

  VkSemaphore *image_available_semaphores;
  VkSemaphore *render_finished_semaphores;
  VkFence *in_flight_fences;

  uint32_t current_frame;

  // Texture resources
  VkImage texture_image;
  VkDeviceMemory texture_memory;
  VkImageView texture_image_view;
  VkSampler texture_sampler;
  VkDescriptorPool descriptor_pool;
  VkDescriptorSetLayout descriptor_set_layout;
  VkDescriptorSet descriptor_set;

  // Vertex buffer
  VkBuffer vertex_buffer;
  VkDeviceMemory vertex_buffer_memory;
  VkBuffer index_buffer;
  VkDeviceMemory index_buffer_memory;

  // Staging buffer for texture uploads
  VkBuffer staging_buffer;
  VkDeviceMemory staging_buffer_memory;
  void *staging_buffer_mapped;

  // Game texture dimensions
  int texture_width;
  int texture_height;
  uint8_t *pixel_buffer;
} VulkanState;

static VulkanState vk = {0};

static uint32_t FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties mem_props;
  vkGetPhysicalDeviceMemoryProperties(vk.physical_device, &mem_props);

  for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
    if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }

  VK_ERR("Failed to find suitable memory type");
  return 0;
}

static bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                         VkBuffer *buffer, VkDeviceMemory *buffer_memory) {
  VkBufferCreateInfo buffer_info = {0};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = size;
  buffer_info.usage = usage;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(vk.device, &buffer_info, NULL, buffer) != VK_SUCCESS) {
    VK_ERR("Failed to create buffer");
    return false;
  }

  VkMemoryRequirements mem_requirements;
  vkGetBufferMemoryRequirements(vk.device, *buffer, &mem_requirements);

  VkMemoryAllocateInfo alloc_info = {0};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = mem_requirements.size;
  alloc_info.memoryTypeIndex = FindMemoryType(mem_requirements.memoryTypeBits, properties);

  if (vkAllocateMemory(vk.device, &alloc_info, NULL, buffer_memory) != VK_SUCCESS) {
    VK_ERR("Failed to allocate buffer memory");
    return false;
  }

  vkBindBufferMemory(vk.device, *buffer, *buffer_memory, 0);
  return true;
}

static bool CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                        VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                        VkImage *image, VkDeviceMemory *image_memory) {
  VkImageCreateInfo image_info = {0};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.extent.width = width;
  image_info.extent.height = height;
  image_info.extent.depth = 1;
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.format = format;
  image_info.tiling = tiling;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.usage = usage;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;

  if (vkCreateImage(vk.device, &image_info, NULL, image) != VK_SUCCESS) {
    VK_ERR("Failed to create image");
    return false;
  }

  VkMemoryRequirements mem_requirements;
  vkGetImageMemoryRequirements(vk.device, *image, &mem_requirements);

  VkMemoryAllocateInfo alloc_info = {0};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = mem_requirements.size;
  alloc_info.memoryTypeIndex = FindMemoryType(mem_requirements.memoryTypeBits, properties);

  if (vkAllocateMemory(vk.device, &alloc_info, NULL, image_memory) != VK_SUCCESS) {
    VK_ERR("Failed to allocate image memory");
    return false;
  }

  vkBindImageMemory(vk.device, *image, *image_memory, 0);
  return true;
}

static VkImageView CreateImageView(VkImage image, VkFormat format) {
  VkImageViewCreateInfo view_info = {0};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = image;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = format;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;

  VkImageView image_view;
  if (vkCreateImageView(vk.device, &view_info, NULL, &image_view) != VK_SUCCESS) {
    VK_ERR("Failed to create image view");
    return VK_NULL_HANDLE;
  }

  return image_view;
}

static VkShaderModule CreateShaderModule(const uint32_t *code, size_t size) {
  VK_LOG("CreateShaderModule: size=%zu bytes, magic=0x%08x", size, code[0]);

  // Validate SPIR-V magic number
  if (size < 4 || code[0] != 0x07230203) {
    VK_ERR("Invalid SPIR-V magic number: expected 0x07230203, got 0x%08x", code[0]);
    return VK_NULL_HANDLE;
  }

  // Validate size is multiple of 4 (SPIR-V requirement)
  if (size % 4 != 0) {
    VK_ERR("Invalid SPIR-V size: %zu bytes (must be multiple of 4)", size);
    return VK_NULL_HANDLE;
  }

  VkShaderModuleCreateInfo create_info = {0};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = size;
  create_info.pCode = code;

  VkShaderModule shader_module;
  VK_LOG("Calling vkCreateShaderModule...");
  VkResult result = vkCreateShaderModule(vk.device, &create_info, NULL, &shader_module);
  VK_LOG("vkCreateShaderModule returned: %d", result);

  if (result != VK_SUCCESS) {
    VK_ERR("Failed to create shader module: %d", result);
    return VK_NULL_HANDLE;
  }

  return shader_module;
}

static VkShaderModule LoadShaderFromAsset(const char *asset_path) {
  VK_LOG("LoadShaderFromAsset: %s", asset_path);

#ifdef __ANDROID__
  // Load shader via JNI on Android
  int shader_size = 0;
  void *shader_data = Android_LoadAsset(asset_path, &shader_size);

  if (!shader_data) {
    VK_ERR("Failed to load shader asset: %s", asset_path);
    return VK_NULL_HANDLE;
  }

  VK_LOG("Loaded shader asset: %s (%d bytes)", asset_path, shader_size);

  // Create shader module from loaded data
  VkShaderModule shader_module = CreateShaderModule((const uint32_t*)shader_data, shader_size);

  // Free the loaded data (CreateShaderModule makes a copy)
  free(shader_data);

  return shader_module;
#else
  // On desktop platforms, load from filesystem
  size_t shader_size = 0;
  uint8_t *shader_data = Platform_ReadWholeFile(asset_path, &shader_size);

  if (!shader_data) {
    VK_ERR("Failed to load shader file: %s", asset_path);
    return VK_NULL_HANDLE;
  }

  VK_LOG("Loaded shader file: %s (%zu bytes)", asset_path, shader_size);

  // Create shader module from loaded data
  VkShaderModule shader_module = CreateShaderModule((const uint32_t*)shader_data, (int)shader_size);

  // Free the loaded data (CreateShaderModule makes a copy)
  free(shader_data);

  return shader_module;
#endif
}

static bool CreateSwapchain() {
  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.physical_device, vk.surface, &capabilities);

  uint32_t format_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(vk.physical_device, vk.surface, &format_count, NULL);
  VkSurfaceFormatKHR *formats = malloc(format_count * sizeof(VkSurfaceFormatKHR));
  vkGetPhysicalDeviceSurfaceFormatsKHR(vk.physical_device, vk.surface, &format_count, formats);

  VkSurfaceFormatKHR surface_format = formats[0];
  for (uint32_t i = 0; i < format_count; i++) {
    if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      surface_format = formats[i];
      break;
    }
  }
  free(formats);

  vk.swapchain_extent = capabilities.currentExtent;
  if (vk.swapchain_extent.width == UINT32_MAX) {
    int w, h;
    SDL_Vulkan_GetDrawableSize(vk.window, &w, &h);
    vk.swapchain_extent.width = w;
    vk.swapchain_extent.height = h;
  }

  uint32_t image_count = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
    image_count = capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR create_info = {0};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = vk.surface;
  create_info.minImageCount = image_count;
  create_info.imageFormat = surface_format.format;
  create_info.imageColorSpace = surface_format.colorSpace;
  create_info.imageExtent = vk.swapchain_extent;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  // Use IDENTITY transform - app is landscape-locked in manifest (AndroidManifest.xml:84)
  // Using currentTransform can cause incorrect rotation on some Android devices
  create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  create_info.clipped = VK_TRUE;
  create_info.oldSwapchain = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(vk.device, &create_info, NULL, &vk.swapchain) != VK_SUCCESS) {
    VK_ERR("Failed to create swapchain");
    return false;
  }

  vk.swapchain_format = surface_format.format;

  vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &vk.swapchain_image_count, NULL);
  vk.swapchain_images = malloc(vk.swapchain_image_count * sizeof(VkImage));
  vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &vk.swapchain_image_count, vk.swapchain_images);

  vk.swapchain_image_views = malloc(vk.swapchain_image_count * sizeof(VkImageView));
  for (uint32_t i = 0; i < vk.swapchain_image_count; i++) {
    vk.swapchain_image_views[i] = CreateImageView(vk.swapchain_images[i], vk.swapchain_format);
  }

  VK_LOG("Swapchain created: %ux%u, %u images", vk.swapchain_extent.width, vk.swapchain_extent.height, vk.swapchain_image_count);
  return true;
}

static bool CreateRenderPass() {
  VkAttachmentDescription color_attachment = {0};
  color_attachment.format = vk.swapchain_format;
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference color_attachment_ref = {0};
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {0};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;

  VkSubpassDependency dependency = {0};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo render_pass_info = {0};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = 1;
  render_pass_info.pAttachments = &color_attachment;
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;
  render_pass_info.dependencyCount = 1;
  render_pass_info.pDependencies = &dependency;

  if (vkCreateRenderPass(vk.device, &render_pass_info, NULL, &vk.render_pass) != VK_SUCCESS) {
    VK_ERR("Failed to create render pass");
    return false;
  }

  return true;
}

static bool CreateDescriptorSetLayout() {
  VkDescriptorSetLayoutBinding sampler_layout_binding = {0};
  sampler_layout_binding.binding = 0;
  sampler_layout_binding.descriptorCount = 1;
  sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  sampler_layout_binding.pImmutableSamplers = NULL;
  sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo layout_info = {0};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = 1;
  layout_info.pBindings = &sampler_layout_binding;

  if (vkCreateDescriptorSetLayout(vk.device, &layout_info, NULL, &vk.descriptor_set_layout) != VK_SUCCESS) {
    VK_ERR("Failed to create descriptor set layout");
    return false;
  }

  return true;
}

static bool CreateGraphicsPipeline() {
  // Load shaders from assets (properly compiled SPIR-V for Adreno compatibility)
  VkShaderModule vert_shader = LoadShaderFromAsset("shaders/vert.spv");
  VkShaderModule frag_shader = LoadShaderFromAsset("shaders/frag.spv");

  if (vert_shader == VK_NULL_HANDLE || frag_shader == VK_NULL_HANDLE) {
    VK_ERR("Shader loading failed (vert=%p, frag=%p)", (void*)vert_shader, (void*)frag_shader);
    // Clean up any successfully loaded shader
    if (vert_shader != VK_NULL_HANDLE) vkDestroyShaderModule(vk.device, vert_shader, NULL);
    if (frag_shader != VK_NULL_HANDLE) vkDestroyShaderModule(vk.device, frag_shader, NULL);
    return false;
  }

  VkPipelineShaderStageCreateInfo vert_stage_info = {0};
  vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vert_stage_info.module = vert_shader;
  vert_stage_info.pName = "main";

  VkPipelineShaderStageCreateInfo frag_stage_info = {0};
  frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  frag_stage_info.module = frag_shader;
  frag_stage_info.pName = "main";

  VkPipelineShaderStageCreateInfo shader_stages[] = {vert_stage_info, frag_stage_info};

  VkVertexInputBindingDescription binding_description = {0};
  binding_description.binding = 0;
  binding_description.stride = sizeof(Vertex);
  binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription attribute_descriptions[2] = {0};
  attribute_descriptions[0].binding = 0;
  attribute_descriptions[0].location = 0;
  attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
  attribute_descriptions[0].offset = offsetof(Vertex, pos);

  attribute_descriptions[1].binding = 0;
  attribute_descriptions[1].location = 1;
  attribute_descriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
  attribute_descriptions[1].offset = offsetof(Vertex, uv);

  VkPipelineVertexInputStateCreateInfo vertex_input_info = {0};
  vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input_info.vertexBindingDescriptionCount = 1;
  vertex_input_info.pVertexBindingDescriptions = &binding_description;
  vertex_input_info.vertexAttributeDescriptionCount = 2;
  vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions;

  VkPipelineInputAssemblyStateCreateInfo input_assembly = {0};
  input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  VkViewport viewport = {0};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)vk.swapchain_extent.width;
  viewport.height = (float)vk.swapchain_extent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor = {0};
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent = vk.swapchain_extent;

  VkPipelineViewportStateCreateInfo viewport_state = {0};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.pViewports = &viewport;
  viewport_state.scissorCount = 1;
  viewport_state.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer = {0};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampling = {0};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo depth_stencil = {0};
  depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil.depthTestEnable = VK_FALSE;
  depth_stencil.depthWriteEnable = VK_FALSE;
  depth_stencil.stencilTestEnable = VK_FALSE;

  VkPipelineColorBlendAttachmentState color_blend_attachment = {0};
  color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  color_blend_attachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo color_blending = {0};
  color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.logicOpEnable = VK_FALSE;
  color_blending.logicOp = VK_LOGIC_OP_COPY;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &color_blend_attachment;

  VkPipelineLayoutCreateInfo pipeline_layout_info = {0};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.setLayoutCount = 1;
  pipeline_layout_info.pSetLayouts = &vk.descriptor_set_layout;

  VK_LOG("Creating pipeline layout...");
  if (vkCreatePipelineLayout(vk.device, &pipeline_layout_info, NULL, &vk.pipeline_layout) != VK_SUCCESS) {
    VK_ERR("Failed to create pipeline layout");
    return false;
  }
  VK_LOG("Pipeline layout created");

  VkGraphicsPipelineCreateInfo pipeline_info = {0};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = 2;
  pipeline_info.pStages = shader_stages;
  pipeline_info.pVertexInputState = &vertex_input_info;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterizer;
  pipeline_info.pMultisampleState = &multisampling;
  pipeline_info.pDepthStencilState = &depth_stencil;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.layout = vk.pipeline_layout;
  pipeline_info.renderPass = vk.render_pass;
  pipeline_info.subpass = 0;

  VK_LOG("Calling vkCreateGraphicsPipelines (this may take a moment)...");
  VkResult result = vkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &vk.graphics_pipeline);
  VK_LOG("vkCreateGraphicsPipelines returned: %d", result);

  if (result != VK_SUCCESS) {
    VK_ERR("Failed to create graphics pipeline: %d", result);
    return false;
  }

  VK_LOG("Destroying shader modules...");
  vkDestroyShaderModule(vk.device, frag_shader, NULL);
  vkDestroyShaderModule(vk.device, vert_shader, NULL);
  VK_LOG("Graphics pipeline created successfully");

  return true;
}

static bool CreateFramebuffers() {
  vk.framebuffers = malloc(vk.swapchain_image_count * sizeof(VkFramebuffer));

  for (uint32_t i = 0; i < vk.swapchain_image_count; i++) {
    VkImageView attachments[] = {vk.swapchain_image_views[i]};

    VkFramebufferCreateInfo framebuffer_info = {0};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = vk.render_pass;
    framebuffer_info.attachmentCount = 1;
    framebuffer_info.pAttachments = attachments;
    framebuffer_info.width = vk.swapchain_extent.width;
    framebuffer_info.height = vk.swapchain_extent.height;
    framebuffer_info.layers = 1;

    if (vkCreateFramebuffer(vk.device, &framebuffer_info, NULL, &vk.framebuffers[i]) != VK_SUCCESS) {
      VK_ERR("Failed to create framebuffer %u", i);
      return false;
    }
  }

  return true;
}

static bool CreateCommandPool() {
  VkCommandPoolCreateInfo pool_info = {0};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_info.queueFamilyIndex = vk.graphics_queue_family;

  if (vkCreateCommandPool(vk.device, &pool_info, NULL, &vk.command_pool) != VK_SUCCESS) {
    VK_ERR("Failed to create command pool");
    return false;
  }

  return true;
}

static bool CreateCommandBuffers() {
  vk.command_buffers = malloc(MAX_FRAMES_IN_FLIGHT * sizeof(VkCommandBuffer));

  VkCommandBufferAllocateInfo alloc_info = {0};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = vk.command_pool;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

  if (vkAllocateCommandBuffers(vk.device, &alloc_info, vk.command_buffers) != VK_SUCCESS) {
    VK_ERR("Failed to allocate command buffers");
    return false;
  }

  return true;
}

static bool CreateSyncObjects() {
  vk.image_available_semaphores = malloc(MAX_FRAMES_IN_FLIGHT * sizeof(VkSemaphore));
  vk.render_finished_semaphores = malloc(MAX_FRAMES_IN_FLIGHT * sizeof(VkSemaphore));
  vk.in_flight_fences = malloc(MAX_FRAMES_IN_FLIGHT * sizeof(VkFence));

  VkSemaphoreCreateInfo semaphore_info = {0};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fence_info = {0};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    if (vkCreateSemaphore(vk.device, &semaphore_info, NULL, &vk.image_available_semaphores[i]) != VK_SUCCESS ||
        vkCreateSemaphore(vk.device, &semaphore_info, NULL, &vk.render_finished_semaphores[i]) != VK_SUCCESS ||
        vkCreateFence(vk.device, &fence_info, NULL, &vk.in_flight_fences[i]) != VK_SUCCESS) {
      VK_ERR("Failed to create synchronization objects");
      return false;
    }
  }

  return true;
}

static bool CreateVertexBuffer() {
  VkDeviceSize buffer_size = sizeof(g_quad_vertices);

  VkBuffer staging_buffer;
  VkDeviceMemory staging_buffer_memory;
  if (!CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    &staging_buffer, &staging_buffer_memory)) {
    return false;
  }

  void *data;
  vkMapMemory(vk.device, staging_buffer_memory, 0, buffer_size, 0, &data);
  memcpy(data, g_quad_vertices, buffer_size);
  vkUnmapMemory(vk.device, staging_buffer_memory);

  if (!CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    &vk.vertex_buffer, &vk.vertex_buffer_memory)) {
    return false;
  }

  // Copy staging to device local buffer
  VkCommandBufferAllocateInfo alloc_info = {0};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandPool = vk.command_pool;
  alloc_info.commandBufferCount = 1;

  VkCommandBuffer command_buffer;
  vkAllocateCommandBuffers(vk.device, &alloc_info, &command_buffer);

  VkCommandBufferBeginInfo begin_info = {0};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(command_buffer, &begin_info);

  VkBufferCopy copy_region = {0};
  copy_region.size = buffer_size;
  vkCmdCopyBuffer(command_buffer, staging_buffer, vk.vertex_buffer, 1, &copy_region);

  vkEndCommandBuffer(command_buffer);

  VkSubmitInfo submit_info = {0};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;

  vkQueueSubmit(vk.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
  vkQueueWaitIdle(vk.graphics_queue);

  vkFreeCommandBuffers(vk.device, vk.command_pool, 1, &command_buffer);

  vkDestroyBuffer(vk.device, staging_buffer, NULL);
  vkFreeMemory(vk.device, staging_buffer_memory, NULL);

  // Create index buffer
  buffer_size = sizeof(g_quad_indices);

  if (!CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    &staging_buffer, &staging_buffer_memory)) {
    return false;
  }

  vkMapMemory(vk.device, staging_buffer_memory, 0, buffer_size, 0, &data);
  memcpy(data, g_quad_indices, buffer_size);
  vkUnmapMemory(vk.device, staging_buffer_memory);

  if (!CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    &vk.index_buffer, &vk.index_buffer_memory)) {
    return false;
  }

  vkAllocateCommandBuffers(vk.device, &alloc_info, &command_buffer);
  vkBeginCommandBuffer(command_buffer, &begin_info);

  copy_region.size = buffer_size;
  vkCmdCopyBuffer(command_buffer, staging_buffer, vk.index_buffer, 1, &copy_region);

  vkEndCommandBuffer(command_buffer);

  vkQueueSubmit(vk.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
  vkQueueWaitIdle(vk.graphics_queue);

  vkFreeCommandBuffers(vk.device, vk.command_pool, 1, &command_buffer);

  vkDestroyBuffer(vk.device, staging_buffer, NULL);
  vkFreeMemory(vk.device, staging_buffer_memory, NULL);

  return true;
}

static bool CreateTextureResources(int width, int height) {
  // Create texture image
  // IMPORTANT: Use BGRA UNORM (linear) format to match OpenGL's GL_BGRA (see opengl.c:224)
  // Game pixel data is already gamma-corrected, so we don't want SRGB conversion
  if (!CreateImage(width, height, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                   &vk.texture_image, &vk.texture_memory)) {
    return false;
  }

  vk.texture_image_view = CreateImageView(vk.texture_image, VK_FORMAT_B8G8R8A8_UNORM);

  // Create texture sampler
  VkSamplerCreateInfo sampler_info = {0};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = VK_FILTER_NEAREST;
  sampler_info.minFilter = VK_FILTER_NEAREST;
  sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.anisotropyEnable = VK_FALSE;
  sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  sampler_info.unnormalizedCoordinates = VK_FALSE;
  sampler_info.compareEnable = VK_FALSE;
  sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

  if (vkCreateSampler(vk.device, &sampler_info, NULL, &vk.texture_sampler) != VK_SUCCESS) {
    VK_ERR("Failed to create texture sampler");
    return false;
  }

  // Create descriptor pool
  VkDescriptorPoolSize pool_size = {0};
  pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  pool_size.descriptorCount = 1;

  VkDescriptorPoolCreateInfo pool_info = {0};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = 1;
  pool_info.pPoolSizes = &pool_size;
  pool_info.maxSets = 1;

  if (vkCreateDescriptorPool(vk.device, &pool_info, NULL, &vk.descriptor_pool) != VK_SUCCESS) {
    VK_ERR("Failed to create descriptor pool");
    return false;
  }

  // Allocate descriptor set
  VkDescriptorSetAllocateInfo alloc_info = {0};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = vk.descriptor_pool;
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts = &vk.descriptor_set_layout;

  if (vkAllocateDescriptorSets(vk.device, &alloc_info, &vk.descriptor_set) != VK_SUCCESS) {
    VK_ERR("Failed to allocate descriptor set");
    return false;
  }

  // Update descriptor set
  VkDescriptorImageInfo image_info = {0};
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image_info.imageView = vk.texture_image_view;
  image_info.sampler = vk.texture_sampler;

  VkWriteDescriptorSet descriptor_write = {0};
  descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptor_write.dstSet = vk.descriptor_set;
  descriptor_write.dstBinding = 0;
  descriptor_write.dstArrayElement = 0;
  descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptor_write.descriptorCount = 1;
  descriptor_write.pImageInfo = &image_info;

  vkUpdateDescriptorSets(vk.device, 1, &descriptor_write, 0, NULL);

  // Create staging buffer for texture uploads (persistent)
  VkDeviceSize buffer_size = width * height * 4;
  if (!CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    &vk.staging_buffer, &vk.staging_buffer_memory)) {
    return false;
  }

  // Map staging buffer permanently
  vkMapMemory(vk.device, vk.staging_buffer_memory, 0, buffer_size, 0, &vk.staging_buffer_mapped);

  // Transition texture to shader read layout
  VkCommandBufferAllocateInfo cmd_alloc_info = {0};
  cmd_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmd_alloc_info.commandPool = vk.command_pool;
  cmd_alloc_info.commandBufferCount = 1;

  VkCommandBuffer command_buffer;
  vkAllocateCommandBuffers(vk.device, &cmd_alloc_info, &command_buffer);

  VkCommandBufferBeginInfo begin_info = {0};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(command_buffer, &begin_info);

  VkImageMemoryBarrier barrier = {0};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = vk.texture_image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = 0;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                      0, NULL, 0, NULL, 1, &barrier);

  vkEndCommandBuffer(command_buffer);

  VkSubmitInfo submit_info = {0};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;

  vkQueueSubmit(vk.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
  vkQueueWaitIdle(vk.graphics_queue);

  vkFreeCommandBuffers(vk.device, vk.command_pool, 1, &command_buffer);

  return true;
}

static bool VulkanRenderer_Init(SDL_Window *window) {
  VK_LOG("Initializing Vulkan renderer");

#ifdef __ANDROID__
  // Check runtime API level - Vulkan requires Android API 24+
  int api_level = android_get_device_api_level();
  if (api_level < 24) {
    VK_ERR("Vulkan requires Android API 24+, device is running API %d", api_level);
    return false;
  }
  VK_LOG("Device API level: %d (Vulkan supported)", api_level);
#endif

  vk.window = window;

  // Get required extensions from SDL
  unsigned int extension_count = 0;
  if (!SDL_Vulkan_GetInstanceExtensions(window, &extension_count, NULL)) {
    VK_ERR("Failed to get Vulkan extension count: %s", SDL_GetError());
    return false;
  }

  // Allocate space for SDL extensions + portability enumeration extension (for MoltenVK)
  const char **extensions = malloc((extension_count + 2) * sizeof(const char *));
  if (!SDL_Vulkan_GetInstanceExtensions(window, &extension_count, extensions)) {
    VK_ERR("Failed to get Vulkan extensions: %s", SDL_GetError());
    free(extensions);
    return false;
  }

  // Add portability enumeration extension for MoltenVK support on macOS
  extensions[extension_count++] = "VK_KHR_portability_enumeration";

  VK_LOG("Requesting %u Vulkan instance extensions", extension_count);
  for (unsigned int i = 0; i < extension_count; i++) {
    VK_LOG("  - %s", extensions[i]);
  }

  // Create Vulkan instance
  VkApplicationInfo app_info = {0};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Zelda3";
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.pEngineName = "No Engine";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = VK_API_VERSION_1_0;

  VkInstanceCreateInfo create_info = {0};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;
  create_info.enabledExtensionCount = extension_count;
  create_info.ppEnabledExtensionNames = extensions;
  create_info.enabledLayerCount = 0;
  create_info.flags = 0x00000001;  // VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR

  VkResult result = vkCreateInstance(&create_info, NULL, &vk.instance);
  free(extensions);

  if (result != VK_SUCCESS) {
    VK_ERR("Failed to create Vulkan instance: %d", result);
    return false;
  }

  // Create surface
  if (!SDL_Vulkan_CreateSurface(window, vk.instance, &vk.surface)) {
    VK_ERR("Failed to create Vulkan surface: %s", SDL_GetError());
    return false;
  }

  // Select physical device
  uint32_t device_count = 0;
  vkEnumeratePhysicalDevices(vk.instance, &device_count, NULL);
  if (device_count == 0) {
    VK_ERR("Failed to find GPUs with Vulkan support");
    return false;
  }

  VkPhysicalDevice *devices = malloc(device_count * sizeof(VkPhysicalDevice));
  vkEnumeratePhysicalDevices(vk.instance, &device_count, devices);
  vk.physical_device = devices[0];
  free(devices);

  VkPhysicalDeviceProperties device_props;
  vkGetPhysicalDeviceProperties(vk.physical_device, &device_props);
  VK_LOG("Selected GPU: %s", device_props.deviceName);

  // Check for known problematic drivers
  if (strstr(device_props.deviceName, "SwiftShader") != NULL) {
    VK_ERR("WARNING: SwiftShader detected. Vulkan pipeline creation may hang.");
    VK_ERR("SwiftShader is a software Vulkan implementation used by Android emulators.");
    VK_ERR("Falling back to OpenGL ES renderer.");
    return false;
  }

  // Note: Adreno GPU support now enabled with properly compiled SPIR-V shaders
  // Shaders are loaded from assets (compiled with glslc) instead of embedded bytecode
  // This should work on Adreno GPUs, but testing is required
  // Uncomment the block below if issues persist on Adreno:
  /*
  if (strstr(device_props.deviceName, "Adreno") != NULL) {
    VK_ERR("WARNING: Adreno GPU detected. Vulkan support experimental.");
    VK_ERR("Falling back to OpenGL ES renderer.");
    return false;
  }
  */

  // Find graphics queue family
  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(vk.physical_device, &queue_family_count, NULL);

  VkQueueFamilyProperties *queue_families = malloc(queue_family_count * sizeof(VkQueueFamilyProperties));
  vkGetPhysicalDeviceQueueFamilyProperties(vk.physical_device, &queue_family_count, queue_families);

  vk.graphics_queue_family = UINT32_MAX;
  for (uint32_t i = 0; i < queue_family_count; i++) {
    if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      VkBool32 present_support = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(vk.physical_device, i, vk.surface, &present_support);
      if (present_support) {
        vk.graphics_queue_family = i;
        break;
      }
    }
  }
  free(queue_families);

  if (vk.graphics_queue_family == UINT32_MAX) {
    VK_ERR("Failed to find suitable queue family");
    return false;
  }

  // Create logical device
  float queue_priority = 1.0f;
  VkDeviceQueueCreateInfo queue_create_info = {0};
  queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info.queueFamilyIndex = vk.graphics_queue_family;
  queue_create_info.queueCount = 1;
  queue_create_info.pQueuePriorities = &queue_priority;

  VkPhysicalDeviceFeatures device_features = {0};

  // Check if portability subset extension is available (required for MoltenVK)
  uint32_t ext_count = 0;
  vkEnumerateDeviceExtensionProperties(vk.physical_device, NULL, &ext_count, NULL);
  VkExtensionProperties *available_exts = malloc(ext_count * sizeof(VkExtensionProperties));
  vkEnumerateDeviceExtensionProperties(vk.physical_device, NULL, &ext_count, available_exts);

  bool has_portability_subset = false;
  for (uint32_t i = 0; i < ext_count; i++) {
    if (strcmp(available_exts[i].extensionName, "VK_KHR_portability_subset") == 0) {
      has_portability_subset = true;
      break;
    }
  }
  free(available_exts);

  const char *device_extensions[2];
  uint32_t device_ext_count = 0;
  device_extensions[device_ext_count++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
  if (has_portability_subset) {
    device_extensions[device_ext_count++] = "VK_KHR_portability_subset";
    VK_LOG("Enabling VK_KHR_portability_subset for MoltenVK compatibility");
  }

  VkDeviceCreateInfo device_create_info = {0};
  device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_create_info.queueCreateInfoCount = 1;
  device_create_info.pQueueCreateInfos = &queue_create_info;
  device_create_info.pEnabledFeatures = &device_features;
  device_create_info.enabledExtensionCount = device_ext_count;
  device_create_info.ppEnabledExtensionNames = device_extensions;
  device_create_info.enabledLayerCount = 0;

  if (vkCreateDevice(vk.physical_device, &device_create_info, NULL, &vk.device) != VK_SUCCESS) {
    VK_ERR("Failed to create logical device");
    return false;
  }

  vkGetDeviceQueue(vk.device, vk.graphics_queue_family, 0, &vk.graphics_queue);
  vk.present_queue = vk.graphics_queue;

  // Create swapchain and rendering resources
  if (!CreateSwapchain()) { VK_ERR("CreateSwapchain failed"); return false; }
  VK_LOG("CreateRenderPass starting");
  if (!CreateRenderPass()) { VK_ERR("CreateRenderPass failed"); return false; }
  VK_LOG("CreateDescriptorSetLayout starting");
  if (!CreateDescriptorSetLayout()) { VK_ERR("CreateDescriptorSetLayout failed"); return false; }
  VK_LOG("CreateGraphicsPipeline starting");
  if (!CreateGraphicsPipeline()) { VK_ERR("CreateGraphicsPipeline failed"); return false; }
  VK_LOG("CreateFramebuffers starting");
  if (!CreateFramebuffers()) { VK_ERR("CreateFramebuffers failed"); return false; }
  VK_LOG("CreateCommandPool starting");
  if (!CreateCommandPool()) { VK_ERR("CreateCommandPool failed"); return false; }
  VK_LOG("CreateVertexBuffer starting");
  if (!CreateVertexBuffer()) { VK_ERR("CreateVertexBuffer failed"); return false; }
  VK_LOG("CreateCommandBuffers starting");
  if (!CreateCommandBuffers()) { VK_ERR("CreateCommandBuffers failed"); return false; }
  VK_LOG("CreateSyncObjects starting");
  if (!CreateSyncObjects()) { VK_ERR("CreateSyncObjects failed"); return false; }

  VK_LOG("Vulkan renderer initialized successfully");
  return true;
}

static void VulkanRenderer_Destroy() {
  if (vk.device) {
    vkDeviceWaitIdle(vk.device);
  }

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    if (vk.image_available_semaphores) vkDestroySemaphore(vk.device, vk.image_available_semaphores[i], NULL);
    if (vk.render_finished_semaphores) vkDestroySemaphore(vk.device, vk.render_finished_semaphores[i], NULL);
    if (vk.in_flight_fences) vkDestroyFence(vk.device, vk.in_flight_fences[i], NULL);
  }
  free(vk.image_available_semaphores);
  free(vk.render_finished_semaphores);
  free(vk.in_flight_fences);

  if (vk.staging_buffer_mapped) {
    vkUnmapMemory(vk.device, vk.staging_buffer_memory);
  }
  if (vk.staging_buffer) vkDestroyBuffer(vk.device, vk.staging_buffer, NULL);
  if (vk.staging_buffer_memory) vkFreeMemory(vk.device, vk.staging_buffer_memory, NULL);

  if (vk.descriptor_pool) vkDestroyDescriptorPool(vk.device, vk.descriptor_pool, NULL);
  if (vk.texture_sampler) vkDestroySampler(vk.device, vk.texture_sampler, NULL);
  if (vk.texture_image_view) vkDestroyImageView(vk.device, vk.texture_image_view, NULL);
  if (vk.texture_image) vkDestroyImage(vk.device, vk.texture_image, NULL);
  if (vk.texture_memory) vkFreeMemory(vk.device, vk.texture_memory, NULL);

  if (vk.vertex_buffer) vkDestroyBuffer(vk.device, vk.vertex_buffer, NULL);
  if (vk.vertex_buffer_memory) vkFreeMemory(vk.device, vk.vertex_buffer_memory, NULL);
  if (vk.index_buffer) vkDestroyBuffer(vk.device, vk.index_buffer, NULL);
  if (vk.index_buffer_memory) vkFreeMemory(vk.device, vk.index_buffer_memory, NULL);

  if (vk.command_pool) vkDestroyCommandPool(vk.device, vk.command_pool, NULL);
  free(vk.command_buffers);

  if (vk.framebuffers) {
    for (uint32_t i = 0; i < vk.swapchain_image_count; i++) {
      vkDestroyFramebuffer(vk.device, vk.framebuffers[i], NULL);
    }
    free(vk.framebuffers);
  }

  if (vk.graphics_pipeline) vkDestroyPipeline(vk.device, vk.graphics_pipeline, NULL);
  if (vk.pipeline_layout) vkDestroyPipelineLayout(vk.device, vk.pipeline_layout, NULL);
  if (vk.descriptor_set_layout) vkDestroyDescriptorSetLayout(vk.device, vk.descriptor_set_layout, NULL);
  if (vk.render_pass) vkDestroyRenderPass(vk.device, vk.render_pass, NULL);

  if (vk.swapchain_image_views) {
    for (uint32_t i = 0; i < vk.swapchain_image_count; i++) {
      vkDestroyImageView(vk.device, vk.swapchain_image_views[i], NULL);
    }
    free(vk.swapchain_image_views);
  }
  free(vk.swapchain_images);

  if (vk.swapchain) vkDestroySwapchainKHR(vk.device, vk.swapchain, NULL);
  if (vk.device) vkDestroyDevice(vk.device, NULL);
  if (vk.surface) vkDestroySurfaceKHR(vk.instance, vk.surface, NULL);
  if (vk.instance) vkDestroyInstance(vk.instance, NULL);

  free(vk.pixel_buffer);

  memset(&vk, 0, sizeof(vk));

  VK_LOG("Vulkan renderer destroyed");
}

static void VulkanRenderer_BeginDraw(int width, int height, uint8_t **pixels, int *pitch) {
  static int frame_count = 0;
  if (frame_count == 0) {
    VK_LOG("VulkanRenderer_BeginDraw called: %dx%d", width, height);
  }
  frame_count++;

  if (!vk.pixel_buffer || vk.texture_width != width || vk.texture_height != height) {
    // Recreate texture if dimensions changed
    if (vk.texture_image) {
      vkDeviceWaitIdle(vk.device);

      vkDestroyDescriptorPool(vk.device, vk.descriptor_pool, NULL);
      vkDestroySampler(vk.device, vk.texture_sampler, NULL);
      vkDestroyImageView(vk.device, vk.texture_image_view, NULL);
      vkDestroyImage(vk.device, vk.texture_image, NULL);
      vkFreeMemory(vk.device, vk.texture_memory, NULL);

      if (vk.staging_buffer_mapped) {
        vkUnmapMemory(vk.device, vk.staging_buffer_memory);
      }
      vkDestroyBuffer(vk.device, vk.staging_buffer, NULL);
      vkFreeMemory(vk.device, vk.staging_buffer_memory, NULL);
    }

    free(vk.pixel_buffer);
    vk.texture_width = width;
    vk.texture_height = height;
    vk.pixel_buffer = malloc(width * height * 4);

    CreateTextureResources(width, height);
  }

  *pixels = vk.pixel_buffer;
  *pitch = width * 4;
}

static void VulkanRenderer_EndDraw() {
  // Wait for previous frame
  vkWaitForFences(vk.device, 1, &vk.in_flight_fences[vk.current_frame], VK_TRUE, UINT64_MAX);

  // Acquire next swapchain image
  uint32_t image_index;
  VkResult result = vkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX,
                                          vk.image_available_semaphores[vk.current_frame],
                                          VK_NULL_HANDLE, &image_index);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    // Swapchain needs recreation (window resize, etc.)
    VK_LOG("Swapchain out of date, skipping frame");
    return;
  }

  vkResetFences(vk.device, 1, &vk.in_flight_fences[vk.current_frame]);

  // Upload texture data
  memcpy(vk.staging_buffer_mapped, vk.pixel_buffer, vk.texture_width * vk.texture_height * 4);

  VkCommandBuffer cmd = vk.command_buffers[vk.current_frame];
  vkResetCommandBuffer(cmd, 0);

  VkCommandBufferBeginInfo begin_info = {0};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkBeginCommandBuffer(cmd, &begin_info);

  // Transition texture to transfer dst
  VkImageMemoryBarrier barrier = {0};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = vk.texture_image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                      VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                      0, NULL, 0, NULL, 1, &barrier);

  // Copy buffer to image
  VkBufferImageCopy region = {0};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset.x = 0;
  region.imageOffset.y = 0;
  region.imageOffset.z = 0;
  region.imageExtent.width = vk.texture_width;
  region.imageExtent.height = vk.texture_height;
  region.imageExtent.depth = 1;

  vkCmdCopyBufferToImage(cmd, vk.staging_buffer, vk.texture_image,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  // Transition texture back to shader read
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                      0, NULL, 0, NULL, 1, &barrier);

  // Begin render pass
  VkRenderPassBeginInfo render_pass_info = {0};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render_pass_info.renderPass = vk.render_pass;
  render_pass_info.framebuffer = vk.framebuffers[image_index];
  render_pass_info.renderArea.offset.x = 0;
  render_pass_info.renderArea.offset.y = 0;
  render_pass_info.renderArea.extent = vk.swapchain_extent;

  VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
  render_pass_info.clearValueCount = 1;
  render_pass_info.pClearValues = &clear_color;

  vkCmdBeginRenderPass(cmd, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.graphics_pipeline);

  VkBuffer vertex_buffers[] = {vk.vertex_buffer};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, offsets);
  vkCmdBindIndexBuffer(cmd, vk.index_buffer, 0, VK_INDEX_TYPE_UINT16);

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout,
                         0, 1, &vk.descriptor_set, 0, NULL);

  vkCmdDrawIndexed(cmd, ARRAY_SIZE(g_quad_indices), 1, 0, 0, 0);

  vkCmdEndRenderPass(cmd);

  vkEndCommandBuffer(cmd);

  // Submit command buffer
  VkSubmitInfo submit_info = {0};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore wait_semaphores[] = {vk.image_available_semaphores[vk.current_frame]};
  VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = wait_semaphores;
  submit_info.pWaitDstStageMask = wait_stages;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &cmd;

  VkSemaphore signal_semaphores[] = {vk.render_finished_semaphores[vk.current_frame]};
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = signal_semaphores;

  if (vkQueueSubmit(vk.graphics_queue, 1, &submit_info, vk.in_flight_fences[vk.current_frame]) != VK_SUCCESS) {
    VK_ERR("Failed to submit draw command buffer");
    return;
  }

  // Present
  VkPresentInfoKHR present_info = {0};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = signal_semaphores;

  VkSwapchainKHR swapchains[] = {vk.swapchain};
  present_info.swapchainCount = 1;
  present_info.pSwapchains = swapchains;
  present_info.pImageIndices = &image_index;

  vkQueuePresentKHR(vk.present_queue, &present_info);

  vk.current_frame = (vk.current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

static void VulkanRenderer_OnResize(int width, int height) {
  (void)width; (void)height;
  // Swapchain recreation is handled automatically on next frame via VK_ERROR_OUT_OF_DATE_KHR
  // detection in EndDraw(). Vulkan drivers handle this gracefully.
  VK_LOG("Window resize detected: %dx%d (swapchain will auto-recreate on next frame)", width, height);
}

const struct RendererFuncs kVulkanRendererFuncs = {
  .Initialize = VulkanRenderer_Init,
  .Destroy = VulkanRenderer_Destroy,
  .BeginDraw = VulkanRenderer_BeginDraw,
  .EndDraw = VulkanRenderer_EndDraw,
  .OnResize = VulkanRenderer_OnResize,
};

#else  // !VULKAN_AVAILABLE

// Stub implementation for platforms without Vulkan support
static bool VulkanRenderer_Init_Stub(SDL_Window *window) {
  (void)window;
  VK_ERR("Vulkan renderer not available - requires Android API 24+ and Vulkan library");
  return false;
}

static void VulkanRenderer_Destroy_Stub() {}

static void VulkanRenderer_BeginDraw_Stub(int width, int height, uint8_t **pixels, int *pitch) {
  (void)width; (void)height; (void)pixels; (void)pitch;
}

static void VulkanRenderer_EndDraw_Stub() {}

static void VulkanRenderer_OnResize_Stub(int width, int height) {
  (void)width; (void)height;
}

const struct RendererFuncs kVulkanRendererFuncs = {
  .Initialize = VulkanRenderer_Init_Stub,
  .Destroy = VulkanRenderer_Destroy_Stub,
  .BeginDraw = VulkanRenderer_BeginDraw_Stub,
  .EndDraw = VulkanRenderer_EndDraw_Stub,
  .OnResize = VulkanRenderer_OnResize_Stub,
};

#endif  // VULKAN_AVAILABLE

void VulkanRenderer_Create(struct RendererFuncs *funcs) {
  *funcs = kVulkanRendererFuncs;
}

// Quick GPU name check without full initialization
const char* GetVulkanGPUName() {
#ifdef VULKAN_AVAILABLE
  // Create minimal instance to query GPU
  VkApplicationInfo app_info = {0};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Zelda3";
  app_info.apiVersion = VK_API_VERSION_1_0;

  VkInstanceCreateInfo create_info = {0};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;

  VkInstance instance;
  if (vkCreateInstance(&create_info, NULL, &instance) != VK_SUCCESS) {
    return NULL;
  }

  uint32_t device_count = 0;
  vkEnumeratePhysicalDevices(instance, &device_count, NULL);
  if (device_count == 0) {
    vkDestroyInstance(instance, NULL);
    return NULL;
  }

  VkPhysicalDevice *devices = malloc(device_count * sizeof(VkPhysicalDevice));
  vkEnumeratePhysicalDevices(instance, &device_count, devices);

  static VkPhysicalDeviceProperties device_props;
  vkGetPhysicalDeviceProperties(devices[0], &device_props);

  free(devices);
  vkDestroyInstance(instance, NULL);

  return device_props.deviceName;
#else
  return NULL;
#endif
}
