#version 460

layout(binding = 0, std140) uniform VP
{
    mat4 viewProj;
} u_vp;

struct MODEL
{
    mat4 model;
};

uniform MODEL u_model;

layout(location = 0) in vec3 aPosition;

void main()
{
    gl_Position = (u_vp.viewProj * u_model.model) * vec4(aPosition, 1.0);
}

