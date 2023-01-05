#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main0_out
{
    float4 FragColor [[color(0)]];
};

struct main0_in
{
    float2 inTexCoord [[user(locn0)]];
};

fragment main0_out main0(main0_in in [[stage_in]], texture2d<float> u_tex [[texture(0)]], sampler u_texSmplr [[sampler(0)]])
{
    main0_out out = {};
    float3 hdrColor = u_tex.sample(u_texSmplr, in.inTexCoord).xyz;
    float3 mapped = float3(1.0) - exp((-hdrColor) * 1.0);
    out.FragColor = float4(mapped, 1.0);
    return out;
}

