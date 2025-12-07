#ifndef ZELDA3_SLANG_COMPILER_H_
#define ZELDA3_SLANG_COMPILER_H_

// Slang compiler wrapper for glslang
// Compiles GLSL (Vulkan flavor) to SPIR-V

#include "types.h"
#include <stddef.h>

// Compilation result - caller owns the memory
typedef struct SlangCompileResult {
  uint32_t *spirv_code;      // SPIR-V bytecode (caller must free)
  size_t spirv_size;         // Size in bytes
  char *error_message;       // Error message if compilation failed (caller must free)
  bool success;
} SlangCompileResult;

// Shader stage type
typedef enum SlangStage {
  SLANG_STAGE_VERTEX,
  SLANG_STAGE_FRAGMENT
} SlangStage;

// Initialize the compiler (call once at startup)
bool SlangCompiler_Init(void);

// Shutdown the compiler (call once at exit)
void SlangCompiler_Shutdown(void);

// Compile GLSL source to SPIR-V
// source: GLSL source code (Vulkan flavor, #version 450 recommended)
// stage: Shader stage (vertex or fragment)
// Returns compilation result - caller must call SlangCompileResult_Destroy
SlangCompileResult SlangCompiler_Compile(const char *source, SlangStage stage);

// Free resources in a compile result
void SlangCompileResult_Destroy(SlangCompileResult *result);

#endif  // ZELDA3_SLANG_COMPILER_H_
