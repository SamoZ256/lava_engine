#version 410

layout(std140) uniform VP
{
    mat4 invViewProj;
} u_vp;

uniform sampler2D u_depth;
uniform sampler2D u_normalRoughness;
uniform sampler2D u_albedoMetallic;

layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}

