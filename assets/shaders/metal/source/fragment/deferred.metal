#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct MATERIAL
{
    float4 albedo;
    float roughness;
    float metallic;
};

struct VP
{
    float4x4 viewProj;
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

fragment main0_out main0(main0_in in [[stage_in]], constant MATERIAL& u_material [[buffer(0)]], texture2d<float> u_roughnessTexture [[texture(0)]], texture2d<float> u_albedoTexture [[texture(1)]], texture2d<float> u_metallicTexture [[texture(2)]], sampler u_roughnessTextureSmplr [[sampler(0)]], sampler u_albedoTextureSmplr [[sampler(1)]], sampler u_metallicTextureSmplr [[sampler(2)]])
{
    main0_out out = {};
    out.outNormalRoughness = float4(in.inNormal, (u_roughnessTexture.sample(u_roughnessTextureSmplr, in.inTexCoord) * u_material.roughness).x);
    out.outAlbedoMetallic = float4((u_albedoTexture.sample(u_albedoTextureSmplr, in.inTexCoord).xyz * u_material.albedo.xyz) * u_material.albedo.w, (u_metallicTexture.sample(u_metallicTextureSmplr, in.inTexCoord) * u_material.metallic).x);
    return out;
}

