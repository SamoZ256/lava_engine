#version 410

layout(std140) uniform VP
{
    mat4 viewProj;
} u_vp;

layout(std140) uniform MODEL
{
    mat4 model;
} u_model;

layout(location = 0) in vec3 aPosition;

void main()
{
    gl_Position = (u_vp.viewProj * u_model.model) * vec4(aPosition, 1.0);
}

