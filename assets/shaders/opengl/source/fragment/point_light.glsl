#version 460

layout(binding = 1, std140) uniform VP
{
    mat4 invViewProj;
} u_vp;

layout(binding = 2) uniform sampler2D u_depth;
layout(binding = 3) uniform sampler2D u_normalRoughness;
layout(binding = 4) uniform sampler2D u_albedoMetallic;

layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}

