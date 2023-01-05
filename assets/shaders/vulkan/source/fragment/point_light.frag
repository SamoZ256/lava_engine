#version 460

layout (location = 0) out vec4 FragColor;

layout (set = 0, binding = 1) uniform VP {
    mat4 invViewProj;
} u_vp;

layout (set = 0, binding = 2) uniform sampler2D u_depth;
layout (set = 0, binding = 3) uniform sampler2D u_normalRoughness;
layout (set = 0, binding = 4) uniform sampler2D u_albedoMetallic;

void main() {
    FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
