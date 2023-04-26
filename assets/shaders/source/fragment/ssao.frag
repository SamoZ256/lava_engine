#version 450

vec3 reconstructPosFromDepth(in mat4 invViewProj, in vec2 texCoord, in float depth) {
    vec3 posInViewProj = vec3(texCoord.x * 2.0 - 1.0, (1.0 - texCoord.y) * 2.0 - 1.0, depth);
    vec4 position = invViewProj * vec4(posInViewProj, 1.0);
    position.xyz /= position.w;

    return position.xyz;
}

layout (location = 0) out float FragColor;

layout (location = 0) in vec2 v_texCoord;

layout (constant_id = 0) const int aoType = 0;

layout (set = 0, binding = 0) uniform sampler2D u_depth;
layout (set = 0, binding = 1) uniform sampler2D u_normalRoughness;
layout (set = 0, binding = 2) uniform sampler2D u_ssaoNoise;

/*
layout (binding = 3) uniform SSAO_KERNEL {
	vec4 samples[SSAO_KERNEL_SIZE];
} u_ssaoKernel;
*/

layout (push_constant) uniform VP  {
	mat4 projection;
    mat4 view;
    mat4 invViewProj;
} u_vp;

const float INFINITY = 1.0 / 0.0;

const int SSAO_KERNEL_SIZE = 24;
const float SSAO_RADIUS = 0.8;
const float HBAO_RADIUS = 4.0;
//const float RANGE_CHECK_RADIUS = 0.02;

const vec3 SSAO_KERNEL[SSAO_KERNEL_SIZE] = vec3[](
    vec3(-0.0622651, -0.0516783, 0.0374449),
    vec3(0.0306225, -0.0203063, 0.0168497),
    vec3(-0.0411804, 0.0422952, 0.00777669),
    vec3(0.0110055, 0.0414319, 0.0396107),
    vec3(0.0631798, 0.0830497, 0.0609105),
    vec3(-0.0369299, -0.0134597, 0.0818599),
    vec3(0.0872061, 0.100237, 0.0178864),
    vec3(0.045577, -0.0230743, 0.0524416),
    vec3(-0.109747, 0.00853307, 0.113283),
    vec3(0.0113271, 0.00265135, 0.0177387),
    vec3(0.0726579, -0.044955, 0.198941),
    vec3(-0.00153467, 0.0549661, 0.00721829),
    vec3(-0.0411426, 0.0417342, 0.00624951),
    vec3(0.0337328, 0.0228769, 0.106854),
    vec3(-0.0528015, -0.075991, 0.39236),
    vec3(0.0237407, -0.103542, 0.107603),
    vec3(-0.00895408, -0.0858161, 0.0130408),
    vec3(-0.0283149, -0.225021, 0.122593),
    vec3(0.0162343, 0.0357724, 0.00959596),
    vec3(0.1006, -0.0333268, 0.0288559),
    vec3(0.429389, 0.459596, 0.0531716),
    vec3(0.152051, 0.214739, 0.381863),
    vec3(-0.303363, -0.212891, 0.347137),
    vec3(0.212305, -0.122185, 0.0529759)
);

//Position reconstruction
/*
vec3 reconstructPosFromDepth(in float depth) {
    vec3 posInViewProj = vec3(v_texCoord.x * 2.0 - 1.0, (1.0 - v_texCoord.y) * 2.0 - 1.0, depth);
    vec4 position = inverse(u_vp.projection * u_vp.view) * vec4(posInViewProj, 1.0);
    position.xyz /= position.w;

    return position.xyz;
}
*/

float linearizeDepth(float depth, float zNear, float zFar) {
    return zNear * zFar / (zFar + depth * (zNear - zFar));
}

float ssao()  {
	// Get G-Buffer values
    //vec4 positionDepth = texture(u_positionDepth, v_texCoord);
    float depth = texture(u_depth, v_texCoord).r;
	vec3 fragPos = reconstructPosFromDepth(u_vp.invViewProj, v_texCoord, depth);
    //float linearDepth = (u_vp.view * vec4(fragPos, 1.0)).z;
	vec3 normal = normalize(texture(u_normalRoughness, v_texCoord).xyz * 2.0 - 1.0);

	// Get a random vector using a noise lookup
	ivec2 texDim = textureSize(u_depth, 0); 
	ivec2 noiseDim = textureSize(u_ssaoNoise, 0);

	const vec2 noiseUV = vec2(float(texDim.x) / float(noiseDim.x), float(texDim.y) / (noiseDim.y)) * v_texCoord;
	vec3 randomVec = texture(u_ssaoNoise, noiseUV).xyz;
	
	// Create TBN matrix
	vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 bitangent = cross(tangent, normal);
	mat3 TBN = mat3(tangent, bitangent, normal);

	// Calculate occlusion value
	float occlusion = 0.0;
	// remove banding
	const float bias = 0.04;

	for (int i = 0; i < SSAO_KERNEL_SIZE; i++) {		
		vec3 samplePos = TBN * SSAO_KERNEL[i].xyz; 
		samplePos = fragPos + samplePos * SSAO_RADIUS;
		
		// project
        vec4 viewSamplePos = u_vp.view * vec4(samplePos, 1.0);
		vec4 offset = u_vp.projection * viewSamplePos;
		offset.xy /= offset.w;
		offset.xy = offset.xy * 0.5 + 0.5;
		
		float sampleDepth = texture(u_depth, vec2(offset.x, 1.0 - offset.y)).r;
        float sampleLinearDepth = /*linearizeDepth(sampleDepth, 0.01, 1000.0);*/(u_vp.view * vec4(reconstructPosFromDepth(u_vp.invViewProj, vec2(offset.x, 1.0 - offset.y), sampleDepth), 1.0)).z;
        //FragColor = sampleDepth;

		float rangeCheck = smoothstep(0.0, 1.0, SSAO_RADIUS / abs(sampleLinearDepth - viewSamplePos.z));
		occlusion += (viewSamplePos.z <= sampleLinearDepth - bias ? rangeCheck : 0.0);
        //if (sampleDepth < texture(u_positionDepth, v_texCoord).w)
        //    occlusion += 1.0;
	}
    /*
    for (int x = -8; x <= 8; x++) {
        for (int y = -8; y <= 8; y++) {
            float sampleDepth = texture(u_positionDepth, v_texCoord + vec2(x, y) / texDim).w; 

            //float rangeCheck = smoothstep(0.0, 1.0, SSAO_RADIUS / abs(fragPos.z - sampleDepth));
            //occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
            if (sampleDepth < texture(u_positionDepth, v_texCoord).w)
                occlusion += 1.0 / 81.0 * 0.25;
        }
    }
    */
	return 1.0 - (occlusion / float(SSAO_KERNEL_SIZE));
}

/*
float saturate(float a) {
	return min(max(a, 0), 1);
}
*/

float length2(vec3 a) {
    return a.x * a.x + a.y * a.y + a.z * a.z;
}

float computeAO(vec3 normal, vec2 direction, vec2 texelSize, vec3 fragPos) {
	//float RAD = 1.0;
	//float RAD_FOR_DIRECTION = length(direction * vec2(10.0) / (vec2(abs(fragPos.z)) * screenSize));

	vec3 viewVector = normalize(fragPos);

	vec3 leftDirection = cross(viewVector, vec3(direction, 0));
	//vec3 projectedNormal = normal - dot(leftDirection, normal) * leftDirection;
	//projectedNormal = normalize(projectedNormal);

	vec3 tangent = cross(/*projectedNormal*/normal, leftDirection);

	const float bias = (3.141592 / 360.0) * 10.0;

	float tangentAngle = atan(tangent.z / length(tangent.xy));
	float sinTangentAngle = sin(tangentAngle + bias);

	//float highestZ = -INFINITY;
	//vec3 foundPos = vec3(0, 0, -INFINITY);
    float horizonAngle = tangentAngle + bias;
    vec3 horizonVector;// = vec3(0.0);
	
	for (int i = 2; i < 10; i++) {
		vec2 marchPosition = v_texCoord + i * texelSize * direction * 1.0;
		
		float depth = texture(u_depth, vec2(marchPosition.x, marchPosition.y)).x;
        //if (depth == 1.0) continue;
        vec3 fragPosMarch = (u_vp.view * vec4(reconstructPosFromDepth(u_vp.invViewProj, marchPosition, depth), 1.0)).xyz;
		
		vec3 crntVector = fragPosMarch - fragPos; //inspre origine

		//float rangeCheck = 1 - saturate(length(fragPosMarch - fragPos) / RAD-1);

		if (length2(crntVector) < HBAO_RADIUS * HBAO_RADIUS) {
			horizonAngle = max(horizonAngle, atan(crntVector.z / length(crntVector.xy)));
            horizonVector = crntVector;
		}
	}
    //horizonAngle = horizonAngle % 6.3;
    //horizonAngle += 2.0;

	float sinHorizonAngle = sin(horizonAngle);

	//vec2 rez = vec2(saturate((sinHorizonAngle - sinTangentAngle)) / 2, projectedLen);

    float norm = length(horizonVector) / HBAO_RADIUS;
    float attenuation = 1.0 - norm * norm;

    //if (horizonVector.x == 0.0) return 0.0;

	return attenuation * (sinHorizonAngle - sinTangentAngle);
}

float hbao() {
    vec2 screenSize = textureSize(u_depth, 0).xy;
	vec2 noiseScale = vec2(screenSize.x / 8.0, screenSize.y / 8.0);
	vec2 noisePos = v_texCoord * noiseScale;

    float depth = texture(u_depth, v_texCoord).x;

	vec3 fragPos = (u_vp.view * vec4(reconstructPosFromDepth(u_vp.invViewProj, v_texCoord, depth), 1.0)).xyz;

	vec3 normal = normalize((u_vp.view * vec4(texture(u_normalRoughness, v_texCoord).xyz, 1.0)).xyz);

	vec2 randomVec = texture(u_ssaoNoise, noisePos).xy;
	//vec2 randomVec = normalize(vec2(0, 1) + texture(u_ssaoNoise, noisePos).xy * 1.0);

    vec2 texelSize = 1.0 / screenSize;

	float rez = 0.0;

	rez += computeAO(normal, vec2(randomVec), texelSize, fragPos);
	rez += computeAO(normal, -vec2(randomVec), texelSize, fragPos);
	rez += computeAO(normal, vec2(-randomVec.y, -randomVec.x), texelSize, fragPos);
	rez += computeAO(normal, vec2(randomVec.y, randomVec.x), texelSize, fragPos);

    return 1.0 - rez * 0.25;
}

void main() {
    if (aoType == 0)
        FragColor = 1.0;
    else if (aoType == 1)
        FragColor = ssao();
    else if (aoType == 2)
        FragColor = hbao();
}
