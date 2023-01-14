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
    float3 inNormal [[user(locn1)]];
};

fragment main0_out main0(main0_in in [[stage_in]], constant MATERIAL& u_material [[buffer(0)]], texture2d<float> u_metallicRoughnessTexture [[texture(0)]], texture2d<float> u_albedoTexture [[texture(1)]], sampler u_metallicRoughnessTextureSmplr [[sampler(0)]], sampler u_albedoTextureSmplr [[sampler(1)]])
{
    main0_out out = {};
    float2 metallicRoughness = u_metallicRoughnessTexture.sample(u_metallicRoughnessTextureSmplr, in.inTexCoord).xy;
    out.outNormalRoughness = float4(in.inNormal, metallicRoughness.y * u_material.roughness);
    out.outAlbedoMetallic = float4((u_albedoTexture.sample(u_albedoTextureSmplr, in.inTexCoord).xyz * u_material.albedo.xyz) * u_material.albedo.w, metallicRoughness.x * u_material.metallic);
    return out;
}

