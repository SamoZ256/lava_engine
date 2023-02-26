#version 410

const vec2 _20[3] = vec2[](vec2(0.0, 1.0), vec2(0.0, -1.0), vec2(2.0, 1.0));

layout(std140) uniform VP
{
    mat4 viewProj;
    int layerIndex;
} u_vp;

layout(location = 0) out vec2 outTexCoord;

void main()
{
    outTexCoord = _20[gl_VertexID];
    gl_Position = vec4((outTexCoord * 2.0) - vec2(1.0), 0.0, 1.0);
    gl_Layer = u_vp.layerIndex;
}

