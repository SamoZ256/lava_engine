#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main0_out
{
    float3 FragColor [[color(0)]];
};

struct main0_in
{
    float2 v_texCoord [[user(locn0)]];
};

fragment main0_out main0(main0_in in [[stage_in]], texture2d<float> srcTexture [[texture(0)]], sampler srcTextureSmplr [[sampler(0)]])
{
    main0_out out = {};
    float2 srcResolution = float2(int2(srcTexture.get_width(), srcTexture.get_height()));
    float2 srcTexelSize = float2(1.0) / srcResolution;
    float x = srcTexelSize.x * 0.5;
    float y = srcTexelSize.y * 0.5;
    float3 a = srcTexture.sample(srcTextureSmplr, float2(in.v_texCoord.x - (2.0 * x), in.v_texCoord.y + (2.0 * y))).xyz;
    float3 b = srcTexture.sample(srcTextureSmplr, float2(in.v_texCoord.x, in.v_texCoord.y + (2.0 * y))).xyz;
    float3 c = srcTexture.sample(srcTextureSmplr, float2(in.v_texCoord.x + (2.0 * x), in.v_texCoord.y + (2.0 * y))).xyz;
    float3 d = srcTexture.sample(srcTextureSmplr, float2(in.v_texCoord.x - (2.0 * x), in.v_texCoord.y)).xyz;
    float3 e = srcTexture.sample(srcTextureSmplr, float2(in.v_texCoord.x, in.v_texCoord.y)).xyz;
    float3 f = srcTexture.sample(srcTextureSmplr, float2(in.v_texCoord.x + (2.0 * x), in.v_texCoord.y)).xyz;
    float3 g = srcTexture.sample(srcTextureSmplr, float2(in.v_texCoord.x - (2.0 * x), in.v_texCoord.y - (2.0 * y))).xyz;
    float3 h = srcTexture.sample(srcTextureSmplr, float2(in.v_texCoord.x, in.v_texCoord.y - (2.0 * y))).xyz;
    float3 i = srcTexture.sample(srcTextureSmplr, float2(in.v_texCoord.x + (2.0 * x), in.v_texCoord.y - (2.0 * y))).xyz;
    float3 j = srcTexture.sample(srcTextureSmplr, float2(in.v_texCoord.x - x, in.v_texCoord.y + y)).xyz;
    float3 k = srcTexture.sample(srcTextureSmplr, float2(in.v_texCoord.x + x, in.v_texCoord.y + y)).xyz;
    float3 l = srcTexture.sample(srcTextureSmplr, float2(in.v_texCoord.x - x, in.v_texCoord.y - y)).xyz;
    float3 m = srcTexture.sample(srcTextureSmplr, float2(in.v_texCoord.x + x, in.v_texCoord.y - y)).xyz;
    out.FragColor = e * 0.125;
    out.FragColor += ((((a + c) + g) + i) * 0.03125);
    out.FragColor += ((((b + d) + f) + h) * 0.0625);
    out.FragColor += ((((j + k) + l) + m) * 0.125);
    out.FragColor = fast::max(out.FragColor, float3(9.9999997473787516355514526367188e-05));
    return out;
}

