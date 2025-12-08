# Slang Shader Support

[Home](../index.md) > [Architecture](../architecture.md) > Slang Shaders

## Overview

Zelda3 supports RetroArch-compatible slang shaders via the Vulkan renderer. Slang shaders allow post-processing effects like CRT simulation, scanlines, and color correction.

**Requirements:** Vulkan renderer, `.slangp` preset file

## Supported Features

### Shader Presets (.slangp)

| Feature | Status | Notes |
|---------|--------|-------|
| Multi-pass rendering | Supported | Up to 20 passes |
| Shader parameters | Supported | Float parameters with min/max/step |
| Pass scaling | Supported | source, viewport, absolute |
| Filter modes | Supported | nearest, linear |
| Wrap modes | Supported | repeat, clamp_to_edge, clamp_to_border, mirrored_repeat |
| Frame count mod | Supported | `frame_count_mod` for animated effects |
| sRGB framebuffer | Supported | `srgb_framebuffer = true` |
| Float framebuffer | Supported | `float_framebuffer = true` (16-bit float) |
| LUT textures | Supported | PNG lookup tables |
| Mipmap filtering | Partial | Sampler configured, generation not implemented |

### Uniform Semantics

| Semantic | Type | Status | Description |
|----------|------|--------|-------------|
| MVP | mat4 | Supported | Model-view-projection matrix |
| SourceSize | vec4 | Supported | Input texture (w, h, 1/w, 1/h) |
| OriginalSize | vec4 | Supported | Original game framebuffer size |
| OutputSize | vec4 | Supported | Current pass output size |
| FinalViewportSize | vec4 | Supported | Final display viewport size |
| FrameCount | uint | Supported | Frame counter |
| FrameDirection | int | Supported | 1 = forward, -1 = rewind |
| Rotation | uint | Supported | Screen rotation (0, 90, 180, 270) |
| OriginalAspect | float | Supported | Original content aspect ratio |
| OriginalAspectRotated | float | Supported | Aspect ratio accounting for rotation |
| PassOutputNSize | vec4 | Supported | Previous pass N output size |
| TextureNameSize | vec4 | Supported | LUT texture dimensions |

### Texture Bindings

| Binding | Semantic | Status | Description |
|---------|----------|--------|-------------|
| 0 | UBO | Supported | Uniform buffer |
| 2 | Source | Supported | Input to current pass |
| 3 | Original | Partial | Falls back to first pass input |
| 4-11 | PassOutput0-7 | Supported | Previous pass outputs |
| 12+ | User/LUT | Supported | LUT textures |

## Not Supported

### Features Not Implemented

| Feature | Reason |
|---------|--------|
| **OriginalHistory textures** | Requires frame history buffer management. Would need ring buffer of previous frames. |
| **PassFeedback textures** | Requires feedback loop support. Complex synchronization needed. |
| **Mipmap generation** | Sampler supports mipmaps, but vkCmdBlitImage chain not implemented. Only affects LUT textures with `mipmap = true`. |
| **True Original texture** | Currently falls back to input. Full support would require texture copy at frame start. |
| **#reference directive** | Preset includes via `#reference` not parsed. |
| **Shader aliases** | Pass aliases for texture references not fully supported. |

### Semantic Gaps

| Semantic | Notes |
|----------|-------|
| TotalSubFrames | Subframe timing not applicable |
| CurrentSubFrame | Subframe timing not applicable |
| OriginalHistoryN | History frames not stored |
| PassFeedbackN | Feedback textures not implemented |

## Configuration

### zelda3.ini

```ini
[Graphics]
OutputMethod = Vulkan
Shader = shaders/shaders_slang/crt/crt-easymode.slangp
```

### Preset Format

Standard RetroArch `.slangp` format:

```
shaders = 1
shader0 = shaders/shaders_slang/crt/shaders/crt-easymode.slang
filter_linear0 = true
scale_type0 = viewport

parameters = "SHARPNESS_H;SHARPNESS_V;MASK_STRENGTH"
SHARPNESS_H = 0.5
SHARPNESS_V = 1.0
MASK_STRENGTH = 0.3

textures = "mask"
mask = shaders/shaders_slang/crt/textures/mask.png
mask_filter_linear = true
```

## Tested Shaders

| Shader | Status | Notes |
|--------|--------|-------|
| crt-aperture | Works | Single pass |
| crt-caligari | Works | Single pass, push constants |
| crt-consumer | Works | Non-standard UBO layout |
| crt-easymode | Works | Single pass, UBO parameters |
| crt-lottes | Works | Single pass |
| crt-geom | Untested | Multi-pass, may need LUT |
| crt-royale | Untested | Complex, many passes |

## Implementation Details

### Files

- `src/slang_shader.c` - Main shader implementation
- `src/slang_shader.h` - Public API
- `src/slang_compiler.c` - GLSL to SPIR-V compilation (uses glslang)

### Descriptor Set Layout

Each pass creates a descriptor set with:
- Binding 0: UBO (uniform buffer)
- Binding 2: Source texture
- Binding 3: Original texture
- Bindings 4-11: PassOutput0-7 (if pass index > 0)
- Bindings 12+: LUT textures

### UBO Layout

Dynamic UBO parsing supports both standard and non-standard layouts:

**Standard Layout (std140):**
```c
layout(std140, binding = 0) uniform UBO {
    mat4 MVP;           // offset 0
    vec4 OutputSize;    // offset 64
    vec4 OriginalSize;  // offset 80
    vec4 SourceSize;    // offset 96
    uint FrameCount;    // offset 112
};
```

**Dynamic Layout:**
Parsed from GLSL reflection, supports arbitrary member order and shader-specific parameters.

### Push Constants

Shader parameters can use push constants instead of UBO:
```c
layout(push_constant) uniform Push {
    float param1;
    float param2;
    vec4 SourceSize;
} params;
```

## Limitations

1. **Vulkan only** - Slang shaders require Vulkan renderer
2. **No runtime shader editing** - Shaders compiled at load time
3. **Android path handling** - Shaders must be in app-accessible storage
4. **Performance** - Complex multi-pass shaders may impact mobile performance

## See Also

- [Renderers](renderers.md) - Renderer overview
- [Graphics Pipeline](graphics-pipeline.md) - PPU rendering

**External References:**
- [RetroArch Slang Spec](https://github.com/libretro/slang-shaders)
- [slang-shaders repository](https://github.com/libretro/slang-shaders)
