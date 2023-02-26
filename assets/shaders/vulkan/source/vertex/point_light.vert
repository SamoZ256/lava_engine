#version 450

layout (location = 0) in vec3 aPosition;

layout (push_constant) uniform MODEL {
    vec3 position;
} u_model;

layout (set = 0, binding = 0) uniform VP {
    mat4 viewProj;
} u_vp;

void main() {
    gl_Position = u_vp.viewProj * vec4(u_model.position + aPosition, 1.0);
}
