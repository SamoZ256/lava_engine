#version 410

layout(std140) uniform VP
{
    mat4 viewProj;
    int layerIndex;
} u_vp;

layout(std140) uniform ROUGHNESS
{
    float roughness;
} u_roughness;

uniform samplerCube environmentMap;

layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out vec4 FragColor;

float RadicalInverse_VdC(inout uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 1431655765u) << 1u) | ((bits & 2863311530u) >> 1u);
    bits = ((bits & 858993459u) << 2u) | ((bits & 3435973836u) >> 2u);
    bits = ((bits & 252645135u) << 4u) | ((bits & 4042322160u) >> 4u);
    bits = ((bits & 16711935u) << 8u) | ((bits & 4278255360u) >> 8u);
    return float(bits) * 2.3283064365386962890625e-10;
}

vec2 Hammersley(uint i, uint N)
{
    uint param = i;
    float _131 = RadicalInverse_VdC(param);
    return vec2(float(i) / float(N), _131);
}

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
    float a = roughness * roughness;
    float phi = 6.283185482025146484375 * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (((a * a) - 1.0) * Xi.y)));
    float sinTheta = sqrt(1.0 - (cosTheta * cosTheta));
    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;
    bvec3 _188 = bvec3(abs(N.z) < 0.999000012874603271484375);
    vec3 up = vec3(_188.x ? vec3(0.0, 0.0, 1.0).x : vec3(1.0, 0.0, 0.0).x, _188.y ? vec3(0.0, 0.0, 1.0).y : vec3(1.0, 0.0, 0.0).y, _188.z ? vec3(0.0, 0.0, 1.0).z : vec3(1.0, 0.0, 0.0).z);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    vec3 sampleVec = ((tangent * H.x) + (bitangent * H.y)) + (N * H.z);
    return normalize(sampleVec);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0)) + 1.0;
    denom = (3.1415927410125732421875 * denom) * denom;
    return nom / denom;
}

void main()
{
    vec4 position = u_vp.viewProj * vec4((v_texCoord * 2.0) - vec2(1.0), 1.0, 1.0);
    if (abs(position.y) == 1.0)
    {
        position.x = -position.x;
        position.z = -position.z;
        position.z -= 0.20000000298023223876953125;
    }
    position.y = -position.y;
    vec3 N = normalize(position.xyz);
    vec3 R = N;
    vec3 V = R;
    vec3 prefilteredColor = vec3(0.0);
    float totalWeight = 0.0;
    float _370;
    for (uint i = 0u; i < 1024u; i++)
    {
        uint param = i;
        uint param_1 = 1024u;
        vec2 Xi = Hammersley(param, param_1);
        vec2 param_2 = Xi;
        vec3 param_3 = N;
        float param_4 = u_roughness.roughness;
        vec3 H = ImportanceSampleGGX(param_2, param_3, param_4);
        vec3 L = normalize((H * (2.0 * dot(V, H))) - V);
        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0)
        {
            vec3 param_5 = N;
            vec3 param_6 = H;
            float param_7 = u_roughness.roughness;
            float D = DistributionGGX(param_5, param_6, param_7);
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);
            float pdf = ((D * NdotH) / (4.0 * HdotV)) + 9.9999997473787516355514526367188e-05;
            float resolution = 512.0;
            float saTexel = 12.56637096405029296875 / ((6.0 * resolution) * resolution);
            float saSample = 1.0 / ((1024.0 * pdf) + 9.9999997473787516355514526367188e-05);
            if (u_roughness.roughness == 0.0)
            {
                _370 = 0.0;
            }
            else
            {
                _370 = 0.5 * log2(saSample / saTexel);
            }
            float mipLevel = _370;
            prefilteredColor += (textureLod(environmentMap, L, mipLevel).xyz * NdotL);
            totalWeight += NdotL;
        }
    }
    prefilteredColor /= vec3(totalWeight);
    FragColor = vec4(prefilteredColor, 1.0);
}

