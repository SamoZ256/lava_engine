#version 410

const vec2 _20[3] = vec2[](vec2(0.0, 1.0), vec2(0.0, -1.0), vec2(2.0, 1.0));

layout(location = 0) out vec2 v_texCoord;

void main()
{
    v_texCoord = _20[gl_VertexID];
    gl_Position = vec4((v_texCoord * 2.0) - vec2(1.0), 1.0, 1.0);
    v_texCoord.y = 1.0 - v_texCoord.y;
}

