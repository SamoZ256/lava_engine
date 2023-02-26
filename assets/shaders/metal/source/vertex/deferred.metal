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
    float4x4 normalMatrix;
};

struct main0_out
{
    float2 v_texCoord [[user(locn0)]];
    float3 v_TBN_0 [[user(locn1)]];
    float3 v_TBN_1 [[user(locn2)]];
    float3 v_TBN_2 [[user(locn3)]];
    float4 gl_Position [[position]];
};

struct main0_in
{
    float3 aPosition [[attribute(0)]];
    float2 aTexCoord [[attribute(1)]];
    float3 aNormal [[attribute(2)]];
    float4 aTangent [[attribute(3)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant VP& u_vp [[buffer(0)]], constant MODEL& u_model [[buffer(1)]])
{
    main0_out out = {};
    float3x3 v_TBN = {};
    out.gl_Position = (u_vp.viewProj * u_model.model) * float4(in.aPosition, 1.0);
    out.v_texCoord = in.aTexCoord;
    float3 T = fast::normalize(float3x3(u_model.normalMatrix[0].xyz, u_model.normalMatrix[1].xyz, u_model.normalMatrix[2].xyz) * in.aTangent.xyz);
    float3 N = fast::normalize(float3x3(u_model.normalMatrix[0].xyz, u_model.normalMatrix[1].xyz, u_model.normalMatrix[2].xyz) * in.aNormal);
    T = fast::normalize(T - (N * dot(T, N)));
    float3 B = cross(N, T) * in.aTangent.w;
    v_TBN = float3x3(float3(T), float3(B), float3(N));
    out.v_TBN_0 = v_TBN[0];
    out.v_TBN_1 = v_TBN[1];
    out.v_TBN_2 = v_TBN[2];
    return out;
}

