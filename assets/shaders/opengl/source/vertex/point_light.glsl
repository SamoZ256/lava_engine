#version 410

layout(std140) uniform VP
{
    mat4 viewProj;
} u_vp;

layout(std140) uniform MODEL
{
    vec3 position;
} u_model;

layout(location = 0) in vec3 aPosition;

void main()
{
    gl_Position = u_vp.viewProj * vec4(u_model.position + aPosition, 1.0);
}

