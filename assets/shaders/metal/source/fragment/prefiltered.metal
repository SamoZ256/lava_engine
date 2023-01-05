#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct VP
{
    float4x4 viewProj;
    int layerIndex;
};

struct ROUGHNESS
{
    float roughness;
};

struct main0_out
{
    float4 FragColor [[color(0)]];
};

struct main0_in
{
    float2 inTexCoord [[user(locn0)]];
};

static inline __attribute__((always_inline))
float RadicalInverse_VdC(thread uint& bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 1431655765u) << 1u) | ((bits & 2863311530u) >> 1u);
    bits = ((bits & 858993459u) << 2u) | ((bits & 3435973836u) >> 2u);
    bits = ((bits & 252645135u) << 4u) | ((bits & 4042322160u) >> 4u);
    bits = ((bits & 16711935u) << 8u) | ((bits & 4278255360u) >> 8u);
    return float(bits) * 2.3283064365386962890625e-10;
}

static inline __attribute__((always_inline))
float2 Hammersley(thread const uint& i, thread const uint& N)
{
    uint param = i;
    float _131 = RadicalInverse_VdC(param);
    return float2(float(i) / float(N), _131);
}

static inline __attribute__((always_inline))
float3 ImportanceSampleGGX(thread const float2& Xi, thread const float3& N, thread const float& roughness)
{
    float a = roughness * roughness;
    float phi = 6.283185482025146484375 * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (((a * a) - 1.0) * Xi.y)));
    float sinTheta = sqrt(1.0 - (cosTheta * cosTheta));
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;
    float3 up = select(float3(1.0, 0.0, 0.0), float3(0.0, 0.0, 1.0), bool3(abs(N.z) < 0.999000012874603271484375));
    float3 tangent = fast::normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);
    float3 sampleVec = ((tangent * H.x) + (bitangent * H.y)) + (N * H.z);
    return fast::normalize(sampleVec);
}

static inline __attribute__((always_inline))
float DistributionGGX(thread const float3& N, thread const float3& H, thread const float& roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = fast::max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0)) + 1.0;
    denom = (3.1415927410125732421875 * denom) * denom;
    return nom / denom;
}

fragment main0_out main0(main0_in in [[stage_in]], constant VP& u_vp [[buffer(0)]], constant ROUGHNESS& u_roughness [[buffer(1)]], texturecube<float> environmentMap [[texture(0)]], sampler environmentMapSmplr [[sampler(0)]])
{
    main0_out out = {};
    float4 position = u_vp.viewProj * float4((in.inTexCoord * 2.0) - float2(1.0), 1.0, 1.0);
    if (abs(position.y) == 1.0)
    {
        position.x = -position.x;
        position.z = -position.z;
        position.z -= 0.20000000298023223876953125;
    }
    position.y = -position.y;
    float3 N = fast::normalize(position.xyz);
    float3 R = N;
    float3 V = R;
    float3 prefilteredColor = float3(0.0);
    float totalWeight = 0.0;
    float _370;
    for (uint i = 0u; i < 1024u; i++)
    {
        uint param = i;
        uint param_1 = 1024u;
        float2 Xi = Hammersley(param, param_1);
        float2 param_2 = Xi;
        float3 param_3 = N;
        float param_4 = u_roughness.roughness;
        float3 H = ImportanceSampleGGX(param_2, param_3, param_4);
        float3 L = fast::normalize((H * (2.0 * dot(V, H))) - V);
        float NdotL = fast::max(dot(N, L), 0.0);
        if (NdotL > 0.0)
        {
            float3 param_5 = N;
            float3 param_6 = H;
            float param_7 = u_roughness.roughness;
            float D = DistributionGGX(param_5, param_6, param_7);
            float NdotH = fast::max(dot(N, H), 0.0);
            float HdotV = fast::max(dot(H, V), 0.0);
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
            prefilteredColor += (environmentMap.sample(environmentMapSmplr, L, level(mipLevel)).xyz * NdotL);
            totalWeight += NdotL;
        }
    }
    prefilteredColor /= float3(totalWeight);
    out.FragColor = float4(prefilteredColor, 1.0);
    return out;
}

