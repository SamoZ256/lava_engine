#version 450

//#extension SPV_EXT_shader_viewport_index_layer : enable

layout (location = 0) in vec3 aPosition;

layout (location = 0) out vec3 v_texCoord;

layout (push_constant) uniform VP {
	mat4 viewProj;
} u_vp;

void main() {
	v_texCoord = aPosition;
	gl_Position = u_vp.viewProj * vec4(aPosition, 1.0);
}
