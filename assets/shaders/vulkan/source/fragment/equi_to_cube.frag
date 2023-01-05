#version 460

layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec2 inTexCoord;

layout (set = 0, binding = 0) uniform VP {
	mat4 viewProj;
    int layerIndex;
} u_vp;

layout (set = 0, binding = 1) uniform sampler2D hdrMap;

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 sampleSphericalMap(vec3 v) {
	vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
	uv *= invAtan;
	uv += 0.5;

	return uv;
}

void main() {
	vec4 position = u_vp.viewProj * vec4(inTexCoord * 2.0 - 1.0, 1.0, 1.0);
	if (abs(position.y) == 1.0)  {
		position.x = -position.x;
		position.z = -position.z;
		position.z -= 0.2;
	}
	vec2 uv = sampleSphericalMap(normalize(position.xyz)); // make sure to normalize localPos
	vec3 color = texture(hdrMap, uv).rgb;

	//color = position.xyz;
	FragColor = vec4(color, 1.0);
	//FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
