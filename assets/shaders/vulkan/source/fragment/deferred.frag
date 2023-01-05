#version 460

//layout (location = 0) out vec4 outPositionDepth;
layout (location = 0) out vec4 outNormalRoughness;
layout (location = 1) out vec4 outAlbedoMetallic;

//layout (location = 0) in vec3 inPosition;
layout (location = 0) in vec2 inTexCoord;
layout (location = 1) in vec3 inNormal;
//layout (location = 2) in mat3 inTBN;

layout (set = 0, binding = 0) uniform VP {
    mat4 viewProj;
    //vec3 cameraPos;
} u_vp;

layout (set = 1, binding = 0) uniform MATERIAL {
    vec4 albedo;
    float roughness;
    float metallic;
} u_material;

layout (set = 2, binding = 0) uniform sampler2D u_albedoTexture;
layout (set = 2, binding = 1) uniform sampler2D u_roughnessTexture;
layout (set = 2, binding = 2) uniform sampler2D u_metallicTexture;
//layout (set = 2, binding = 3) uniform sampler2D u_normalTexture;

void main() {
    //vec4 albedo = texture(u_albedoTexture, inTexCoord);
    //if (albedo.a < 0.2)
    //    discard;
    //outPositionDepth = vec4(/*u_vp.cameraPos - */inPosition, inDepth);
    /*
    vec3 normal = normalize(texture(u_normalTexture, inTexCoord).rgb);
    normal = normal * 2.0 - 1.0;
    normal = normalize(inTBN * normal);
    */

    outNormalRoughness = vec4(inNormal, texture(u_roughnessTexture, inTexCoord) * u_material.roughness);
    outAlbedoMetallic = vec4(texture(u_albedoTexture, inTexCoord).rgb * u_material.albedo.rgb * u_material.albedo.a, texture(u_metallicTexture, inTexCoord) * u_material.metallic);
}
