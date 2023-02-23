#version 460

layout(binding = 0, std140) uniform MATERIAL
{
    vec4 albedo;
    float roughness;
    float metallic;
} u_material;

layout(binding = 2) uniform sampler2D u_normalTexture;
layout(binding = 1) uniform sampler2D u_metallicRoughnessTexture;
layout(binding = 0) uniform sampler2D u_albedoTexture;

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in mat3 inTBN;
layout(location = 0) out vec4 outNormalRoughness;
layout(location = 1) out vec4 outAlbedoMetallic;

void main()
{
    vec3 localNormal = texture(u_normalTexture, inTexCoord).xyz;
    localNormal = normalize((localNormal * 2.0) - vec3(1.0));
    vec3 normal = normalize(inTBN * localNormal);
    vec2 metallicRoughness = texture(u_metallicRoughnessTexture, inTexCoord).xy;
    outNormalRoughness = vec4(normal, metallicRoughness.y * u_material.roughness);
    outAlbedoMetallic = vec4((texture(u_albedoTexture, inTexCoord).xyz * u_material.albedo.xyz) * u_material.albedo.w, metallicRoughness.x * u_material.metallic);
}

