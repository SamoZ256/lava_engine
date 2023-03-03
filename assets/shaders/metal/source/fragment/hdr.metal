#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main0_out
{
    float4 FragColor [[color(0)]];
};

struct main0_in
{
    float2 v_texCoord [[user(locn0)]];
};

static inline __attribute__((always_inline))
float3 fxaa(thread const float2& fragCoord, thread const float2& resolution, texture2d<float> u_colorTex, sampler u_colorTexSmplr)
{
    float2 inverseVP = float2(1.0) / resolution;
    float2 v_rgbNW = (fragCoord + float2(-1.0)) * inverseVP;
    float2 v_rgbNE = (fragCoord + float2(1.0, -1.0)) * inverseVP;
    float2 v_rgbSW = (fragCoord + float2(-1.0, 1.0)) * inverseVP;
    float2 v_rgbSE = (fragCoord + float2(1.0)) * inverseVP;
    float2 v_rgbM = float2(fragCoord * inverseVP);
    float3 rgbNW = u_colorTex.sample(u_colorTexSmplr, v_rgbNW).xyz;
    float3 rgbNE = u_colorTex.sample(u_colorTexSmplr, v_rgbNE).xyz;
    float3 rgbSW = u_colorTex.sample(u_colorTexSmplr, v_rgbSW).xyz;
    float3 rgbSE = u_colorTex.sample(u_colorTexSmplr, v_rgbSE).xyz;
    float4 texColor = u_colorTex.sample(u_colorTexSmplr, v_rgbM);
    float3 rgbM = texColor.xyz;
    float3 luma = float3(0.2989999949932098388671875, 0.58700001239776611328125, 0.114000000059604644775390625);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM = dot(rgbM, luma);
    float lumaMin = fast::min(lumaM, fast::min(fast::min(lumaNW, lumaNE), fast::min(lumaSW, lumaSE)));
    float lumaMax = fast::max(lumaM, fast::max(fast::max(lumaNW, lumaNE), fast::max(lumaSW, lumaSE)));
    float2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = (lumaNW + lumaSW) - (lumaNE + lumaSE);
    float dirReduce = fast::max((((lumaNW + lumaNE) + lumaSW) + lumaSE) * 0.03125, 0.0078125);
    float rcpDirMin = 1.0 / (fast::min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = fast::min(float2(8.0), fast::max(float2(-8.0), dir * rcpDirMin)) * inverseVP;
    float3 rgbA = (u_colorTex.sample(u_colorTexSmplr, ((fragCoord * inverseVP) + (dir * (-0.16666667163372039794921875)))).xyz + u_colorTex.sample(u_colorTexSmplr, ((fragCoord * inverseVP) + (dir * 0.16666667163372039794921875))).xyz) * 0.5;
    float3 rgbB = (rgbA * 0.5) + ((u_colorTex.sample(u_colorTexSmplr, ((fragCoord * inverseVP) + (dir * (-0.5)))).xyz + u_colorTex.sample(u_colorTexSmplr, ((fragCoord * inverseVP) + (dir * 0.5))).xyz) * 0.25);
    float lumaB = dot(rgbB, luma);
    if ((lumaB < lumaMin) || (lumaB > lumaMax))
    {
        return rgbA;
    }
    else
    {
        return rgbB;
    }
}

fragment main0_out main0(main0_in in [[stage_in]], texture2d<float> u_colorTex [[texture(0)]], texture2d<float> u_bloomTex [[texture(1)]], texture2d<float> u_ssaoTex [[texture(2)]], sampler u_colorTexSmplr [[sampler(0)]], sampler u_bloomTexSmplr [[sampler(1)]], sampler u_ssaoTexSmplr [[sampler(2)]])
{
    main0_out out = {};
    float2 param = in.v_texCoord * float2(1920.0, 1080.0);
    float2 param_1 = float2(1920.0, 1080.0);
    float3 hdrColor = fxaa(param, param_1, u_colorTex, u_colorTexSmplr);
    hdrColor = mix(hdrColor, u_bloomTex.sample(u_bloomTexSmplr, in.v_texCoord).xyz, float3(0.039999999105930328369140625)) * u_ssaoTex.sample(u_ssaoTexSmplr, in.v_texCoord).x;
    float3 mapped = float3(1.0) - exp((-hdrColor) * 1.0);
    out.FragColor = float4(mapped, 1.0);
    return out;
}

