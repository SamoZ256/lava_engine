#version 460

#extension GL_ARB_shader_viewport_layer_array : enable

layout (location = 0) out vec2 outTexCoord;

vec2 texCoords[3] = {
    vec2(0.0,  1.0),
    vec2(0.0, -1.0),
    vec2(2.0,  1.0)
};

layout (set = 0, binding = 0) uniform VP {
	mat4 viewProj;
    int layerIndex;
} u_vp;

void main() {
    outTexCoord = texCoords[gl_VertexIndex];
    gl_Position = vec4(outTexCoord * 2.0 - 1.0, 0.0, 1.0);
    //outTexCoord.y = 1.0 - outTexCoord.y;

    gl_Layer = u_vp.layerIndex;
}
