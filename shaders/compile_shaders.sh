#!/bin/bash
# Shader compilation script for Vulkan SPIR-V shaders
# Compiles GLSL shaders to SPIR-V bytecode for Android Vulkan renderer

set -e

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SHADER_DIR="$SCRIPT_DIR"

# Try to find glslc (preferred for Android) or glslangValidator
SHADER_COMPILER=""

# Check for glslc in Android NDK (most common on macOS)
if [ -d "$HOME/Library/Android/sdk/ndk" ]; then
    SHADER_COMPILER=$(find "$HOME/Library/Android/sdk/ndk" -name "glslc" -type f 2>/dev/null | head -1)
fi

# Fallback to glslangValidator
if [ -z "$SHADER_COMPILER" ] || [ ! -f "$SHADER_COMPILER" ]; then
    if command -v glslangValidator &> /dev/null; then
        SHADER_COMPILER="glslangValidator"
        USE_GLSLANG=1
    elif command -v glslc &> /dev/null; then
        SHADER_COMPILER="glslc"
    else
        echo "ERROR: No shader compiler found!"
        echo "Please install Vulkan SDK or ensure Android NDK is properly configured"
        echo "Download from: https://vulkan.lunarg.com/sdk/home"
        exit 1
    fi
fi

echo "Using shader compiler: $SHADER_COMPILER"
echo "Compiling Vulkan shaders..."

# Compile vertex shader
echo "  - vertex.glsl -> vert.spv"
if [ -n "$USE_GLSLANG" ]; then
    $SHADER_COMPILER -V "$SHADER_DIR/vertex.glsl" -o "$SHADER_DIR/vert.spv" \
        --target-env vulkan1.0 \
        --source-entrypoint main \
        -g
else
    # glslc syntax
    $SHADER_COMPILER -fshader-stage=vertex \
        --target-env=vulkan1.0 \
        -g \
        "$SHADER_DIR/vertex.glsl" \
        -o "$SHADER_DIR/vert.spv"
fi

# Compile fragment shader
echo "  - fragment.glsl -> frag.spv"
if [ -n "$USE_GLSLANG" ]; then
    $SHADER_COMPILER -V "$SHADER_DIR/fragment.glsl" -o "$SHADER_DIR/frag.spv" \
        --target-env vulkan1.0 \
        --source-entrypoint main \
        -g
else
    # glslc syntax
    $SHADER_COMPILER -fshader-stage=fragment \
        --target-env=vulkan1.0 \
        -g \
        "$SHADER_DIR/fragment.glsl" \
        -o "$SHADER_DIR/frag.spv"
fi

# Validate compiled shaders
echo "Validating compiled shaders..."
if command -v spirv-val &> /dev/null; then
    spirv-val "$SHADER_DIR/vert.spv"
    spirv-val "$SHADER_DIR/frag.spv"
    echo "✓ All shaders compiled and validated successfully"
else
    echo "✓ Shaders compiled (spirv-val not available for validation)"
fi

# Display shader info
echo ""
echo "Shader statistics:"
ls -lh "$SHADER_DIR"/*.spv | awk '{print "  " $9 ": " $5}'
