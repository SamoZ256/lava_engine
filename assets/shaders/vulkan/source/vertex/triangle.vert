#version 450

layout (location = 0) out vec2 v_texCoord;

vec2 texCoords[3] = {
    vec2(0.0,  1.0),
    vec2(0.0, -1.0),
    vec2(2.0,  1.0)
};

void main() {
    v_texCoord = texCoords[gl_VertexIndex];
    gl_Position = vec4(v_texCoord * 2.0 - 1.0, 1.0, 1.0);
    v_texCoord.y = 1.0 - v_texCoord.y;
}
