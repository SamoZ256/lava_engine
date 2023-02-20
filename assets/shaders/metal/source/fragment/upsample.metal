#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main0_out
{
    float3 FragColor [[color(0)]];
};

struct main0_in
{
    float2 inTexCoord [[user(locn0)]];
};

fragment main0_out main0(main0_in in [[stage_in]], texture2d<float> srcTexture [[texture(0)]], sampler srcTextureSmplr [[sampler(0)]])
{
    main0_out out = {};
    float3 a = srcTexture.sample(srcTextureSmplr, float2(in.inTexCoord.x - 0.004999999888241291046142578125, in.inTexCoord.y + 0.004999999888241291046142578125)).xyz;
    float3 b = srcTexture.sample(srcTextureSmplr, float2(in.inTexCoord.x, in.inTexCoord.y + 0.004999999888241291046142578125)).xyz;
    float3 c = srcTexture.sample(srcTextureSmplr, float2(in.inTexCoord.x + 0.004999999888241291046142578125, in.inTexCoord.y + 0.004999999888241291046142578125)).xyz;
    float3 d = srcTexture.sample(srcTextureSmplr, float2(in.inTexCoord.x - 0.004999999888241291046142578125, in.inTexCoord.y)).xyz;
    float3 e = srcTexture.sample(srcTextureSmplr, float2(in.inTexCoord.x, in.inTexCoord.y)).xyz;
    float3 f = srcTexture.sample(srcTextureSmplr, float2(in.inTexCoord.x + 0.004999999888241291046142578125, in.inTexCoord.y)).xyz;
    float3 g = srcTexture.sample(srcTextureSmplr, float2(in.inTexCoord.x - 0.004999999888241291046142578125, in.inTexCoord.y - 0.004999999888241291046142578125)).xyz;
    float3 h = srcTexture.sample(srcTextureSmplr, float2(in.inTexCoord.x, in.inTexCoord.y - 0.004999999888241291046142578125)).xyz;
    float3 i = srcTexture.sample(srcTextureSmplr, float2(in.inTexCoord.x + 0.004999999888241291046142578125, in.inTexCoord.y - 0.004999999888241291046142578125)).xyz;
    out.FragColor = e * 4.0;
    out.FragColor += ((((b + d) + f) + h) * 2.0);
    out.FragColor += (((a + c) + g) + i);
    out.FragColor *= 0.0625;
    return out;
}

