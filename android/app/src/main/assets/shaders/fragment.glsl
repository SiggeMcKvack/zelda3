#version 310 es
precision mediump float;

// Fragment shader for texture sampling
// Samples the game framebuffer texture and outputs to screen

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D texSampler;

void main() {
    outColor = texture(texSampler, fragTexCoord);
}
