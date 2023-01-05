#version 460

layout (location = 0) out vec2 outTexCoord;

vec2 texCoords[3] = {
    vec2(0.0,  1.0),
    vec2(0.0, -1.0),
    vec2(2.0,  1.0)
};

void main() {
    outTexCoord = texCoords[gl_VertexIndex];
    gl_Position = vec4(outTexCoord * 2.0 - 1.0, 1.0, 1.0);
    outTexCoord.y = 1.0 - outTexCoord.y;
}
