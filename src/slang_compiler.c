// Slang compiler wrapper for glslang
// Compiles GLSL (Vulkan flavor) to SPIR-V

#include "slang_compiler.h"
#include "logging.h"
#include <stdlib.h>
#include <string.h>

#ifdef SLANG_SHADERS_AVAILABLE

#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>

static bool g_compiler_initialized = false;

bool SlangCompiler_Init(void) {
  if (g_compiler_initialized)
    return true;

  if (!glslang_initialize_process()) {
    LogError("glslang_initialize_process failed");
    return false;
  }

  g_compiler_initialized = true;
  LogInfo("Slang compiler initialized");
  return true;
}

void SlangCompiler_Shutdown(void) {
  if (g_compiler_initialized) {
    glslang_finalize_process();
    g_compiler_initialized = false;
  }
}

SlangCompileResult SlangCompiler_Compile(const char *source, SlangStage stage) {
  SlangCompileResult result = {0};

  if (!g_compiler_initialized) {
    result.error_message = strdup("Compiler not initialized");
    return result;
  }

  if (!source || !*source) {
    result.error_message = strdup("Empty source code");
    return result;
  }

  // Map stage to glslang stage
  glslang_stage_t glslang_stage;
  switch (stage) {
    case SLANG_STAGE_VERTEX:
      glslang_stage = GLSLANG_STAGE_VERTEX;
      break;
    case SLANG_STAGE_FRAGMENT:
      glslang_stage = GLSLANG_STAGE_FRAGMENT;
      break;
    default:
      result.error_message = strdup("Unsupported shader stage");
      return result;
  }

  // Configure input
  glslang_input_t input = {0};
  input.language = GLSLANG_SOURCE_GLSL;
  input.stage = glslang_stage;
  input.client = GLSLANG_CLIENT_VULKAN;
  input.client_version = GLSLANG_TARGET_VULKAN_1_0;
  input.target_language = GLSLANG_TARGET_SPV;
  input.target_language_version = GLSLANG_TARGET_SPV_1_0;
  input.code = source;
  input.default_version = 450;
  input.default_profile = GLSLANG_CORE_PROFILE;
  input.force_default_version_and_profile = false;
  input.forward_compatible = false;
  input.messages = GLSLANG_MSG_DEFAULT_BIT | GLSLANG_MSG_VULKAN_RULES_BIT | GLSLANG_MSG_SPV_RULES_BIT;
  input.resource = glslang_default_resource();

  // Create shader
  glslang_shader_t *shader = glslang_shader_create(&input);
  if (!shader) {
    result.error_message = strdup("glslang_shader_create failed");
    return result;
  }

  // Preprocess
  if (!glslang_shader_preprocess(shader, &input)) {
    const char *info_log = glslang_shader_get_info_log(shader);
    result.error_message = strdup(info_log ? info_log : "Preprocessing failed");
    glslang_shader_delete(shader);
    return result;
  }

  // Parse
  if (!glslang_shader_parse(shader, &input)) {
    const char *info_log = glslang_shader_get_info_log(shader);
    result.error_message = strdup(info_log ? info_log : "Parsing failed");
    glslang_shader_delete(shader);
    return result;
  }

  // Create program and link
  glslang_program_t *program = glslang_program_create();
  if (!program) {
    result.error_message = strdup("glslang_program_create failed");
    glslang_shader_delete(shader);
    return result;
  }

  glslang_program_add_shader(program, shader);

  if (!glslang_program_link(program, GLSLANG_MSG_DEFAULT_BIT | GLSLANG_MSG_VULKAN_RULES_BIT | GLSLANG_MSG_SPV_RULES_BIT)) {
    const char *info_log = glslang_program_get_info_log(program);
    result.error_message = strdup(info_log ? info_log : "Linking failed");
    glslang_program_delete(program);
    glslang_shader_delete(shader);
    return result;
  }

  // Generate SPIR-V
  glslang_spv_options_t spv_options = {0};
  spv_options.generate_debug_info = false;
  spv_options.strip_debug_info = true;
  spv_options.disable_optimizer = false;
  spv_options.optimize_size = true;
  spv_options.disassemble = false;
  spv_options.validate = true;

  glslang_program_SPIRV_generate_with_options(program, glslang_stage, &spv_options);

  // Check for SPIR-V generation messages
  const char *spirv_messages = glslang_program_SPIRV_get_messages(program);
  if (spirv_messages && *spirv_messages) {
    LogDebug("SPIR-V generation: %s", spirv_messages);
  }

  // Get SPIR-V binary
  size_t spirv_word_count = glslang_program_SPIRV_get_size(program);
  if (spirv_word_count == 0) {
    result.error_message = strdup("SPIR-V generation produced no output");
    glslang_program_delete(program);
    glslang_shader_delete(shader);
    return result;
  }

  result.spirv_size = spirv_word_count * sizeof(uint32_t);
  result.spirv_code = (uint32_t *)malloc(result.spirv_size);
  if (!result.spirv_code) {
    result.error_message = strdup("Failed to allocate SPIR-V buffer");
    result.spirv_size = 0;
    glslang_program_delete(program);
    glslang_shader_delete(shader);
    return result;
  }

  glslang_program_SPIRV_get(program, result.spirv_code);
  result.success = true;

  // Cleanup
  glslang_program_delete(program);
  glslang_shader_delete(shader);

  return result;
}

void SlangCompileResult_Destroy(SlangCompileResult *result) {
  if (!result) return;
  free(result->spirv_code);
  free(result->error_message);
  memset(result, 0, sizeof(*result));
}

#else  // !SLANG_SHADERS_AVAILABLE

// Stub implementations when glslang is not available

bool SlangCompiler_Init(void) {
  LogError("Slang compiler not available - rebuild with glslang support");
  return false;
}

void SlangCompiler_Shutdown(void) {
}

SlangCompileResult SlangCompiler_Compile(const char *source, SlangStage stage) {
  (void)source;
  (void)stage;
  SlangCompileResult result = {0};
  result.error_message = strdup("Slang compiler not available");
  return result;
}

void SlangCompileResult_Destroy(SlangCompileResult *result) {
  if (!result) return;
  free(result->spirv_code);
  free(result->error_message);
  memset(result, 0, sizeof(*result));
}

#endif  // SLANG_SHADERS_AVAILABLE
