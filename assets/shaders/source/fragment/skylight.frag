#version 450

layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec3 v_texCoord;

layout (set = 0, binding = 0) uniform samplerCube environmentMap;
//uniform samplerCube prefilterMap;

void main() {
	FragColor = texture(environmentMap, v_texCoord);
	//FragColor = vec4(1.0, 1.0, 1.0, 1.0);
	//FragColor = vec4(textureLod(prefilterMap, texCoord, 1.2).rgb, 1.0);
}
