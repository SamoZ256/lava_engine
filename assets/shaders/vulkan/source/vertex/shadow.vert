#version 460

layout (location = 0) in vec3 aPosition;

layout (set = 0, binding = 0) uniform VP {
    mat4 viewProj;
    //int layerIndex;
} u_vp;

layout (push_constant) uniform MODEL {
    mat4 model;
} u_model;

void main() {
    gl_Position = u_vp.viewProj * u_model.model * vec4(aPosition, 1.0);
    //gl_Layer = u_vp.layerIndex;
}
