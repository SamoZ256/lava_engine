#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct VP
{
    float4x4 viewProj;
};

struct main0_out
{
    float3 v_texCoord [[user(locn0)]];
    float4 gl_Position [[position]];
};

struct main0_in
{
    float3 aPosition [[attribute(0)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant VP& u_vp [[buffer(0)]])
{
    main0_out out = {};
    out.v_texCoord = in.aPosition;
    out.gl_Position = u_vp.viewProj * float4(in.aPosition, 1.0);
    return out;
}

