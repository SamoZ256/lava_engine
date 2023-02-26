#version 410

layout(std140) uniform MATERIAL
{
    vec4 albedo;
    float roughness;
    float metallic;
} u_material;

uniform sampler2D u_normalTexture;
uniform sampler2D u_metallicRoughnessTexture;
uniform sampler2D u_albedoTexture;

layout(location = 0) in vec2 v_texCoord;
layout(location = 1) in mat3 v_TBN;
layout(location = 0) out vec4 outNormalRoughness;
layout(location = 1) out vec4 outAlbedoMetallic;

void main()
{
    vec3 localNormal = texture(u_normalTexture, v_texCoord).xyz;
    localNormal = normalize((localNormal * 2.0) - vec3(1.0));
    vec3 normal = normalize(v_TBN * localNormal);
    vec2 metallicRoughness = texture(u_metallicRoughnessTexture, v_texCoord).xy;
    outNormalRoughness = vec4(normal, metallicRoughness.y * u_material.roughness);
    outAlbedoMetallic = vec4((texture(u_albedoTexture, v_texCoord).xyz * u_material.albedo.xyz) * u_material.albedo.w, metallicRoughness.x * u_material.metallic);
}

