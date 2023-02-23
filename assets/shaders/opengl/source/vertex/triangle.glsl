#version 460

const vec2 _20[3] = vec2[](vec2(0.0, 1.0), vec2(0.0, -1.0), vec2(2.0, 1.0));

layout(location = 0) out vec2 outTexCoord;

void main()
{
    outTexCoord = _20[gl_VertexID];
    gl_Position = vec4((outTexCoord * 2.0) - vec2(1.0), 1.0, 1.0);
    outTexCoord.y = 1.0 - outTexCoord.y;
}

