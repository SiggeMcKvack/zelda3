#pragma once

struct RendererFuncs;

void VulkanRenderer_Create(struct RendererFuncs *funcs);
const char* GetVulkanGPUName(void);  // Returns NULL if Vulkan unavailable
