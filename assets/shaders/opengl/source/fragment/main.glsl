#version 460

layout(binding = 0, std140) uniform LIGHT
{
    vec4 color;
    vec3 direction;
} u_light;

layout(binding = 2, std140) uniform SHADOW
{
    mat4 VPs[3];
} u_shadow;

layout(binding = 1, std140) uniform VP
{
    mat4 invViewProj;
    vec3 cameraPos;
} u_vp;

layout(binding = 3) uniform sampler2DArrayShadow u_shadowMap;
layout(binding = 0) uniform sampler2D u_depth;
layout(binding = 1) uniform sampler2D u_normalRoughness;
layout(binding = 2) uniform sampler2D u_albedoMetallic;
layout(binding = 0) uniform samplerCube u_irradianceMap;
layout(binding = 1) uniform samplerCube u_prefilteredMap;
layout(binding = 2) uniform sampler2D u_brdfLutMap;
layout(binding = 3) uniform sampler2D u_ssaoTex;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 FragColor;

vec3 reconstructPosFromDepth(mat4 invViewProj, vec2 texCoord, float depth)
{
    vec3 posInViewProj = vec3((texCoord.x * 2.0) - 1.0, ((1.0 - texCoord.y) * 2.0) - 1.0, depth);
    vec4 position = invViewProj * vec4(posInViewProj, 1.0);
    float _87 = position.w;
    vec4 _88 = position;
    vec3 _91 = _88.xyz / vec3(_87);
    position.x = _91.x;
    position.y = _91.y;
    position.z = _91.z;
    return position.xyz;
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0)) + 1.0;
    denom = (3.1415927410125732421875 * denom) * denom;
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float num = NdotV;
    float denom = (NdotV * (1.0 - k)) + k;
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float param = NdotV;
    float param_1 = roughness;
    float ggx2 = GeometrySchlickGGX(param, param_1);
    float param_2 = NdotL;
    float param_3 = roughness;
    float ggx1 = GeometrySchlickGGX(param_2, param_3);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0, float roughness)
{
    return F0 + ((max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0));
}

vec3 calcDirectLight(vec3 norm, vec3 viewDir, vec3 F0, vec3 albedo, float roughness, float metallic)
{
    vec3 L = normalize(-u_light.direction);
    vec3 H = normalize(viewDir + L);
    float cosTheta = max(dot(norm, L), 0.0);
    vec3 radiance = vec3(1.0) * cosTheta;
    vec3 param = norm;
    vec3 param_1 = H;
    float param_2 = roughness;
    float NDF = DistributionGGX(param, param_1, param_2);
    vec3 param_3 = norm;
    vec3 param_4 = viewDir;
    vec3 param_5 = L;
    float param_6 = roughness;
    float G = GeometrySmith(param_3, param_4, param_5, param_6);
    float param_7 = max(dot(H, viewDir), 0.0);
    vec3 param_8 = F0;
    float param_9 = roughness;
    vec3 F = fresnelSchlick(param_7, param_8, param_9);
    vec3 numerator = F * (NDF * G);
    float denominator = ((4.0 * max(dot(norm, viewDir), 0.0)) * max(dot(norm, L), 0.0)) + 9.9999997473787516355514526367188e-05;
    vec3 spec = numerator / vec3(denominator);
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= roughness;
    float NdotL = max(dot(norm, L), 0.0);
    return (((((((kD * albedo) / vec3(3.1415927410125732421875)) + spec) * radiance) * NdotL) * u_light.color.xyz) * u_light.color.w) * 2.0;
}

float getVisibility(vec3 position, vec3 normal)
{
    int layer = -1;
    vec4 shadowCoord;
    for (int i = 0; i < 3; i++)
    {
        shadowCoord = u_shadow.VPs[i] * vec4(position, 1.0);
        float _351 = shadowCoord.w;
        vec4 _352 = shadowCoord;
        vec3 _355 = _352.xyz / vec3(_351);
        shadowCoord.x = _355.x;
        shadowCoord.y = _355.y;
        shadowCoord.z = _355.z;
        vec4 _362 = shadowCoord;
        vec2 _367 = (_362.xy * 0.5) + vec2(0.5);
        shadowCoord.x = _367.x;
        shadowCoord.y = _367.y;
        shadowCoord.y = 1.0 - shadowCoord.y;
        bool _379 = shadowCoord.x >= 0.00999999977648258209228515625;
        bool _386;
        if (_379)
        {
            _386 = shadowCoord.x <= 0.9900000095367431640625;
        }
        else
        {
            _386 = _379;
        }
        bool _392;
        if (_386)
        {
            _392 = shadowCoord.y >= 0.00999999977648258209228515625;
        }
        else
        {
            _392 = _386;
        }
        bool _398;
        if (_392)
        {
            _398 = shadowCoord.y <= 0.9900000095367431640625;
        }
        else
        {
            _398 = _392;
        }
        if (_398)
        {
            layer = i;
            break;
        }
    }
    if (layer == (-1))
    {
        return 1.0;
    }
    float layerRatio = 1.0;
    if (layer != 0)
    {
        layerRatio = 1.0 / (float(layer) * 2.0);
    }
    float bias = 0.0500000007450580596923828125 * tan(acos(dot(normal, u_light.direction)));
    bias = clamp(bias, 0.0, 0.001000000047497451305389404296875);
    vec4 _446 = vec4(shadowCoord.xy, float(layer), shadowCoord.z - bias);
    return texture(u_shadowMap, vec4(_446.xyz, _446.w));
}

void main()
{
    float depth = texture(u_depth, inTexCoord).x;
    mat4 param = u_vp.invViewProj;
    vec2 param_1 = inTexCoord;
    float param_2 = depth;
    vec3 position = reconstructPosFromDepth(param, param_1, param_2);
    vec4 normalRoughness = texture(u_normalRoughness, inTexCoord);
    vec3 normal = normalRoughness.xyz;
    float roughness = normalRoughness.w;
    vec4 albedoMetallic = texture(u_albedoMetallic, inTexCoord);
    vec3 albedo = albedoMetallic.xyz;
    float metallic = albedoMetallic.w;
    vec3 posToCamera = u_vp.cameraPos - position;
    float distToCamera = length(posToCamera);
    vec3 viewDir = posToCamera / vec3(distToCamera);
    vec3 F0 = vec3(0.039999999105930328369140625);
    F0 = mix(F0, albedo, vec3(metallic));
    vec3 param_3 = normal;
    vec3 param_4 = viewDir;
    vec3 param_5 = F0;
    vec3 param_6 = albedo;
    float param_7 = roughness;
    float param_8 = metallic;
    vec3 param_9 = position;
    vec3 param_10 = normal;
    vec3 result = calcDirectLight(param_3, param_4, param_5, param_6, param_7, param_8) * getVisibility(param_9, param_10);
    float param_11 = max(dot(normal, viewDir), 0.0);
    vec3 param_12 = F0;
    float param_13 = roughness;
    vec3 kS = fresnelSchlick(param_11, param_12, param_13);
    vec3 kD = vec3(1.0) - kS;
    vec3 irradiance = texture(u_irradianceMap, normal).xyz;
    vec3 diffuse = irradiance * albedo;
    vec3 R = reflect(viewDir, normal);
    R.y = -R.y;
    vec3 prefilteredColor = textureLod(u_prefilteredMap, R, roughness * 4.0).xyz;
    vec2 brdf = texture(u_brdfLutMap, vec2(max(dot(normal, viewDir), 0.0), roughness)).xy;
    vec3 specular = prefilteredColor * ((kS * brdf.x) + vec3(brdf.y));
    vec3 ambient = ((kD * diffuse) + specular) * texture(u_ssaoTex, inTexCoord).x;
    result += ambient;
    FragColor = vec4(result, 1.0);
}

