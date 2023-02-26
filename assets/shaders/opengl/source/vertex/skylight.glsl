#version 410

layout(std140) uniform VP
{
    mat4 viewProj;
} u_vp;

layout(location = 0) out vec3 v_texCoord;
layout(location = 0) in vec3 aPosition;

void main()
{
    v_texCoord = aPosition;
    gl_Position = u_vp.viewProj * vec4(aPosition, 1.0);
}

