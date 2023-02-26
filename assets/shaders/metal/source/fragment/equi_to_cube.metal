#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct VP
{
    float4x4 viewProj;
    int layerIndex;
};

struct main0_out
{
    float4 FragColor [[color(0)]];
};

struct main0_in
{
    float2 v_texCoord [[user(locn0)]];
};

static inline __attribute__((always_inline))
float2 sampleSphericalMap(thread const float3& v)
{
    float2 uv = float2(precise::atan2(v.z, v.x), asin(v.y));
    uv *= float2(0.159099996089935302734375, 0.3183000087738037109375);
    uv += float2(0.5);
    return uv;
}

fragment main0_out main0(main0_in in [[stage_in]], constant VP& u_vp [[buffer(0)]], texture2d<float> hdrMap [[texture(0)]], sampler hdrMapSmplr [[sampler(0)]])
{
    main0_out out = {};
    float4 position = u_vp.viewProj * float4((in.v_texCoord * 2.0) - float2(1.0), 1.0, 1.0);
    if (abs(position.y) == 1.0)
    {
        position.x = -position.x;
        position.z = -position.z;
        position.z -= 0.20000000298023223876953125;
    }
    float3 param = fast::normalize(position.xyz);
    float2 uv = sampleSphericalMap(param);
    float3 color = hdrMap.sample(hdrMapSmplr, uv).xyz;
    out.FragColor = float4(color, 1.0);
    return out;
}

