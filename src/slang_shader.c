// Slang shader support for Vulkan renderer
// Implements RetroArch-compatible slang shader loading and multi-pass rendering

#include "slang_shader.h"
#include "slang_compiler.h"
#include "util.h"
#include "config.h"
#include "platform.h"
#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef SLANG_SHADERS_AVAILABLE

#include <vulkan/vulkan.h>

// Use stb_image for LUT texture loading (already defined in glsl_shader.c, so just declare)
unsigned char *stbi_load(char const *filename, int *x, int *y, int *channels_in_file, int desired_channels);
void stbi_image_free(void *retval_from_stbi_load);

// Constants
enum {
  kSlangMaxPasses = 20,
  kSlangMaxTextures = 16,
  kSlangMaxHistory = 8,
  kSlangMaxParams = 64,
};

// Scale types (matching GLSL shader conventions)
typedef enum SlangScaleType {
  SLANG_SCALE_NONE = 0,
  SLANG_SCALE_SOURCE,
  SLANG_SCALE_VIEWPORT,
  SLANG_SCALE_ABSOLUTE
} SlangScaleType;

// Wrap modes for Vulkan samplers
typedef enum SlangWrapMode {
  SLANG_WRAP_CLAMP_TO_BORDER = 0,
  SLANG_WRAP_CLAMP_TO_EDGE,
  SLANG_WRAP_REPEAT,
  SLANG_WRAP_MIRRORED_REPEAT
} SlangWrapMode;

// Filter modes
typedef enum SlangFilterMode {
  SLANG_FILTER_UNSPECIFIED = 0,
  SLANG_FILTER_NEAREST,
  SLANG_FILTER_LINEAR
} SlangFilterMode;

// Shader pass configuration (parsed from preset)
typedef struct SlangPassConfig {
  char *filename;          // Path to .slang file
  char *alias;             // Optional alias for referencing

  // Scale configuration
  SlangScaleType scale_type_x;
  SlangScaleType scale_type_y;
  float scale_x;
  float scale_y;

  // Framebuffer configuration
  VkFormat format;
  bool float_framebuffer;
  bool srgb_framebuffer;
  bool mipmap_input;

  // Sampling
  SlangFilterMode filter;
  SlangWrapMode wrap_mode;
  uint32_t frame_count_mod;
} SlangPassConfig;

// Vulkan resources for a single pass
typedef struct SlangPass {
  SlangPassConfig config;

  // Compiled shaders
  VkShaderModule vert_module;
  VkShaderModule frag_module;

  // Pipeline
  VkPipeline pipeline;
  VkPipelineLayout pipeline_layout;
  VkRenderPass render_pass;
  VkDescriptorSetLayout descriptor_set_layout;
  VkDescriptorSet descriptor_set;

  // Output framebuffer (intermediate passes only)
  VkImage output_image;
  VkDeviceMemory output_memory;
  VkImageView output_view;
  VkFramebuffer framebuffer;
  VkSampler sampler;

  // Current dimensions (computed per-frame)
  uint16_t width;
  uint16_t height;

  // UBO buffer for this pass
  VkBuffer ubo_buffer;
  VkDeviceMemory ubo_memory;

  // Push constant size (from shader reflection or default)
  uint32_t push_constant_size;

  // Push constant member names in order (for parameter lookup)
  char **push_const_names;
  int push_const_count;
} SlangPass;

// Standard UBO layout matching RetroArch slang spec
// Must match std140 layout rules
typedef struct SlangUBO {
  float mvp[16];         // mat4 MVP
  float output_size[4];  // vec4 OutputSize (w, h, 1/w, 1/h)
  float source_size[4];  // vec4 SourceSize (w, h, 1/w, 1/h)
  uint32_t frame_count;  // uint FrameCount
  float padding[3];      // Padding to 16-byte alignment
} SlangUBO;

// Maximum push constant size (128 bytes is Vulkan minimum guaranteed)
#define SLANG_MAX_PUSH_CONSTANT_SIZE 128

// Push constant data - crt-geom layout
// Most slang shaders put FrameCount first, then shader parameters
// Size info (SourceSize, OutputSize) comes from UBO, not push constants
typedef struct SlangPushConstants {
  uint32_t frame_count;     // uint FrameCount (offset 0)
  float params[31];         // Shader-specific parameters (CRTgamma, monitorgamma, etc.)
} SlangPushConstants;

// LUT texture
typedef struct SlangTexture {
  struct SlangTexture *next;
  char *id;                // Identifier for binding
  char *filename;          // Path to PNG file

  SlangFilterMode filter;
  SlangWrapMode wrap_mode;
  bool mipmap;

  // Vulkan resources
  VkImage image;
  VkDeviceMemory memory;
  VkImageView view;
  VkSampler sampler;
  int width;
  int height;
} SlangTexture;

// Shader parameter
typedef struct SlangParam {
  struct SlangParam *next;
  char *id;
  bool has_value;
  float value;
  float min;
  float max;
  float step;
} SlangParam;

// Frame history entry
typedef struct SlangHistoryFrame {
  VkImage image;
  VkDeviceMemory memory;
  VkImageView view;
  uint16_t width;
  uint16_t height;
} SlangHistoryFrame;

// Main shader structure
struct SlangShader {
  // Pass configuration
  int n_pass;
  SlangPass *pass;

  // Textures and parameters
  SlangTexture *first_texture;
  SlangParam *first_param;

  // Frame history
  int max_prev_frame;
  SlangHistoryFrame prev_frame[kSlangMaxHistory];

  // Shared Vulkan resources
  VkDevice device;
  VkPhysicalDevice physical_device;
  VkCommandPool command_pool;
  VkQueue graphics_queue;
  uint32_t graphics_family;
  VkFormat swapchain_format;
  VkDescriptorPool descriptor_pool;

  // Vertex buffer for fullscreen quad
  VkBuffer vertex_buffer;
  VkDeviceMemory vertex_buffer_memory;
  VkBuffer index_buffer;
  VkDeviceMemory index_buffer_memory;

  // Frame counter
  uint32_t frame_count;

  // Base path for resolving relative paths
  char *base_path;

  // Cached output render pass for final pass pipeline compatibility
  VkRenderPass output_render_pass;
  bool final_pass_pipeline_valid;
};

// ============================================================================
// Utility functions
// ============================================================================

// These will be used in phase 3-4 when pipelines are created
static VkSamplerAddressMode SlangWrapModeToVk(SlangWrapMode mode) __attribute__((unused));
static VkSamplerAddressMode SlangWrapModeToVk(SlangWrapMode mode) {
  switch (mode) {
    case SLANG_WRAP_REPEAT: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case SLANG_WRAP_CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case SLANG_WRAP_MIRRORED_REPEAT: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    default: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  }
}

static VkFilter SlangFilterModeToVk(SlangFilterMode mode) __attribute__((unused));
static VkFilter SlangFilterModeToVk(SlangFilterMode mode) {
  return (mode == SLANG_FILTER_LINEAR) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
}

// ============================================================================
// Preset file parsing
// ============================================================================

static SlangScaleType ParseScaleType(const char *s) {
  if (StringEqualsNoCase(s, "source")) return SLANG_SCALE_SOURCE;
  if (StringEqualsNoCase(s, "viewport")) return SLANG_SCALE_VIEWPORT;
  if (StringEqualsNoCase(s, "absolute")) return SLANG_SCALE_ABSOLUTE;
  return SLANG_SCALE_NONE;
}

static SlangWrapMode ParseWrapMode(const char *s) {
  if (StringEqualsNoCase(s, "repeat")) return SLANG_WRAP_REPEAT;
  if (StringEqualsNoCase(s, "clamp_to_edge")) return SLANG_WRAP_CLAMP_TO_EDGE;
  if (StringEqualsNoCase(s, "clamp")) return SLANG_WRAP_CLAMP_TO_EDGE;
  if (StringEqualsNoCase(s, "mirrored_repeat")) return SLANG_WRAP_MIRRORED_REPEAT;
  return SLANG_WRAP_CLAMP_TO_BORDER;
}

static VkFormat ParseFormat(const char *s) {
  // Common formats used in slang shaders
  if (StringEqualsNoCase(s, "R8G8B8A8_UNORM")) return VK_FORMAT_R8G8B8A8_UNORM;
  if (StringEqualsNoCase(s, "R8G8B8A8_SRGB")) return VK_FORMAT_R8G8B8A8_SRGB;
  if (StringEqualsNoCase(s, "R16G16B16A16_SFLOAT")) return VK_FORMAT_R16G16B16A16_SFLOAT;
  if (StringEqualsNoCase(s, "R32G32B32A32_SFLOAT")) return VK_FORMAT_R32G32B32A32_SFLOAT;
  if (StringEqualsNoCase(s, "R8_UNORM")) return VK_FORMAT_R8_UNORM;
  if (StringEqualsNoCase(s, "R16_SFLOAT")) return VK_FORMAT_R16_SFLOAT;
  if (StringEqualsNoCase(s, "R32_SFLOAT")) return VK_FORMAT_R32_SFLOAT;
  return VK_FORMAT_R8G8B8A8_UNORM;  // Default
}

static SlangPassConfig *ParseConfigKeyPass(SlangShader *ss, const char *key, const char *match) {
  char *endp;
  for (; *match; key++, match++) {
    if (*key != *match)
      return NULL;
  }
  if ((uint8_t)(*key - '0') >= 10)
    return NULL;
  uint32_t pass = strtoul(key, &endp, 10);
  if (pass >= (uint32_t)ss->n_pass || *endp != 0)
    return NULL;
  return &ss->pass[pass].config;
}

static void ParseTextures(SlangShader *ss, char *value) {
  char *id;
  SlangTexture **nextp = &ss->first_texture;
  for (int num = 0; (id = NextDelim(&value, ';')) != NULL && num < kSlangMaxTextures; num++) {
    SlangTexture *t = calloc(sizeof(SlangTexture), 1);
    if (!t) {
      LogError("calloc failed: SlangTexture");
      return;
    }
    t->id = strdup(id);
    if (!t->id) {
      free(t);
      LogError("strdup failed: texture id");
      return;
    }
    t->wrap_mode = SLANG_WRAP_CLAMP_TO_BORDER;
    t->filter = SLANG_FILTER_NEAREST;
    *nextp = t;
    nextp = &t->next;
  }
}

static bool ParseTextureKeyValue(SlangShader *ss, const char *key, const char *value) {
  for (SlangTexture *t = ss->first_texture; t != NULL; t = t->next) {
    const char *key2 = SkipPrefix(key, t->id);
    if (!key2) continue;
    if (*key2 == 0) {
      StrSet(&t->filename, value);
      return true;
    } else if (!strcmp(key2, "_wrap_mode")) {
      t->wrap_mode = ParseWrapMode(value);
      return true;
    } else if (!strcmp(key2, "_mipmap")) {
      t->mipmap = ParseBool(value, NULL);
      return true;
    } else if (!strcmp(key2, "_linear")) {
      t->filter = ParseBool(value, NULL) ? SLANG_FILTER_LINEAR : SLANG_FILTER_NEAREST;
      return true;
    }
  }
  return false;
}

static SlangParam *SlangShader_GetParam(SlangShader *ss, const char *id) {
  SlangParam **pp = &ss->first_param;
  for (; (*pp) != NULL; pp = &(*pp)->next)
    if (!strcmp((*pp)->id, id))
      return *pp;
  SlangParam *p = (SlangParam *)calloc(1, sizeof(SlangParam));
  if (!p) {
    LogError("calloc failed: SlangParam");
    return NULL;
  }
  *pp = p;
  p->id = strdup(id);
  if (!p->id) {
    LogError("strdup failed: parameter id");
    return NULL;
  }
  return p;
}

static void ParseParameters(SlangShader *ss, char *value) {
  char *id;
  while ((id = NextDelim(&value, ';')) != NULL)
    SlangShader_GetParam(ss, id);
}

static bool ParseParameterKeyValue(SlangShader *ss, const char *key, const char *value) {
  for (SlangParam *p = ss->first_param; p != NULL; p = p->next) {
    if (strcmp(p->id, key) == 0) {
      p->value = (float)atof(value);
      p->has_value = true;
      return true;
    }
  }
  return false;
}

static void SlangPassConfig_Initialize(SlangPassConfig *config) {
  memset(config, 0, sizeof(*config));
  config->scale_x = 1.0f;
  config->scale_y = 1.0f;
  config->wrap_mode = SLANG_WRAP_CLAMP_TO_BORDER;
  config->format = VK_FORMAT_R8G8B8A8_UNORM;
}

static bool SlangShader_InitializePasses(SlangShader *ss, int passes) {
  if (passes < 1 || passes > kSlangMaxPasses) {
    LogError("Invalid pass count: %d (must be 1-%d)", passes, kSlangMaxPasses);
    return false;
  }

  ss->n_pass = passes;
  ss->pass = (SlangPass *)calloc(passes, sizeof(SlangPass));
  if (!ss->pass) {
    LogError("Failed to allocate memory for %d shader passes", passes);
    ss->n_pass = 0;
    return false;
  }

  for (int i = 0; i < passes; i++) {
    SlangPassConfig_Initialize(&ss->pass[i].config);
  }

  return true;
}

static bool SlangShader_ReadPresetFile(SlangShader *ss, const char *filename) {
  char *data = (char *)Platform_ReadWholeFile(filename, NULL);
  char *data_org = data;
  char *line;
  SlangPassConfig *pass_config;

  if (data == NULL) {
    LogError("Unable to read preset file '%s'", filename);
    return false;
  }

  // Store base path for resolving relative shader paths
  ss->base_path = strdup(filename);

  for (int lineno = 1; (line = NextLineStripComments(&data)) != NULL; lineno++) {
    char *value = SplitKeyValue(line);
    char *t;

    if (value == NULL) {
      if (*line)
        LogError("%s:%d: Expecting key=value", filename, lineno);
      continue;
    }

    // Strip quotes from value
    if (*value == '"') {
      for (t = ++value; *t && *t != '"'; t++);
      if (*t) *t = 0;
    }

    // First line must be "shaders = N"
    if (ss->n_pass == 0) {
      if (strcmp(line, "shaders") != 0) {
        LogError("%s:%d: Expecting 'shaders'", filename, lineno);
        break;
      }
      int passes = (int)strtoul(value, NULL, 10);
      if (!SlangShader_InitializePasses(ss, passes))
        break;
      continue;
    }

    // Parse pass-specific options
    if ((pass_config = ParseConfigKeyPass(ss, line, "filter_linear")) != NULL)
      pass_config->filter = ParseBool(value, NULL) ? SLANG_FILTER_LINEAR : SLANG_FILTER_NEAREST;
    else if ((pass_config = ParseConfigKeyPass(ss, line, "scale_type")) != NULL)
      pass_config->scale_type_x = pass_config->scale_type_y = ParseScaleType(value);
    else if ((pass_config = ParseConfigKeyPass(ss, line, "scale_type_x")) != NULL)
      pass_config->scale_type_x = ParseScaleType(value);
    else if ((pass_config = ParseConfigKeyPass(ss, line, "scale_type_y")) != NULL)
      pass_config->scale_type_y = ParseScaleType(value);
    else if ((pass_config = ParseConfigKeyPass(ss, line, "scale")) != NULL)
      pass_config->scale_x = pass_config->scale_y = (float)atof(value);
    else if ((pass_config = ParseConfigKeyPass(ss, line, "scale_x")) != NULL)
      pass_config->scale_x = (float)atof(value);
    else if ((pass_config = ParseConfigKeyPass(ss, line, "scale_y")) != NULL)
      pass_config->scale_y = (float)atof(value);
    else if ((pass_config = ParseConfigKeyPass(ss, line, "shader")) != NULL)
      StrSet(&pass_config->filename, value);
    else if ((pass_config = ParseConfigKeyPass(ss, line, "wrap_mode")) != NULL)
      pass_config->wrap_mode = ParseWrapMode(value);
    else if ((pass_config = ParseConfigKeyPass(ss, line, "mipmap_input")) != NULL)
      pass_config->mipmap_input = ParseBool(value, NULL);
    else if ((pass_config = ParseConfigKeyPass(ss, line, "frame_count_mod")) != NULL)
      pass_config->frame_count_mod = (uint32_t)atoi(value);
    else if ((pass_config = ParseConfigKeyPass(ss, line, "float_framebuffer")) != NULL)
      pass_config->float_framebuffer = ParseBool(value, NULL);
    else if ((pass_config = ParseConfigKeyPass(ss, line, "srgb_framebuffer")) != NULL)
      pass_config->srgb_framebuffer = ParseBool(value, NULL);
    else if ((pass_config = ParseConfigKeyPass(ss, line, "alias")) != NULL)
      StrSet(&pass_config->alias, value);
    else if (strcmp(line, "textures") == 0 && ss->first_texture == NULL)
      ParseTextures(ss, value);
    else if (strcmp(line, "parameters") == 0)
      ParseParameters(ss, value);
    else if (!ParseTextureKeyValue(ss, line, value) && !ParseParameterKeyValue(ss, line, value)) {
      // Silently ignore unknown keys (there are many RetroArch-specific ones)
    }
  }

  free(data_org);

  if (ss->n_pass == 0) {
    LogError("No shader passes defined in '%s'", filename);
    return false;
  }

  // Validate all passes have shader files
  for (int i = 0; i < ss->n_pass; i++) {
    if (ss->pass[i].config.filename == NULL) {
      LogError("shader%d not specified in '%s'", i, filename);
      return false;
    }
  }

  LogInfo("Loaded slang preset '%s' with %d passes", filename, ss->n_pass);
  return true;
}

// ============================================================================
// Slang file parsing (#pragma extraction)
// ============================================================================

typedef struct SlangShaderSource {
  ByteArray vertex_source;
  ByteArray fragment_source;
  char *name;
  VkFormat format;
  // Push constant member names in struct order (excluding FrameCount)
  char **push_const_names;
  int push_const_count;
} SlangShaderSource;

static void SlangShaderSource_Init(SlangShaderSource *src) {
  memset(src, 0, sizeof(*src));
  src->format = VK_FORMAT_R8G8B8A8_UNORM;
}

static void SlangShaderSource_Destroy(SlangShaderSource *src) {
  ByteArray_Destroy(&src->vertex_source);
  ByteArray_Destroy(&src->fragment_source);
  free(src->name);
  for (int i = 0; i < src->push_const_count; i++) {
    free(src->push_const_names[i]);
  }
  free(src->push_const_names);
}

// Parse push constant struct from shader source to get member names in order
// This is needed because push constant layout order != #pragma parameter order
static void ParsePushConstantLayout(const char *shader_src, SlangShaderSource *result) {
  // Find "layout(push_constant)" and then the struct body
  const char *pc = strstr(shader_src, "layout(push_constant)");
  if (!pc) return;

  // Find opening brace
  const char *brace = strchr(pc, '{');
  if (!brace) return;
  brace++;

  // Find closing brace
  const char *end_brace = strchr(brace, '}');
  if (!end_brace) return;

  // Allocate space for names (max 32 should be plenty)
  result->push_const_names = calloc(32, sizeof(char *));
  result->push_const_count = 0;

  // Parse each line between braces
  const char *p = brace;
  while (p < end_brace && result->push_const_count < 32) {
    // Skip whitespace
    while (p < end_brace && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    if (p >= end_brace) break;

    // Find end of line (semicolon)
    const char *semi = strchr(p, ';');
    if (!semi || semi > end_brace) break;

    // Parse "type name;" - we want the name (last word before semicolon)
    // Skip type (uint, float, vec4, etc)
    const char *type_end = p;
    while (type_end < semi && *type_end != ' ' && *type_end != '\t') type_end++;

    // Skip whitespace after type
    const char *name_start = type_end;
    while (name_start < semi && (*name_start == ' ' || *name_start == '\t')) name_start++;

    // Find end of name (before semicolon, skip any trailing whitespace)
    const char *name_end = semi;
    while (name_end > name_start && (*(name_end-1) == ' ' || *(name_end-1) == '\t')) name_end--;

    if (name_end > name_start) {
      size_t name_len = name_end - name_start;
      char *name = malloc(name_len + 1);
      memcpy(name, name_start, name_len);
      name[name_len] = '\0';

      // Skip FrameCount - it's handled separately
      if (strcmp(name, "FrameCount") != 0) {
        result->push_const_names[result->push_const_count++] = name;
      } else {
        free(name);
      }
    }

    p = semi + 1;
  }
}

// Parse a .slang file and extract vertex/fragment sources
static bool SlangShader_ReadSlangFile(SlangShader *ss, const char *filename, SlangShaderSource *result) {
  char *data = (char *)Platform_ReadWholeFile(filename, NULL);
  char *data_org = data;
  char *line;

  if (data == NULL) {
    LogError("Unable to read shader file '%s'", filename);
    return false;
  }

  SlangShaderSource_Init(result);

  // Parse push constant layout BEFORE modifying the buffer with NextDelim
  ParsePushConstantLayout(data, result);

  // Track if we've seen a #pragma stage directive yet
  // Lines before any stage directive are "preamble" that goes to both shaders
  bool seen_stage_directive = false;
  ByteArray preamble = {0};

  // Default to vertex stage after preamble (slang files typically start with vertex)
  ByteArray *current_stage = &result->vertex_source;

  // Check if shader already has #version directive
  bool has_version = strstr(data, "#version") != NULL;

  // Add version directive only if shader doesn't have one
  if (!has_version) {
    const char *version = "#version 450\n";
    ByteArray_AppendData(&preamble, (const uint8_t *)version, strlen(version));
  }

  while ((line = NextDelim(&data, '\n')) != NULL) {
    size_t linelen = strlen(line);

    // Handle #pragma directives
    if (linelen >= 7 && memcmp(line, "#pragma", 7) == 0) {
      char *pragma = line + 7;
      while (*pragma == ' ' || *pragma == '\t') pragma++;

      // #pragma stage vertex|fragment
      if (strncmp(pragma, "stage", 5) == 0) {
        char *stage = pragma + 5;
        while (*stage == ' ' || *stage == '\t') stage++;

        // First stage directive: copy preamble to both shaders
        if (!seen_stage_directive) {
          seen_stage_directive = true;
          ByteArray_AppendData(&result->vertex_source, preamble.data, preamble.size);
          ByteArray_AppendData(&result->fragment_source, preamble.data, preamble.size);
        }

        if (strncmp(stage, "vertex", 6) == 0) {
          current_stage = &result->vertex_source;
        } else if (strncmp(stage, "fragment", 8) == 0) {
          current_stage = &result->fragment_source;
        }
        continue;  // Don't add pragma to output
      }

      // #pragma name ShaderName
      if (strncmp(pragma, "name", 4) == 0) {
        char *name = pragma + 4;
        while (*name == ' ' || *name == '\t') name++;
        // Trim trailing whitespace
        char *end = name + strlen(name) - 1;
        while (end > name && (*end == ' ' || *end == '\t' || *end == '\r')) *end-- = 0;
        result->name = strdup(name);
        continue;
      }

      // #pragma format FORMAT
      if (strncmp(pragma, "format", 6) == 0) {
        char *fmt = pragma + 6;
        while (*fmt == ' ' || *fmt == '\t') fmt++;
        result->format = ParseFormat(fmt);
        continue;
      }

      // #pragma parameter id "desc" default min max step
      if (strncmp(pragma, "parameter", 9) == 0) {
        char *tt = pragma + 9;
        char *param_id = NextPossiblyQuotedString(&tt);
        if (param_id && *param_id) {
          SlangParam *param = SlangShader_GetParam(ss, param_id);
          if (param) {
            NextPossiblyQuotedString(&tt);  // Skip description
            char *val_str = NextPossiblyQuotedString(&tt);
            if (val_str && !param->has_value) {
              param->value = (float)atof(val_str);
              LogInfo("Parsed parameter: %s = %f", param_id, param->value);
            }
            val_str = NextPossiblyQuotedString(&tt);
            if (val_str) param->min = (float)atof(val_str);
            val_str = NextPossiblyQuotedString(&tt);
            if (val_str) param->max = (float)atof(val_str);
            val_str = NextPossiblyQuotedString(&tt);
            if (val_str) param->step = (float)atof(val_str);
          }
        }
        continue;
      }
    }

    // Handle #include
    if (linelen >= 8 && memcmp(line, "#include", 8) == 0) {
      char *tt = line + 8;
      char *include_path = NextPossiblyQuotedString(&tt);
      if (include_path && *include_path) {
        char *full_path = ReplaceFilenameWithNewPath(filename, include_path);
        // Recursively read included file (simplified - append to current stage)
        char *inc_data = (char *)Platform_ReadWholeFile(full_path, NULL);
        if (inc_data) {
          ByteArray_AppendData(current_stage, (const uint8_t *)inc_data, strlen(inc_data));
          ByteArray_AppendByte(current_stage, '\n');
          free(inc_data);
        } else {
          LogError("Unable to include '%s' from '%s'", include_path, filename);
        }
        free(full_path);
      }
      continue;
    }

    // Regular line - add to preamble if before stage directive, else to current stage
    line[linelen] = '\n';
    if (!seen_stage_directive) {
      ByteArray_AppendData(&preamble, (const uint8_t *)line, linelen + 1);
    } else {
      ByteArray_AppendData(current_stage, (const uint8_t *)line, linelen + 1);
    }
  }

  // If no stage directives were seen, the preamble IS the vertex shader
  // (This handles simple shaders without #pragma stage)
  if (!seen_stage_directive) {
    ByteArray_AppendData(&result->vertex_source, preamble.data, preamble.size);
    ByteArray_AppendData(&result->fragment_source, preamble.data, preamble.size);
  }

  ByteArray_Destroy(&preamble);

  // Null-terminate the sources
  ByteArray_AppendByte(&result->vertex_source, 0);
  ByteArray_AppendByte(&result->fragment_source, 0);

  free(data_org);

  return result->vertex_source.size > 1 && result->fragment_source.size > 1;
}

// ============================================================================
// Vulkan resource creation
// ============================================================================

// Fullscreen quad vertices (position vec4 + texcoord vec2)
// Slang shaders expect vec4 Position at location 0
typedef struct SlangVertex {
  float pos[4];  // x, y, z, w
  float uv[2];   // u, v
} SlangVertex;

static const SlangVertex kQuadVertices[] = {
  {{-1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
  {{ 1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
  {{ 1.0f,  1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
  {{-1.0f,  1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
};

static const uint16_t kQuadIndices[] = {0, 1, 2, 2, 3, 0};

static bool CreateShaderModule(VkDevice device, const uint32_t *code, size_t size, VkShaderModule *module) {
  VkShaderModuleCreateInfo create_info = {0};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = size;
  create_info.pCode = code;

  return vkCreateShaderModule(device, &create_info, NULL, module) == VK_SUCCESS;
}

static uint32_t FindMemoryType(VkPhysicalDevice physical_device, uint32_t type_filter, VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties mem_props;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

  for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
    if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }
  return UINT32_MAX;
}

static bool CreateBuffer(SlangShader *ss, VkDeviceSize size, VkBufferUsageFlags usage,
                         VkMemoryPropertyFlags properties, VkBuffer *buffer, VkDeviceMemory *memory) {
  VkBufferCreateInfo buffer_info = {0};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = size;
  buffer_info.usage = usage;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(ss->device, &buffer_info, NULL, buffer) != VK_SUCCESS) {
    return false;
  }

  VkMemoryRequirements mem_reqs;
  vkGetBufferMemoryRequirements(ss->device, *buffer, &mem_reqs);

  VkMemoryAllocateInfo alloc_info = {0};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = mem_reqs.size;
  alloc_info.memoryTypeIndex = FindMemoryType(ss->physical_device, mem_reqs.memoryTypeBits, properties);

  if (alloc_info.memoryTypeIndex == UINT32_MAX) {
    vkDestroyBuffer(ss->device, *buffer, NULL);
    return false;
  }

  if (vkAllocateMemory(ss->device, &alloc_info, NULL, memory) != VK_SUCCESS) {
    vkDestroyBuffer(ss->device, *buffer, NULL);
    return false;
  }

  vkBindBufferMemory(ss->device, *buffer, *memory, 0);
  return true;
}

static bool CreateVertexAndIndexBuffers(SlangShader *ss) {
  // Create vertex buffer
  VkDeviceSize vertex_size = sizeof(kQuadVertices);
  if (!CreateBuffer(ss, vertex_size,
                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    &ss->vertex_buffer, &ss->vertex_buffer_memory)) {
    LogError("Failed to create vertex buffer");
    return false;
  }

  // Upload vertex data
  void *data;
  vkMapMemory(ss->device, ss->vertex_buffer_memory, 0, vertex_size, 0, &data);
  memcpy(data, kQuadVertices, vertex_size);
  vkUnmapMemory(ss->device, ss->vertex_buffer_memory);

  // Create index buffer
  VkDeviceSize index_size = sizeof(kQuadIndices);
  if (!CreateBuffer(ss, index_size,
                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    &ss->index_buffer, &ss->index_buffer_memory)) {
    LogError("Failed to create index buffer");
    return false;
  }

  // Upload index data
  vkMapMemory(ss->device, ss->index_buffer_memory, 0, index_size, 0, &data);
  memcpy(data, kQuadIndices, index_size);
  vkUnmapMemory(ss->device, ss->index_buffer_memory);

  return true;
}

static bool CreateDescriptorPool(SlangShader *ss) {
  // Pool sizes: UBOs + samplers per pass + LUT textures
  VkDescriptorPoolSize pool_sizes[2] = {0};

  // UBO descriptors (one per pass)
  pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_sizes[0].descriptorCount = (uint32_t)(ss->n_pass + 1);

  // Sampler descriptors (one per pass + LUT textures)
  pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  pool_sizes[1].descriptorCount = (uint32_t)(ss->n_pass * 2 + kSlangMaxTextures);

  VkDescriptorPoolCreateInfo pool_info = {0};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = 2;
  pool_info.pPoolSizes = pool_sizes;
  pool_info.maxSets = (uint32_t)(ss->n_pass + kSlangMaxTextures);

  if (vkCreateDescriptorPool(ss->device, &pool_info, NULL, &ss->descriptor_pool) != VK_SUCCESS) {
    LogError("Failed to create descriptor pool");
    return false;
  }

  return true;
}

static bool CreatePassRenderPass(SlangShader *ss, int pass_idx, VkFormat format, bool is_final) {
  SlangPass *pass = &ss->pass[pass_idx];

  VkAttachmentDescription color_attachment = {0};
  color_attachment.format = format;
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = is_final ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentReference color_ref = {0};
  color_ref.attachment = 0;
  color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {0};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_ref;

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

  if (vkCreateRenderPass(ss->device, &render_pass_info, NULL, &pass->render_pass) != VK_SUCCESS) {
    LogError("Failed to create render pass for pass %d", pass_idx);
    return false;
  }

  return true;
}

static bool CreatePassUBO(SlangShader *ss, int pass_idx) {
  SlangPass *pass = &ss->pass[pass_idx];

  // Create UBO buffer (host-visible for easy updates)
  if (!CreateBuffer(ss, sizeof(SlangUBO),
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    &pass->ubo_buffer, &pass->ubo_memory)) {
    LogError("Failed to create UBO buffer for pass %d", pass_idx);
    return false;
  }

  // Set default push constant size (shader may override via reflection)
  pass->push_constant_size = SLANG_MAX_PUSH_CONSTANT_SIZE;

  return true;
}

static bool CreatePassDescriptorSetLayout(SlangShader *ss, int pass_idx) {
  SlangPass *pass = &ss->pass[pass_idx];

  // Two bindings: UBO at binding 0, sampler at binding 2 (standard slang layout)
  VkDescriptorSetLayoutBinding bindings[2] = {0};

  // Binding 0: UBO (accessed by both vertex and fragment)
  bindings[0].binding = 0;
  bindings[0].descriptorCount = 1;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

  // Binding 2: Combined image sampler for Source texture (slang convention)
  bindings[1].binding = 2;
  bindings[1].descriptorCount = 1;
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo layout_info = {0};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = 2;
  layout_info.pBindings = bindings;

  if (vkCreateDescriptorSetLayout(ss->device, &layout_info, NULL, &pass->descriptor_set_layout) != VK_SUCCESS) {
    LogError("Failed to create descriptor set layout for pass %d", pass_idx);
    return false;
  }

  return true;
}

// Create pipeline for a pass, optionally using an override render pass
static bool CreatePassPipelineWithRenderPass(SlangShader *ss, int pass_idx, VkRenderPass render_pass_override) {
  SlangPass *pass = &ss->pass[pass_idx];
  VkRenderPass render_pass = render_pass_override ? render_pass_override : pass->render_pass;
  LogWarn("CreatePipeline: pass=%d, override=%p, using rp=%p", pass_idx, (void*)render_pass_override, (void*)render_pass);

  // Shader stages
  VkPipelineShaderStageCreateInfo vert_stage = {0};
  vert_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vert_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vert_stage.module = pass->vert_module;
  vert_stage.pName = "main";

  VkPipelineShaderStageCreateInfo frag_stage = {0};
  frag_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  frag_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  frag_stage.module = pass->frag_module;
  frag_stage.pName = "main";

  VkPipelineShaderStageCreateInfo shader_stages[] = {vert_stage, frag_stage};

  // Vertex input
  VkVertexInputBindingDescription binding = {0};
  binding.binding = 0;
  binding.stride = sizeof(SlangVertex);
  binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription attributes[2] = {0};
  attributes[0].binding = 0;
  attributes[0].location = 0;
  attributes[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;  // vec4 Position
  attributes[0].offset = offsetof(SlangVertex, pos);
  attributes[1].binding = 0;
  attributes[1].location = 1;
  attributes[1].format = VK_FORMAT_R32G32_SFLOAT;  // vec2 TexCoord
  attributes[1].offset = offsetof(SlangVertex, uv);

  VkPipelineVertexInputStateCreateInfo vertex_input = {0};
  vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input.vertexBindingDescriptionCount = 1;
  vertex_input.pVertexBindingDescriptions = &binding;
  vertex_input.vertexAttributeDescriptionCount = 2;
  vertex_input.pVertexAttributeDescriptions = attributes;

  VkPipelineInputAssemblyStateCreateInfo input_assembly = {0};
  input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  // Dynamic viewport/scissor
  VkPipelineViewportStateCreateInfo viewport_state = {0};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.scissorCount = 1;

  VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic_state = {0};
  dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount = 2;
  dynamic_state.pDynamicStates = dynamic_states;

  VkPipelineRasterizationStateCreateInfo rasterizer = {0};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_NONE;  // No culling for fullscreen quad
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

  VkPipelineMultisampleStateCreateInfo multisampling = {0};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState blend_attachment = {0};
  blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  blend_attachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo color_blending = {0};
  color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &blend_attachment;

  // Push constant range - accessible by both vertex and fragment shaders
  VkPushConstantRange push_constant_range = {0};
  push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  push_constant_range.offset = 0;
  push_constant_range.size = pass->push_constant_size;

  // Pipeline layout with push constants
  VkPipelineLayoutCreateInfo layout_info = {0};
  layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layout_info.setLayoutCount = 1;
  layout_info.pSetLayouts = &pass->descriptor_set_layout;
  layout_info.pushConstantRangeCount = 1;
  layout_info.pPushConstantRanges = &push_constant_range;

  if (vkCreatePipelineLayout(ss->device, &layout_info, NULL, &pass->pipeline_layout) != VK_SUCCESS) {
    LogError("Failed to create pipeline layout for pass %d", pass_idx);
    return false;
  }

  // Graphics pipeline
  VkGraphicsPipelineCreateInfo pipeline_info = {0};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = 2;
  pipeline_info.pStages = shader_stages;
  pipeline_info.pVertexInputState = &vertex_input;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterizer;
  pipeline_info.pMultisampleState = &multisampling;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.pDynamicState = &dynamic_state;
  pipeline_info.layout = pass->pipeline_layout;
  pipeline_info.renderPass = render_pass;
  pipeline_info.subpass = 0;

  VkResult result = vkCreateGraphicsPipelines(ss->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pass->pipeline);
  if (result != VK_SUCCESS) {
    LogError("Failed to create graphics pipeline for pass %d (VkResult=%d)", pass_idx, (int)result);
    return false;
  }

  return true;
}

static bool CreatePassPipeline(SlangShader *ss, int pass_idx) {
  return CreatePassPipelineWithRenderPass(ss, pass_idx, VK_NULL_HANDLE);
}

static bool CreatePassSampler(SlangShader *ss, int pass_idx) {
  SlangPass *pass = &ss->pass[pass_idx];

  VkSamplerCreateInfo sampler_info = {0};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = SlangFilterModeToVk(pass->config.filter);
  sampler_info.minFilter = SlangFilterModeToVk(pass->config.filter);
  sampler_info.addressModeU = SlangWrapModeToVk(pass->config.wrap_mode);
  sampler_info.addressModeV = SlangWrapModeToVk(pass->config.wrap_mode);
  sampler_info.addressModeW = SlangWrapModeToVk(pass->config.wrap_mode);
  sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
  sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

  if (vkCreateSampler(ss->device, &sampler_info, NULL, &pass->sampler) != VK_SUCCESS) {
    LogError("Failed to create sampler for pass %d", pass_idx);
    return false;
  }

  return true;
}

static bool CreatePassDescriptorSet(SlangShader *ss, int pass_idx) {
  SlangPass *pass = &ss->pass[pass_idx];

  VkDescriptorSetAllocateInfo alloc_info = {0};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = ss->descriptor_pool;
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts = &pass->descriptor_set_layout;

  if (vkAllocateDescriptorSets(ss->device, &alloc_info, &pass->descriptor_set) != VK_SUCCESS) {
    LogError("Failed to allocate descriptor set for pass %d", pass_idx);
    return false;
  }

  return true;
}

static bool CreateIntermediateFramebuffer(SlangShader *ss, int pass_idx, uint16_t width, uint16_t height) {
  SlangPass *pass = &ss->pass[pass_idx];

  // Destroy existing if present
  if (pass->framebuffer) {
    vkDestroyFramebuffer(ss->device, pass->framebuffer, NULL);
    pass->framebuffer = VK_NULL_HANDLE;
  }
  if (pass->output_view) {
    vkDestroyImageView(ss->device, pass->output_view, NULL);
    pass->output_view = VK_NULL_HANDLE;
  }
  if (pass->output_image) {
    vkDestroyImage(ss->device, pass->output_image, NULL);
    pass->output_image = VK_NULL_HANDLE;
  }
  if (pass->output_memory) {
    vkFreeMemory(ss->device, pass->output_memory, NULL);
    pass->output_memory = VK_NULL_HANDLE;
  }

  // Create image
  VkImageCreateInfo image_info = {0};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = pass->config.format;
  image_info.extent.width = width;
  image_info.extent.height = height;
  image_info.extent.depth = 1;
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  if (vkCreateImage(ss->device, &image_info, NULL, &pass->output_image) != VK_SUCCESS) {
    LogError("Failed to create intermediate image for pass %d", pass_idx);
    return false;
  }

  // Allocate memory
  VkMemoryRequirements mem_reqs;
  vkGetImageMemoryRequirements(ss->device, pass->output_image, &mem_reqs);

  VkMemoryAllocateInfo alloc_info = {0};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = mem_reqs.size;
  alloc_info.memoryTypeIndex = FindMemoryType(ss->physical_device, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  if (vkAllocateMemory(ss->device, &alloc_info, NULL, &pass->output_memory) != VK_SUCCESS) {
    LogError("Failed to allocate memory for pass %d framebuffer", pass_idx);
    return false;
  }

  vkBindImageMemory(ss->device, pass->output_image, pass->output_memory, 0);

  // Create image view
  VkImageViewCreateInfo view_info = {0};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = pass->output_image;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = pass->config.format;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;

  if (vkCreateImageView(ss->device, &view_info, NULL, &pass->output_view) != VK_SUCCESS) {
    LogError("Failed to create image view for pass %d", pass_idx);
    return false;
  }

  // Create framebuffer
  VkFramebufferCreateInfo fb_info = {0};
  fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fb_info.renderPass = pass->render_pass;
  fb_info.attachmentCount = 1;
  fb_info.pAttachments = &pass->output_view;
  fb_info.width = width;
  fb_info.height = height;
  fb_info.layers = 1;

  if (vkCreateFramebuffer(ss->device, &fb_info, NULL, &pass->framebuffer) != VK_SUCCESS) {
    LogError("Failed to create framebuffer for pass %d", pass_idx);
    return false;
  }

  pass->width = width;
  pass->height = height;

  return true;
}

// ============================================================================
// LUT texture loading
// ============================================================================

static bool LoadLutTexture(SlangShader *ss, SlangTexture *tex) {
  // Resolve path relative to preset
  char *full_path = ReplaceFilenameWithNewPath(ss->base_path, tex->filename);
  if (!full_path) {
    LogError("Failed to resolve LUT path: %s", tex->filename);
    return false;
  }

  // Load image with stb_image
  int width, height, channels;
  unsigned char *pixels = stbi_load(full_path, &width, &height, &channels, 4);
  if (!pixels) {
    LogError("Failed to load LUT texture: %s", full_path);
    free(full_path);
    return false;
  }

  tex->width = width;
  tex->height = height;

  VkDeviceSize image_size = (VkDeviceSize)width * (VkDeviceSize)height * 4;

  // Create staging buffer
  VkBuffer staging_buffer;
  VkDeviceMemory staging_memory;
  if (!CreateBuffer(ss, image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    &staging_buffer, &staging_memory)) {
    LogError("Failed to create staging buffer for LUT: %s", tex->id);
    stbi_image_free(pixels);
    free(full_path);
    return false;
  }

  // Copy pixel data to staging buffer
  void *data;
  vkMapMemory(ss->device, staging_memory, 0, image_size, 0, &data);
  memcpy(data, pixels, image_size);
  vkUnmapMemory(ss->device, staging_memory);
  stbi_image_free(pixels);

  // Create image
  VkImageCreateInfo image_info = {0};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  image_info.extent.width = (uint32_t)width;
  image_info.extent.height = (uint32_t)height;
  image_info.extent.depth = 1;
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  if (vkCreateImage(ss->device, &image_info, NULL, &tex->image) != VK_SUCCESS) {
    LogError("Failed to create LUT image: %s", tex->id);
    vkDestroyBuffer(ss->device, staging_buffer, NULL);
    vkFreeMemory(ss->device, staging_memory, NULL);
    free(full_path);
    return false;
  }

  // Allocate memory
  VkMemoryRequirements mem_reqs;
  vkGetImageMemoryRequirements(ss->device, tex->image, &mem_reqs);

  VkMemoryAllocateInfo alloc_info = {0};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = mem_reqs.size;
  alloc_info.memoryTypeIndex = FindMemoryType(ss->physical_device, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  if (vkAllocateMemory(ss->device, &alloc_info, NULL, &tex->memory) != VK_SUCCESS) {
    LogError("Failed to allocate LUT memory: %s", tex->id);
    vkDestroyImage(ss->device, tex->image, NULL);
    tex->image = VK_NULL_HANDLE;
    vkDestroyBuffer(ss->device, staging_buffer, NULL);
    vkFreeMemory(ss->device, staging_memory, NULL);
    free(full_path);
    return false;
  }

  vkBindImageMemory(ss->device, tex->image, tex->memory, 0);

  // Record and execute copy command
  VkCommandBufferAllocateInfo cmd_alloc = {0};
  cmd_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmd_alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmd_alloc.commandPool = ss->command_pool;
  cmd_alloc.commandBufferCount = 1;

  VkCommandBuffer cmd;
  vkAllocateCommandBuffers(ss->device, &cmd_alloc, &cmd);

  VkCommandBufferBeginInfo begin_info = {0};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmd, &begin_info);

  // Transition to transfer dst
  VkImageMemoryBarrier barrier = {0};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = tex->image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = 0;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       0, 0, NULL, 0, NULL, 1, &barrier);

  // Copy buffer to image
  VkBufferImageCopy region = {0};
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageExtent.width = (uint32_t)width;
  region.imageExtent.height = (uint32_t)height;
  region.imageExtent.depth = 1;

  vkCmdCopyBufferToImage(cmd, staging_buffer, tex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  // Transition to shader read
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                       0, 0, NULL, 0, NULL, 1, &barrier);

  vkEndCommandBuffer(cmd);

  // Submit and wait
  VkSubmitInfo submit = {0};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &cmd;

  vkQueueSubmit(ss->graphics_queue, 1, &submit, VK_NULL_HANDLE);
  vkQueueWaitIdle(ss->graphics_queue);

  vkFreeCommandBuffers(ss->device, ss->command_pool, 1, &cmd);
  vkDestroyBuffer(ss->device, staging_buffer, NULL);
  vkFreeMemory(ss->device, staging_memory, NULL);

  // Create image view
  VkImageViewCreateInfo view_info = {0};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = tex->image;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;

  if (vkCreateImageView(ss->device, &view_info, NULL, &tex->view) != VK_SUCCESS) {
    LogError("Failed to create LUT image view: %s", tex->id);
    free(full_path);
    return false;
  }

  // Create sampler
  VkSamplerCreateInfo sampler_info = {0};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = SlangFilterModeToVk(tex->filter);
  sampler_info.minFilter = SlangFilterModeToVk(tex->filter);
  sampler_info.addressModeU = SlangWrapModeToVk(tex->wrap_mode);
  sampler_info.addressModeV = SlangWrapModeToVk(tex->wrap_mode);
  sampler_info.addressModeW = SlangWrapModeToVk(tex->wrap_mode);
  sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
  sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

  if (vkCreateSampler(ss->device, &sampler_info, NULL, &tex->sampler) != VK_SUCCESS) {
    LogError("Failed to create LUT sampler: %s", tex->id);
    free(full_path);
    return false;
  }

  LogInfo("Loaded LUT texture '%s': %dx%d from %s", tex->id, width, height, full_path);
  free(full_path);
  return true;
}

static bool LoadAllLutTextures(SlangShader *ss) {
  for (SlangTexture *tex = ss->first_texture; tex != NULL; tex = tex->next) {
    if (tex->filename && *tex->filename) {
      if (!LoadLutTexture(ss, tex)) {
        return false;
      }
    }
  }
  return true;
}

static bool SlangShader_CompilePass(SlangShader *ss, int pass_idx) {
  SlangPass *pass = &ss->pass[pass_idx];
  char *shader_path = ReplaceFilenameWithNewPath(ss->base_path, pass->config.filename);

  SlangShaderSource source;
  if (!SlangShader_ReadSlangFile(ss, shader_path, &source)) {
    LogError("Failed to read shader file for pass %d", pass_idx);
    free(shader_path);
    return false;
  }

  // Override format from shader if specified
  if (source.format != VK_FORMAT_R8G8B8A8_UNORM) {
    pass->config.format = source.format;
  }

  // Copy push constant member names to pass (for parameter lookup)
  pass->push_const_names = source.push_const_names;
  pass->push_const_count = source.push_const_count;
  source.push_const_names = NULL;  // Transfer ownership
  source.push_const_count = 0;

  LogInfo("Compiling pass %d vertex shader (%zu bytes), %d push const params",
          pass_idx, source.vertex_source.size, pass->push_const_count);

  // Compile vertex shader
  SlangCompileResult vert_result = SlangCompiler_Compile(
    (const char *)source.vertex_source.data, SLANG_STAGE_VERTEX);
  if (!vert_result.success) {
    LogError("Vertex shader compilation failed for pass %d: %s",
             pass_idx, vert_result.error_message ? vert_result.error_message : "unknown error");
    SlangCompileResult_Destroy(&vert_result);
    SlangShaderSource_Destroy(&source);
    free(shader_path);
    return false;
  }

  LogInfo("Compiling pass %d fragment shader (%zu bytes)", pass_idx, source.fragment_source.size);

  // Compile fragment shader
  SlangCompileResult frag_result = SlangCompiler_Compile(
    (const char *)source.fragment_source.data, SLANG_STAGE_FRAGMENT);
  if (!frag_result.success) {
    LogError("Fragment shader compilation failed for pass %d: %s",
             pass_idx, frag_result.error_message ? frag_result.error_message : "unknown error");
    SlangCompileResult_Destroy(&vert_result);
    SlangCompileResult_Destroy(&frag_result);
    SlangShaderSource_Destroy(&source);
    free(shader_path);
    return false;
  }

  LogInfo("Pass %d shaders compiled: vert SPIRV=%zu bytes, frag SPIRV=%zu bytes",
          pass_idx, vert_result.spirv_size, frag_result.spirv_size);

  // Create shader modules
  if (!CreateShaderModule(ss->device, vert_result.spirv_code, vert_result.spirv_size, &pass->vert_module)) {
    LogError("Failed to create vertex shader module for pass %d", pass_idx);
    SlangCompileResult_Destroy(&vert_result);
    SlangCompileResult_Destroy(&frag_result);
    SlangShaderSource_Destroy(&source);
    free(shader_path);
    return false;
  }

  if (!CreateShaderModule(ss->device, frag_result.spirv_code, frag_result.spirv_size, &pass->frag_module)) {
    LogError("Failed to create fragment shader module for pass %d", pass_idx);
    vkDestroyShaderModule(ss->device, pass->vert_module, NULL);
    pass->vert_module = VK_NULL_HANDLE;
    SlangCompileResult_Destroy(&vert_result);
    SlangCompileResult_Destroy(&frag_result);
    SlangShaderSource_Destroy(&source);
    free(shader_path);
    return false;
  }

  LogInfo("Compiled pass %d: %s", pass_idx, pass->config.filename);

  SlangCompileResult_Destroy(&vert_result);
  SlangCompileResult_Destroy(&frag_result);
  SlangShaderSource_Destroy(&source);
  free(shader_path);
  return true;
}

// ============================================================================
// Public API
// ============================================================================

bool IsSlangPreset(const char *filename) {
  if (!filename) return false;
  size_t len = strlen(filename);
  return (len >= 7 && strcmp(filename + len - 7, ".slangp") == 0) ||
         (len >= 6 && strcmp(filename + len - 6, ".slang") == 0);
}

SlangShader *SlangShader_CreateFromFile(const char *filename, const SlangVulkanContext *vk_ctx) {
  if (!filename || !vk_ctx) {
    LogError("SlangShader_CreateFromFile: invalid arguments");
    return NULL;
  }

  // Initialize compiler
  if (!SlangCompiler_Init()) {
    LogError("Failed to initialize slang compiler");
    return NULL;
  }

  SlangShader *ss = (SlangShader *)calloc(1, sizeof(SlangShader));
  if (!ss) {
    LogError("Failed to allocate SlangShader");
    return NULL;
  }

  // Store Vulkan context
  ss->device = (VkDevice)vk_ctx->device;
  ss->physical_device = (VkPhysicalDevice)vk_ctx->physical_device;
  ss->command_pool = (VkCommandPool)vk_ctx->command_pool;
  ss->graphics_queue = (VkQueue)vk_ctx->graphics_queue;
  ss->graphics_family = vk_ctx->graphics_family;
  ss->swapchain_format = (VkFormat)vk_ctx->swapchain_format;

  // Parse preset file
  if (!SlangShader_ReadPresetFile(ss, filename)) {
    SlangShader_Destroy(ss);
    return NULL;
  }

  // Compile all passes
  for (int i = 0; i < ss->n_pass; i++) {
    if (!SlangShader_CompilePass(ss, i)) {
      SlangShader_Destroy(ss);
      return NULL;
    }
  }

  // Create shared Vulkan resources
  if (!CreateVertexAndIndexBuffers(ss)) {
    SlangShader_Destroy(ss);
    return NULL;
  }

  if (!CreateDescriptorPool(ss)) {
    SlangShader_Destroy(ss);
    return NULL;
  }

  // Create per-pass Vulkan resources
  for (int i = 0; i < ss->n_pass; i++) {
    bool is_final = (i == ss->n_pass - 1);

    // Create render pass (final pass uses swapchain format, others use pass-specific format)
    VkFormat format = is_final ? ss->swapchain_format : ss->pass[i].config.format;
    if (!CreatePassRenderPass(ss, i, format, is_final)) {
      SlangShader_Destroy(ss);
      return NULL;
    }

    if (!CreatePassDescriptorSetLayout(ss, i)) {
      SlangShader_Destroy(ss);
      return NULL;
    }

    if (!CreatePassUBO(ss, i)) {
      SlangShader_Destroy(ss);
      return NULL;
    }

    if (!CreatePassPipeline(ss, i)) {
      SlangShader_Destroy(ss);
      return NULL;
    }

    if (!CreatePassSampler(ss, i)) {
      SlangShader_Destroy(ss);
      return NULL;
    }

    if (!CreatePassDescriptorSet(ss, i)) {
      SlangShader_Destroy(ss);
      return NULL;
    }

    // Intermediate framebuffers will be created lazily when we know the actual dimensions
  }

  // Load LUT textures
  if (!LoadAllLutTextures(ss)) {
    LogError("Failed to load LUT textures");
    SlangShader_Destroy(ss);
    return NULL;
  }

  // Count parameters
  int param_count = 0;
  for (SlangParam *p = ss->first_param; p; p = p->next) param_count++;
  LogInfo("SlangShader created successfully with %d passes, %d parameters", ss->n_pass, param_count);
  return ss;
}

void SlangShader_Destroy(SlangShader *ss) {
  if (!ss) return;

  if (ss->device) {
    vkDeviceWaitIdle(ss->device);

    // Destroy passes
    for (int i = 0; i < ss->n_pass; i++) {
      SlangPass *pass = &ss->pass[i];

      if (pass->vert_module) vkDestroyShaderModule(ss->device, pass->vert_module, NULL);
      if (pass->frag_module) vkDestroyShaderModule(ss->device, pass->frag_module, NULL);
      if (pass->pipeline) vkDestroyPipeline(ss->device, pass->pipeline, NULL);
      if (pass->pipeline_layout) vkDestroyPipelineLayout(ss->device, pass->pipeline_layout, NULL);
      if (pass->render_pass) vkDestroyRenderPass(ss->device, pass->render_pass, NULL);
      if (pass->descriptor_set_layout) vkDestroyDescriptorSetLayout(ss->device, pass->descriptor_set_layout, NULL);
      if (pass->output_view) vkDestroyImageView(ss->device, pass->output_view, NULL);
      if (pass->output_image) vkDestroyImage(ss->device, pass->output_image, NULL);
      if (pass->output_memory) vkFreeMemory(ss->device, pass->output_memory, NULL);
      if (pass->framebuffer) vkDestroyFramebuffer(ss->device, pass->framebuffer, NULL);
      if (pass->sampler) vkDestroySampler(ss->device, pass->sampler, NULL);
      if (pass->ubo_buffer) vkDestroyBuffer(ss->device, pass->ubo_buffer, NULL);
      if (pass->ubo_memory) vkFreeMemory(ss->device, pass->ubo_memory, NULL);

      free(pass->config.filename);
      free(pass->config.alias);
      // Free push constant names
      for (int j = 0; j < pass->push_const_count; j++) {
        free(pass->push_const_names[j]);
      }
      free(pass->push_const_names);
    }
    free(ss->pass);

    // Destroy textures
    SlangTexture *tex = ss->first_texture;
    while (tex) {
      SlangTexture *next = tex->next;
      if (tex->view) vkDestroyImageView(ss->device, tex->view, NULL);
      if (tex->image) vkDestroyImage(ss->device, tex->image, NULL);
      if (tex->memory) vkFreeMemory(ss->device, tex->memory, NULL);
      if (tex->sampler) vkDestroySampler(ss->device, tex->sampler, NULL);
      free(tex->id);
      free(tex->filename);
      free(tex);
      tex = next;
    }

    // Destroy history frames
    for (int i = 0; i < kSlangMaxHistory; i++) {
      if (ss->prev_frame[i].view) vkDestroyImageView(ss->device, ss->prev_frame[i].view, NULL);
      if (ss->prev_frame[i].image) vkDestroyImage(ss->device, ss->prev_frame[i].image, NULL);
      if (ss->prev_frame[i].memory) vkFreeMemory(ss->device, ss->prev_frame[i].memory, NULL);
    }

    // Destroy shared resources
    if (ss->descriptor_pool) vkDestroyDescriptorPool(ss->device, ss->descriptor_pool, NULL);
    if (ss->vertex_buffer) vkDestroyBuffer(ss->device, ss->vertex_buffer, NULL);
    if (ss->vertex_buffer_memory) vkFreeMemory(ss->device, ss->vertex_buffer_memory, NULL);
    if (ss->index_buffer) vkDestroyBuffer(ss->device, ss->index_buffer, NULL);
    if (ss->index_buffer_memory) vkFreeMemory(ss->device, ss->index_buffer_memory, NULL);
  }

  // Destroy parameters
  SlangParam *param = ss->first_param;
  while (param) {
    SlangParam *next = param->next;
    free(param->id);
    free(param);
    param = next;
  }

  free(ss->base_path);
  free(ss);
}

// Calculate output dimensions for a pass based on scale type
static void CalculatePassDimensions(const SlangPassConfig *config,
                                    uint16_t source_width, uint16_t source_height,
                                    uint16_t viewport_width, uint16_t viewport_height,
                                    uint16_t *out_width, uint16_t *out_height) {
  // X dimension
  switch (config->scale_type_x) {
    case SLANG_SCALE_SOURCE:
      *out_width = (uint16_t)(source_width * config->scale_x);
      break;
    case SLANG_SCALE_VIEWPORT:
      *out_width = (uint16_t)(viewport_width * config->scale_x);
      break;
    case SLANG_SCALE_ABSOLUTE:
      *out_width = (uint16_t)config->scale_x;
      break;
    default:
      *out_width = viewport_width;
      break;
  }

  // Y dimension
  switch (config->scale_type_y) {
    case SLANG_SCALE_SOURCE:
      *out_height = (uint16_t)(source_height * config->scale_y);
      break;
    case SLANG_SCALE_VIEWPORT:
      *out_height = (uint16_t)(viewport_height * config->scale_y);
      break;
    case SLANG_SCALE_ABSOLUTE:
      *out_height = (uint16_t)config->scale_y;
      break;
    default:
      *out_height = viewport_height;
      break;
  }

  // Ensure minimum size
  if (*out_width < 1) *out_width = 1;
  if (*out_height < 1) *out_height = 1;
}

// Update descriptor set with UBO and input texture
static void UpdatePassDescriptorSet(SlangShader *ss, int pass_idx, VkImageView input_view, VkSampler sampler) {
  SlangPass *pass = &ss->pass[pass_idx];

  // UBO descriptor (binding 0)
  VkDescriptorBufferInfo buffer_info = {0};
  buffer_info.buffer = pass->ubo_buffer;
  buffer_info.offset = 0;
  buffer_info.range = sizeof(SlangUBO);

  // Sampler descriptor (binding 2 - standard slang layout)
  VkDescriptorImageInfo image_info = {0};
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image_info.imageView = input_view;
  image_info.sampler = sampler;

  VkWriteDescriptorSet writes[2] = {0};

  // Write 0: UBO at binding 0
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = pass->descriptor_set;
  writes[0].dstBinding = 0;
  writes[0].dstArrayElement = 0;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].descriptorCount = 1;
  writes[0].pBufferInfo = &buffer_info;

  // Write 1: Sampler at binding 2 (slang convention)
  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].dstSet = pass->descriptor_set;
  writes[1].dstBinding = 2;
  writes[1].dstArrayElement = 0;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[1].descriptorCount = 1;
  writes[1].pImageInfo = &image_info;

  vkUpdateDescriptorSets(ss->device, 2, writes, 0, NULL);
}

// Transition image layout (may be needed for explicit barriers in later phases)
static void TransitionImageLayout(VkCommandBuffer cmd, VkImage image,
                                  VkImageLayout old_layout, VkImageLayout new_layout) __attribute__((unused));
static void TransitionImageLayout(VkCommandBuffer cmd, VkImage image,
                                  VkImageLayout old_layout, VkImageLayout new_layout) {
  VkImageMemoryBarrier barrier = {0};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  VkPipelineStageFlags src_stage, dst_stage;

  if (old_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL &&
      new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
             new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  } else {
    // Generic fallback
    barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  }

  vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier);
}

// Build orthographic projection matrix for fullscreen quad
static void BuildMVP(float *mvp) {
  // Identity matrix for fullscreen quad (vertices are already in clip space)
  memset(mvp, 0, 16 * sizeof(float));
  mvp[0] = 1.0f;   // m[0][0]
  mvp[5] = 1.0f;   // m[1][1]
  mvp[10] = 1.0f;  // m[2][2]
  mvp[15] = 1.0f;  // m[3][3]
}

// Update UBO with current frame data
static void UpdatePassUBOData(SlangShader *ss, int pass_idx,
                              uint16_t source_w, uint16_t source_h,
                              uint16_t output_w, uint16_t output_h) {
  SlangPass *pass = &ss->pass[pass_idx];

  SlangUBO ubo;
  BuildMVP(ubo.mvp);

  // OutputSize: (width, height, 1/width, 1/height)
  ubo.output_size[0] = (float)output_w;
  ubo.output_size[1] = (float)output_h;
  ubo.output_size[2] = 1.0f / (float)output_w;
  ubo.output_size[3] = 1.0f / (float)output_h;

  // SourceSize: (width, height, 1/width, 1/height)
  ubo.source_size[0] = (float)source_w;
  ubo.source_size[1] = (float)source_h;
  ubo.source_size[2] = 1.0f / (float)source_w;
  ubo.source_size[3] = 1.0f / (float)source_h;

  ubo.frame_count = ss->frame_count;
  memset(ubo.padding, 0, sizeof(ubo.padding));

  // Map and copy UBO data
  void *data;
  if (vkMapMemory(ss->device, pass->ubo_memory, 0, sizeof(SlangUBO), 0, &data) == VK_SUCCESS) {
    memcpy(data, &ubo, sizeof(SlangUBO));
    vkUnmapMemory(ss->device, pass->ubo_memory);
    if (ss->frame_count < 2) {
      LogWarn("UBO pass %d: output=%.0fx%.0f source=%.0fx%.0f",
              pass_idx, ubo.output_size[0], ubo.output_size[1],
              ubo.source_size[0], ubo.source_size[1]);
    }
  }
}

// Find parameter value by name
static float FindParamValue(SlangShader *ss, const char *name) {
  for (SlangParam *p = ss->first_param; p; p = p->next) {
    if (strcmp(p->id, name) == 0) {
      return p->value;
    }
  }
  return 0.0f;  // Default if not found
}

// Build push constants with frame count and shader parameters in struct order
static void BuildPushConstants(SlangShader *ss, SlangPass *pass, SlangPushConstants *pc) {
  memset(pc, 0, sizeof(SlangPushConstants));

  // FrameCount at offset 0
  pc->frame_count = ss->frame_count;

  // Fill in shader parameters in push constant struct order (not #pragma order)
  for (int i = 0; i < pass->push_const_count && i < 31; i++) {
    const char *name = pass->push_const_names[i];
    pc->params[i] = FindParamValue(ss, name);
    if (ss->frame_count < 2) {
      LogWarn("PushConst[%d] %s = %.2f", i, name, pc->params[i]);
    }
  }
}

void SlangShader_Render(SlangShader *ss, void *cmd_handle,
                        const SlangInputImage *input,
                        const SlangOutputTarget *output) {
  if (!ss || !cmd_handle || !input || !output) return;

  VkCommandBuffer cmd = (VkCommandBuffer)cmd_handle;
  VkRenderPass output_rp = (VkRenderPass)output->render_pass;

  // Check if final pass pipeline needs recreation with the output render pass
  // This is needed because the final pass must be compatible with the swapchain framebuffer
  LogWarn("Pipeline check: valid=%d, cached_rp=%p, output_rp=%p",
          ss->final_pass_pipeline_valid, (void*)ss->output_render_pass, (void*)output_rp);
  if (!ss->final_pass_pipeline_valid || ss->output_render_pass != output_rp) {
    int final_idx = ss->n_pass - 1;
    SlangPass *final_pass = &ss->pass[final_idx];

    LogWarn("Recreating final pass pipeline for render pass %p", (void*)output_rp);

    // Destroy old pipeline if exists
    if (final_pass->pipeline) {
      vkDestroyPipeline(ss->device, final_pass->pipeline, NULL);
      final_pass->pipeline = VK_NULL_HANDLE;
    }

    // Recreate with output render pass
    if (!CreatePassPipelineWithRenderPass(ss, final_idx, output_rp)) {
      LogError("Failed to recreate final pass pipeline");
      return;
    }

    ss->output_render_pass = output_rp;
    ss->final_pass_pipeline_valid = true;
    LogInfo("Final pass pipeline recreated successfully");
  }

  // Track input for each pass
  VkImageView current_input_view = (VkImageView)input->image_view;
  VkSampler current_input_sampler = ss->pass[0].sampler;
  uint16_t source_width = input->width;
  uint16_t source_height = input->height;

  // Debug: log first few frames
  if (ss->frame_count < 3) {
    LogWarn("SlangShader_Render: frame=%u, n_pass=%d, input=%ux%u, output=%ux%u",
            ss->frame_count, ss->n_pass, input->width, input->height,
            output->width, output->height);
  }

  // Render each pass
  for (int i = 0; i < ss->n_pass; i++) {
    SlangPass *pass = &ss->pass[i];
    bool is_final = (i == ss->n_pass - 1);

    // Calculate output dimensions for this pass
    uint16_t pass_width, pass_height;
    if (is_final) {
      pass_width = output->width;
      pass_height = output->height;
    } else {
      CalculatePassDimensions(&pass->config, source_width, source_height,
                              output->width, output->height, &pass_width, &pass_height);
    }

    // Create/recreate intermediate framebuffer if dimensions changed (not for final pass)
    if (!is_final) {
      if (pass->width != pass_width || pass->height != pass_height) {
        if (!CreateIntermediateFramebuffer(ss, i, pass_width, pass_height)) {
          LogError("Failed to create framebuffer for pass %d", i);
          return;
        }
      }
    }

    // Update UBO with current frame data
    UpdatePassUBOData(ss, i, source_width, source_height, pass_width, pass_height);

    // Update descriptor set with current input
    UpdatePassDescriptorSet(ss, i, current_input_view, current_input_sampler);

    // Set viewport and scissor
    // For final pass, use the viewport from output (respects aspect ratio)
    // For intermediate passes, use full framebuffer
    VkViewport viewport = {0};
    VkRect2D scissor = {0};
    if (is_final && output->viewport_w > 0) {
      viewport.x = (float)output->viewport_x;
      viewport.y = (float)output->viewport_y;
      viewport.width = (float)output->viewport_w;
      viewport.height = (float)output->viewport_h;
      scissor.offset.x = output->viewport_x;
      scissor.offset.y = output->viewport_y;
      scissor.extent.width = output->viewport_w;
      scissor.extent.height = output->viewport_h;
    } else {
      viewport.x = 0.0f;
      viewport.y = 0.0f;
      viewport.width = (float)pass_width;
      viewport.height = (float)pass_height;
      scissor.offset.x = 0;
      scissor.offset.y = 0;
      scissor.extent.width = pass_width;
      scissor.extent.height = pass_height;
    }
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // Begin render pass
    // For final pass, use the provided render pass (compatible with swapchain framebuffer)
    // For intermediate passes, use our own render pass
    VkRenderPassBeginInfo render_pass_info = {0};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = is_final ? (VkRenderPass)output->render_pass : pass->render_pass;
    render_pass_info.framebuffer = is_final ? (VkFramebuffer)output->framebuffer : pass->framebuffer;
    render_pass_info.renderArea.offset.x = 0;
    render_pass_info.renderArea.offset.y = 0;
    render_pass_info.renderArea.extent.width = pass_width;
    render_pass_info.renderArea.extent.height = pass_height;

    // Clear to black
    VkClearValue clear_value = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_value;

    if (ss->frame_count < 3)
      LogWarn("BeginRenderPass: pass=%d, is_final=%d, rp=%p, fb=%p, extent=%ux%u",
              i, is_final, (void*)render_pass_info.renderPass, (void*)render_pass_info.framebuffer,
              render_pass_info.renderArea.extent.width, render_pass_info.renderArea.extent.height);

    vkCmdBeginRenderPass(cmd, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    // Bind pipeline
    if (ss->frame_count < 3)
      LogWarn("BindPipeline: pipeline=%p", (void*)pass->pipeline);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pass->pipeline);

    // Build and push constants for this pass
    SlangPushConstants push_constants;
    BuildPushConstants(ss, pass, &push_constants);
    vkCmdPushConstants(cmd, pass->pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, pass->push_constant_size, &push_constants);

    // Set dynamic state
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Bind descriptor set
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pass->pipeline_layout,
                            0, 1, &pass->descriptor_set, 0, NULL);

    // Bind vertex buffer
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &ss->vertex_buffer, &offset);
    vkCmdBindIndexBuffer(cmd, ss->index_buffer, 0, VK_INDEX_TYPE_UINT16);

    // Draw fullscreen quad
    vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

    vkCmdEndRenderPass(cmd);

    // If not final pass, transition output image and set it as input for next pass
    if (!is_final) {
      // The render pass already transitions the image to SHADER_READ_ONLY_OPTIMAL
      current_input_view = pass->output_view;
      current_input_sampler = pass->sampler;
      source_width = pass_width;
      source_height = pass_height;
    }
  }

  if (ss->frame_count < 3)
    LogWarn("SlangShader_Render completed: frame=%u", ss->frame_count);
  ss->frame_count++;
}

#else  // !SLANG_SHADERS_AVAILABLE

// Stub implementations when slang shaders are not available

bool IsSlangPreset(const char *filename) {
  (void)filename;
  return false;
}

SlangShader *SlangShader_CreateFromFile(const char *filename, const SlangVulkanContext *vk_ctx) {
  (void)filename;
  (void)vk_ctx;
  LogError("Slang shaders not available - rebuild with SLANG_SHADERS_AVAILABLE");
  return NULL;
}

void SlangShader_Destroy(SlangShader *ss) {
  (void)ss;
}

void SlangShader_Render(SlangShader *ss, void *cmd,
                        const SlangInputImage *input,
                        const SlangOutputTarget *output) {
  (void)ss;
  (void)cmd;
  (void)input;
  (void)output;
}

#endif  // SLANG_SHADERS_AVAILABLE
