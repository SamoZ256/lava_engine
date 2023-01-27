#version 460

#include "../Utility/utility.glsl"

layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec2 inTexCoord;

//Specialization constants
#define CASCADE_COUNT 3
//#define SHADOW_FAR_PLANE 100.0
//const float CASCADE_LEVELS[CASCADE_COUNT] = float[] (SHADOW_FAR_PLANE * 0.08, SHADOW_FAR_PLANE * 0.22, SHADOW_FAR_PLANE * 0.5, SHADOW_FAR_PLANE);

layout (set = 0, binding = 0) uniform LIGHT {
    vec4 color;
    vec3 direction;
} u_light;

layout (set = 0, binding = 1) uniform VP {
    mat4 invViewProj;
    vec3 cameraPos;
} u_vp;

layout (set = 0, binding = 2) uniform SHADOW {
    mat4 VPs[CASCADE_COUNT];
} u_shadow;

layout (set = 0, binding = 3) uniform sampler2DArrayShadow u_shadowMap;

layout (set = 1, binding = 0) uniform samplerCube u_irradianceMap;
layout (set = 1, binding = 1) uniform samplerCube u_prefilteredMap;
layout (set = 1, binding = 2) uniform sampler2D u_brdfLutMap;

layout (set = 2, binding = 0) uniform sampler2D u_depth;
layout (set = 2, binding = 1) uniform sampler2D u_normalRoughness;
layout (set = 2, binding = 2) uniform sampler2D u_albedoMetallic;

layout (set = 2, binding = 3) uniform sampler2D u_ssaoTex;

//Constants
const float PI = 3.14159265359;
/*
#define SHADOW_SAMPLE_COUNT 16
#define SHADOW_PENUMBRA_SIZE 2.0
*/
//const int PENUMBRA_SAMPLE_COUNT = 4;

/*
vec2 poissonDisk[16] = vec2[](
   vec2( -0.94201624, -0.39906216 ),
   vec2( 0.94558609, -0.76890725 ),
   vec2( -0.094184101, -0.92938870 ),
   vec2( 0.34495938, 0.29387760 ),
   vec2( -0.91588581, 0.45771432 ),
   vec2( -0.81544232, -0.87912464 ),
   vec2( -0.38277543, 0.27676845 ),
   vec2( 0.97484398, 0.75648379 ),
   vec2( 0.44323325, -0.97511554 ),
   vec2( 0.53742981, -0.47373420 ),
   vec2( -0.26496911, -0.41893023 ),
   vec2( 0.79197514, 0.19090188 ),
   vec2( -0.24188840, 0.99706507 ),
   vec2( -0.81409955, 0.91437590 ),
   vec2( 0.19984126, 0.78641367 ),
   vec2( 0.14383161, -0.14100790 )
);
*/

//Random number generator
float random(vec3 seed, int i) {
	float dotProduct = dot(vec4(seed, i), vec4(12.9898, 78.233, 45.164, 94.673));
	return fract(sin(dotProduct) * 43758.5453);
}

//PBR functions
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = r*r / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

//Direct light
vec3 calcDirectLight(vec3 norm, vec3 viewDir, vec3 F0, vec3 albedo, float roughness, float metallic) {
    vec3 L = normalize(-u_light.direction);
    vec3 H = normalize(viewDir + L);
    float cosTheta = max(dot(norm, L), 0.0);
    vec3 radiance = vec3(1.0) * cosTheta;

    //NDF and G
    float NDF = DistributionGGX(norm, H, roughness);
    float G = GeometrySmith(norm, viewDir, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, viewDir), 0.0), F0, roughness);

    //Specular
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(norm, viewDir), 0.0) * max(dot(norm, L), 0.0) + 0.0001;
    vec3 spec = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;

    kD *= roughness;

    float NdotL = max(dot(norm, L), 0.0);

    return (kD * albedo / PI + spec) * radiance * NdotL * u_light.color.rgb * u_light.color.a;
}

/*
float getShadowDist(float crntShadowDist) {
  return min(crntShadowDist + 0.1, 12.0);
}
*/

float getVisibility(vec3 position, vec3 normal) {
    //Layer
    int layer = -1;
    vec4 shadowCoord;
    for (int i = 0; i < CASCADE_COUNT; i++) {
        shadowCoord = u_shadow.VPs[i] * vec4(position, 1.0);
        shadowCoord.xyz /= shadowCoord.w;
        shadowCoord.xy = shadowCoord.xy * 0.5 + 0.5;
        shadowCoord.y = 1.0 - shadowCoord.y;
        if (shadowCoord.x >= 0.01 && shadowCoord.x <= 0.99 && shadowCoord.y >= 0.01 && shadowCoord.y <= 0.99) {
            layer = i;
            break;
        }
        /*
        if (abs(distToCamera) < CASCADE_LEVELS[i]) {
            layer = i;
            break;
        }
        */
    }
    if (layer == -1) {
        return 1.0;
    }

    /*
    shadowCoord = u_shadow.VPs[layer] * vec4(position, 1.0);
    shadowCoord.xyz /= shadowCoord.w;
    shadowCoord.xy = shadowCoord.xy * 0.5 + 0.5;
    shadowCoord.y = 1.0 - shadowCoord.y;
    */

    float layerRatio = 1.0;
    if (layer != 0)
        layerRatio = 1.0 / (layer * 2.0);//((CASCADE_LEVELS[layer] - CASCADE_LEVELS[layer - 1]) / CASCADE_LEVELS[0]) * layer;

    //Shadows
    float bias = 0.05 * tan(acos(dot(normal, u_light.direction)));
    bias = clamp(bias, 0.0/*005*/, 0.001);
    //float bias = 0.00001;

    //float occlusion = 0.0;
    //float lightDepth = shadowCoord.z;

    //float shadowDist = 1.0;//getShadowDist((shadowCoord.z - bias) - texture(u_shadowMap, shadowCoord.xy).x);
    //vec2 texelSize = 1.0 / vec2(textureSize(u_shadowMap, 0));
    //texelSize /= texelSize.x > texelSize.y ? texelSize.x : texelSize.y;

    //Estimating penumbra size
    /*
    for (int i = 0; i < PENUMBRA_SAMPLE_COUNT; i++) {
        int index = int(PENUMBRA_SAMPLE_COUNT * random(position, i)) % 16;
        vec2 coord = vec2(shadowCoord.xy + normalize(poissonDisk[index]) * random(position, i) / 700.0 * 50.0 * texelSize);
        float currentDist = (shadowCoord.z - bias) - texture(u_shadowMap, coord).x;
        if (currentDist > 0.0) {
        shadowDist = getShadowDist(max(shadowDist, currentDist));
        }
    }
    */

    //Calculating visibility
    /*
    for (int i = 0; i < SHADOW_SAMPLE_COUNT; i++) {
        int index = i % 16;//int(SHADOW_SAMPLE_COUNT * random(position, i)) % 16;
        vec2 coord = vec2(shadowCoord.xy + normalize(poissonDisk[index]) * random(position, i) * layerRatio / 700.0 * SHADOW_PENUMBRA_SIZE * texelSize);
        float currentDist = (shadowCoord.z - bias) - texture(u_shadowMap, vec3(coord, layer)).x;
        if (currentDist > 0.0) {
            occlusion += 1.0;// / clamp(currentDist, 1.0, 2.0)
            //shadowDist = getShadowDist(currentDist);
        }
    }
    */

    //return 1.0 - occlusion / SHADOW_SAMPLE_COUNT;
    return texture(u_shadowMap, vec4(shadowCoord.xy, layer, shadowCoord.z - bias));
}

//Position reconstruction
/*
vec3 reconstructPosFromDepth(in float depth) {
    vec3 posInViewProj = vec3(inTexCoord.x * 2.0 - 1.0, (1.0 - inTexCoord.y) * 2.0 - 1.0, depth);
    vec4 position = u_vp.invViewProj * vec4(posInViewProj, 1.0);
    position.xyz /= position.w;

    return position.xyz;
}
*/

void main() {
    //Unpacking G-Buffers
	//vec4 positionDepth = texture(u_positionDepth, inTexCoord);
	//vec3 position = positionDepth.xyz;
	float depth = texture(u_depth, inTexCoord).r;
    //if (depth == 1.0) discard;
    vec3 position = reconstructPosFromDepth(u_vp.invViewProj, inTexCoord, depth);

	vec4 normalRoughness = texture(u_normalRoughness, inTexCoord);
	vec3 normal = normalRoughness.xyz;
	float roughness = normalRoughness.a;

	vec4 albedoMetallic = texture(u_albedoMetallic, inTexCoord);
	vec3 albedo = albedoMetallic.rgb;
	float metallic = albedoMetallic.a;

    //Properties
    vec3 posToCamera = u_vp.cameraPos - position;
    float distToCamera = length(posToCamera);
    vec3 viewDir = posToCamera / distToCamera;

    //vec3 albedo = u_material.albedo.rgb * texture(u_albedoTexture, inTexCoord).rgb;

    //F0
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 result = calcDirectLight(normal, viewDir, F0, albedo, roughness, metallic)/* * getVisibility(position)*//* * getVisibility(normal, position, depth)*/ * getVisibility(position, normal);

    //IBL

    //Diffuse
    vec3 kS = fresnelSchlick(max(dot(normal, viewDir), 0.0), F0, roughness);
    vec3 kD = 1.0 - kS;
    vec3 irradiance = texture(u_irradianceMap, normal).rgb;
    vec3 diffuse = irradiance * albedo;

    //Specular
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 R = reflect(viewDir, normal);
    R.y = -R.y;
    vec3 prefilteredColor = textureLod(u_prefilteredMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf  = texture(u_brdfLutMap, vec2(max(dot(normal, viewDir), 0.0), roughness)).rg;
    vec3 specular = prefilteredColor * (kS * brdf.x + brdf.y);

    vec3 ambient = (kD * diffuse + specular) * texture(u_ssaoTex, inTexCoord).r;

    result += ambient;

    //Shadows
    /*
    vec4 lightSpaceCoord = u_shadow.viewProj * vec4(position, 1.0);
    vec3 shadowCoord = lightSpaceCoord.xyz / lightSpaceCoord.w;
    shadowCoord.xy = shadowCoord.xy * 0.5 + 0.5;
    shadowCoord.y = 1.0 - shadowCoord.y;

    float visibility = 1.0;
    float bias = 0.0001;
    if (shadowCoord.x >= 0.0 && shadowCoord.x <= 1.0 && shadowCoord.y >= 0.0 && shadowCoord.y <= 1.0) {
        if (shadowCoord.z - bias > texture(u_shadowMap, shadowCoord.xy).x)
            visibility = 0.5;
    }
    */

    //Output
    FragColor = vec4(result, 1.0);
    //float shadowVal = texture(u_shadowMap, inTexCoord).x;
    //if (shadowVal == 1.0) discard;
    //FragColor = vec4(vec3(shadowVal), 1.0);
    //FragColor = vec4(vec3(visibility), 1.0);
    /*
    vec4 lightSpaceCoord = u_shadow.VPs[0] * vec4(position, 1.0);
    vec3 shadowCoord = lightSpaceCoord.xyz / lightSpaceCoord.w;
    shadowCoord.xy = shadowCoord.xy * 0.5 + 0.5;
    shadowCoord.y = 1.0 - shadowCoord.y;

    FragColor.rgb = vec3(texture(u_shadowMap, vec3(shadowCoord.xy, 0)).x);
    */
    
    /*
    int layer = -1;
    for (int i = 0; i < CASCADE_COUNT; i++) {
        if (abs(length(posToCamera)) < CASCADE_LEVELS[i]) {
            layer = i;
            break;
        }
    }
    vec3 layerColor = vec3(0.1, 0.1, 0.1);
    if (layer == 0)
        layerColor = vec3(1.0, 0.7, 0.7);
    else if (layer == 1)
        layerColor = vec3(0.7, 1.0, 0.7);
    else if (layer == 2)
        layerColor = vec3(0.7, 0.7, 1.0);
    FragColor.rgb *= layerColor;
    */
    //FragColor = vec4(vec3(depth / 100.0), 1.0);
    //FragColor = mix(FragColor, vec4(vec3(texture(u_shadowMap, vec3(inTexCoord, 0)).r), 1.0), 0.95);
    //FragColor = vec4(vec3(texture(u_ssaoTex, inTexCoord).r), 1.0);
    //FragColor.rgb = position;
    //FragColor.rgb = vec3(depth / 100.0);
    //FragColor.rgb = vec3(texture(u_ssaoTex, inTexCoord).r);
    //FragColor.rgb = vec3(length(posToCamera) / 100.0);
}
