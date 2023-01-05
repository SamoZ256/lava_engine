#version 460

layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec2 inTexCoord;

layout (set = 0, binding = 0) uniform VP {
	mat4 viewProj;
    int layerIndex;
} u_vp;

layout (set = 0, binding = 1) uniform samplerCube environmentMap;

const float PI = 3.14159265359;

void main() {
	// the sample direction equals the hemisphere's orientation
	vec4 position = u_vp.viewProj * vec4(inTexCoord * 2.0 - 1.0, 1.0, 1.0);
	if (abs(position.y) == 1.0)  {
		position.x = -position.x;
		position.z = -position.z;
		position.z -= 0.2;
	}
	position.y = -position.y;
	vec3 normal = normalize(position.xyz);

	vec3 irradiance = vec3(0.0);

	vec3 up = vec3(0.0, 1.0, 0.0);
	vec3 right = normalize(cross(up, normal));
	up = normalize(cross(normal, right));

	float sampleDelta = 0.025;
	float nbSamples = 0.0;
	for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
		for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta) {
			// spherical to cartesian (in tangent space)
			vec3 tangentSample = vec3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
			// tangent space to world
			vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

			irradiance += texture(environmentMap, sampleVec).rgb * cos(theta) * sin(theta);
			nbSamples++;
		}
	}
	irradiance = PI * irradiance / nbSamples;
	//irradiance = texture(environmentMap, normal).rgb;
	//irradiance = vec3(1.0, 0.0, 0.0);
	//irradiance = position.xyz;

	FragColor = vec4(irradiance, 1.0);
}
