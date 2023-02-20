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
    float2 inTexCoord [[user(locn0)]];
    float3 inTBN_0 [[user(locn1)]];
    float3 inTBN_1 [[user(locn2)]];
    float3 inTBN_2 [[user(locn3)]];
};

fragment main0_out main0(main0_in in [[stage_in]], constant MATERIAL& u_material [[buffer(0)]], texture2d<float> u_normalTexture [[texture(0)]], texture2d<float> u_metallicRoughnessTexture [[texture(1)]], texture2d<float> u_albedoTexture [[texture(2)]], sampler u_normalTextureSmplr [[sampler(0)]], sampler u_metallicRoughnessTextureSmplr [[sampler(1)]], sampler u_albedoTextureSmplr [[sampler(2)]])
{
    main0_out out = {};
    float3x3 inTBN = {};
    inTBN[0] = in.inTBN_0;
    inTBN[1] = in.inTBN_1;
    inTBN[2] = in.inTBN_2;
    float3 localNormal = u_normalTexture.sample(u_normalTextureSmplr, in.inTexCoord).xyz;
    localNormal = fast::normalize((localNormal * 2.0) - float3(1.0));
    float3 normal = fast::normalize(inTBN * localNormal);
    float2 metallicRoughness = u_metallicRoughnessTexture.sample(u_metallicRoughnessTextureSmplr, in.inTexCoord).xy;
    out.outNormalRoughness = float4(normal, metallicRoughness.y * u_material.roughness);
    out.outAlbedoMetallic = float4((u_albedoTexture.sample(u_albedoTextureSmplr, in.inTexCoord).xyz * u_material.albedo.xyz) * u_material.albedo.w, metallicRoughness.x * u_material.metallic);
    return out;
}

