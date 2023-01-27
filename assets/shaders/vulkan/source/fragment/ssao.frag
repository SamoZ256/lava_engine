#version 450

#include "../Utility/utility.glsl"

layout (location = 0) out float FragColor;

layout (location = 0) in vec2 inTexCoord;

layout (binding = 0) uniform sampler2D u_depth;
layout (binding = 1) uniform sampler2D u_normalRoughness;
layout (binding = 2) uniform sampler2D u_ssaoNoise;

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

const int SSAO_KERNEL_SIZE = 24;
const float SSAO_RADIUS = 0.4;
const float RANGE_CHECK_RADIUS = 0.02;

const vec3 SSAO_KERNEL[64] = vec3[](
    vec3(-0.0622651, -0.0516783, 0.0374449),
    vec3(0.0302176, -0.0200379, 0.0166269),
    vec3(-0.0390986, 0.0401571, 0.00738356),
    vec3(0.00983945, 0.0370422, 0.035414),
    vec3(0.0523208, 0.0687755, 0.0504415),
    vec3(-0.0280151, -0.0102105, 0.0620991),
    vec3(0.0602267, 0.069226, 0.0123528),
    vec3(0.0285927, -0.0144757, 0.0328993),
    vec3(-0.0625902, 0.00486652, 0.0646069),
    vec3(0.00588934, 0.00137853, 0.00922296),
    vec3(0.0345845, -0.0213982, 0.0946942),
    vec3(-0.000672068, 0.0240709, 0.00316105),
    vec3(-0.0166647, 0.0169044, 0.00253135),
    vec3(0.0127063, 0.00861719, 0.0402493),
    vec3(-0.0185948, -0.0267613, 0.138175),
    vec3(0.00785665, -0.0342657, 0.0356098),
    vec3(-0.00279815, -0.0268175, 0.00407524),
    vec3(-0.00839346, -0.0667035, 0.0363405),
    vec3(0.00458419, 0.0101013, 0.00270968),
    vec3(0.0271658, -0.00899945, 0.00779216),
    vec3(0.11128, 0.119109, 0.0137799),
    vec3(0.0379422, 0.053585, 0.0952886),
    vec3(-0.0731075, -0.0513046, 0.0836565),
    vec3(0.0495465, -0.0285149, 0.0123632),
    vec3(-0.0281168, -0.10963, 0.0879173),
    vec3(-0.164272, -0.152799, 0.0194657),
    vec3(0.0138705, -0.156125, 0.0938625),
    vec3(-0.0235614, -0.0525998, 0.00553039),
    vec3(-0.157122, 0.0382268, 0.0307511),
    vec3(-0.154707, 0.0928639, 0.0572261),
    vec3(-0.173392, -0.123376, 0.0691542),
    vec3(-0.0563887, 0.17184, 0.149005),
    vec3(0.067844, -0.0637417, 0.145913),
    vec3(0.140394, -0.176461, 0.146532),
    vec3(0.175029, 0.159593, 0.151579),
    vec3(0.0270448, -0.184119, 0.028695),
    vec3(0.125719, -0.0983488, 0.144692),
    vec3(0.0752899, 0.000905001, 0.0988445),
    vec3(0.208785, 0.343314, 0.0226372),
    vec3(-0.0877636, -0.0162507, 0.0969601),
    vec3(0.243579, -0.258952, 0.068858),
    vec3(-0.00264632, 0.0201146, 0.0127595),
    vec3(0.0341593, -0.0260185, 0.0276118),
    vec3(0.258152, 0.0200024, 0.0513822),
    vec3(0.12844, -0.0651461, 0.0843168),
    vec3(0.156461, 0.360706, 0.0766686),
    vec3(-0.00812766, 0.0267197, 0.00100736),
    vec3(0.0216424, 0.00413512, 0.0279841),
    vec3(-0.478214, 0.345469, 0.0660385),
    vec3(-0.093043, -0.132376, 0.19712),
    vec3(0.285112, 0.0937665, 0.387254),
    vec3(-0.0123833, 0.0165029, 0.0132211),
    vec3(0.0215764, 0.419197, 0.466108),
    vec3(0.22667, -0.302599, 0.14933),
    vec3(-0.191782, -0.101208, 0.19595),
    vec3(-0.109642, -0.0156432, 0.0858681),
    vec3(0.0490018, -0.255871, 0.405244),
    vec3(0.074718, -0.0710762, 0.0508888),
    vec3(-0.216605, 0.222528, 0.59692),
    vec3(-0.193871, -0.337433, 0.121619),
    vec3(-0.517975, -0.634932, 0.336064),
    vec3(-0.291782, 0.288196, 0.660576),
    vec3(-0.00885274, -0.179628, 0.0218339),
    vec3(0.161253, -0.383984, 0.0959743)
);

//Position reconstruction
/*
vec3 reconstructPosFromDepth(in float depth) {
    vec3 posInViewProj = vec3(inTexCoord.x * 2.0 - 1.0, (1.0 - inTexCoord.y) * 2.0 - 1.0, depth);
    vec4 position = inverse(u_vp.projection * u_vp.view) * vec4(posInViewProj, 1.0);
    position.xyz /= position.w;

    return position.xyz;
}
*/

void main()  {
	// Get G-Buffer values
    //vec4 positionDepth = texture(u_positionDepth, inTexCoord);
    float depth = texture(u_depth, inTexCoord).r;
	vec3 fragPos = (u_vp.view * vec4(reconstructPosFromDepth(u_vp.invViewProj, inTexCoord, depth), 1.0)).xyz;
    float viewZ = fragPos.z;
	vec3 normal = normalize((u_vp.view * vec4(texture(u_normalRoughness, inTexCoord).xyz, 1.0)).xyz * 2.0 - 1.0);

	// Get a random vector using a noise lookup
	ivec2 texDim = textureSize(u_depth, 0); 
	ivec2 noiseDim = textureSize(u_ssaoNoise, 0);
	const vec2 noiseUV = vec2(float(texDim.x) / float(noiseDim.x), float(texDim.y) / (noiseDim.y)) * inTexCoord;
	vec3 randomVec = texture(u_ssaoNoise, noiseUV).xyz * 2.0 - 1.0;
	
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
		vec4 offset = u_vp.projection * vec4(samplePos, 1.0);
		offset.xy /= offset.w;
		offset.xy = offset.xy * 0.5 + 0.5;
		
		float sampleDepth = texture(u_depth, vec2(offset.x, 1.0 - offset.y)).r;
        float sampleViewZ = (u_vp.view * vec4(reconstructPosFromDepth(u_vp.invViewProj, vec2(offset.x, 1.0 - offset.y), sampleDepth), 1.0)).z;
        //FragColor = sampleDepth;

		float rangeCheck = smoothstep(0.0, 1.0, RANGE_CHECK_RADIUS / abs(depth - sampleDepth));
		occlusion += (viewZ <= sampleViewZ - bias ? rangeCheck : 0.0);
        //if (sampleDepth < texture(u_positionDepth, inTexCoord).w)
        //    occlusion += 1.0;
	}
    /*
    for (int x = -8; x <= 8; x++) {
        for (int y = -8; y <= 8; y++) {
            float sampleDepth = texture(u_positionDepth, inTexCoord + vec2(x, y) / texDim).w; 

            //float rangeCheck = smoothstep(0.0, 1.0, SSAO_RADIUS / abs(fragPos.z - sampleDepth));
            //occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
            if (sampleDepth < texture(u_positionDepth, inTexCoord).w)
                occlusion += 1.0 / 81.0 * 0.25;
        }
    }
    */
	FragColor = 1.0 - (occlusion / float(SSAO_KERNEL_SIZE));
}
