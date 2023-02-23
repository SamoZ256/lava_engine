#version 460

layout(binding = 0, std140) uniform VP
{
    mat4 viewProj;
} u_vp;

struct MODEL
{
    vec3 position;
};

uniform MODEL u_model;

layout(location = 0) in vec3 aPosition;

void main()
{
    gl_Position = u_vp.viewProj * vec4(u_model.position + aPosition, 1.0);
}

