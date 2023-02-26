#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct MATERIAL
{
    float4 albedo;
    float roughness;
    float metallic;
};

struct main0_out
{
    float4 outNormalRoughness [[color(0)]];
    float4 outAlbedoMetallic [[color(1)]];
};

struct main0_in
{
    float2 v_texCoord [[user(locn0)]];
    float3 v_TBN_0 [[user(locn1)]];
    float3 v_TBN_1 [[user(locn2)]];
    float3 v_TBN_2 [[user(locn3)]];
};

fragment main0_out main0(main0_in in [[stage_in]], constant MATERIAL& u_material [[buffer(0)]], texture2d<float> u_normalTexture [[texture(0)]], texture2d<float> u_metallicRoughnessTexture [[texture(1)]], texture2d<float> u_albedoTexture [[texture(2)]], sampler u_normalTextureSmplr [[sampler(0)]], sampler u_metallicRoughnessTextureSmplr [[sampler(1)]], sampler u_albedoTextureSmplr [[sampler(2)]])
{
    main0_out out = {};
    float3x3 v_TBN = {};
    v_TBN[0] = in.v_TBN_0;
    v_TBN[1] = in.v_TBN_1;
    v_TBN[2] = in.v_TBN_2;
    float3 localNormal = u_normalTexture.sample(u_normalTextureSmplr, in.v_texCoord).xyz;
    localNormal = fast::normalize((localNormal * 2.0) - float3(1.0));
    float3 normal = fast::normalize(v_TBN * localNormal);
    float2 metallicRoughness = u_metallicRoughnessTexture.sample(u_metallicRoughnessTextureSmplr, in.v_texCoord).xy;
    out.outNormalRoughness = float4(normal, metallicRoughness.y * u_material.roughness);
    out.outAlbedoMetallic = float4((u_albedoTexture.sample(u_albedoTextureSmplr, in.v_texCoord).xyz * u_material.albedo.xyz) * u_material.albedo.w, metallicRoughness.x * u_material.metallic);
    return out;
}

