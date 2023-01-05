#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct MODEL
{
    float4x4 model;
    float4x4 normalMatrix;
};

struct VP
{
    float4x4 viewProj;
};

struct main0_out
{
    float2 outTexCoord [[user(locn0)]];
    float3 outNormal [[user(locn1)]];
    float4 gl_Position [[position]];
};

struct main0_in
{
    float3 aPosition [[attribute(0)]];
    float2 aTexCoord [[attribute(1)]];
    float3 aNormal [[attribute(2)]];
    float3 aTangent [[attribute(3)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant MODEL& u_model [[buffer(0)]], constant VP& u_vp [[buffer(1)]])
{
    main0_out out = {};
    float4 gloablPos = u_model.model * float4(in.aPosition, 1.0);
    out.gl_Position = u_vp.viewProj * gloablPos;
    out.outTexCoord = in.aTexCoord;
    float3 T = fast::normalize(float3x3(u_model.normalMatrix[0].xyz, u_model.normalMatrix[1].xyz, u_model.normalMatrix[2].xyz) * in.aTangent);
    out.outNormal = fast::normalize(float3x3(u_model.normalMatrix[0].xyz, u_model.normalMatrix[1].xyz, u_model.normalMatrix[2].xyz) * in.aNormal);
    return out;
}

