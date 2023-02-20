#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct VP
{
    float4x4 viewProj;
};

struct MODEL
{
    float4x4 model;
};

struct main0_out
{
    float4 gl_Position [[position]];
};

struct main0_in
{
    float3 aPosition [[attribute(0)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant VP& u_vp [[buffer(0)]], constant MODEL& u_model [[buffer(1)]])
{
    main0_out out = {};
    out.gl_Position = (u_vp.viewProj * u_model.model) * float4(in.aPosition, 1.0);
    return out;
}

