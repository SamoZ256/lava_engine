#version 460

struct VP
{
    mat4 viewProj;
};

uniform VP u_vp;

layout(location = 0) out vec3 outTexCoord;
layout(location = 0) in vec3 aPosition;

void main()
{
    outTexCoord = aPosition;
    gl_Position = u_vp.viewProj * vec4(aPosition, 1.0);
}

