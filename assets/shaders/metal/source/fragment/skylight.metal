#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main0_out
{
    float4 FragColor [[color(0)]];
};

struct main0_in
{
    float3 v_texCoord [[user(locn0)]];
};

fragment main0_out main0(main0_in in [[stage_in]], texturecube<float> environmentMap [[texture(0)]], sampler environmentMapSmplr [[sampler(0)]])
{
    main0_out out = {};
    out.FragColor = environmentMap.sample(environmentMapSmplr, in.v_texCoord);
    return out;
}

