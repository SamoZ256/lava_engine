#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];
    
    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }
    
    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }
    
    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }
    
    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
};

struct LIGHT
{
    float4 color;
    float3 direction;
};

struct SHADOW
{
    float4x4 VPs[4];
};

struct VP
{
    float4x4 invViewProj;
    float3 cameraPos;
};

constant spvUnsafeArray<float2, 16> _119 = spvUnsafeArray<float2, 16>({ float2(-0.94201624393463134765625, -0.39906215667724609375), float2(0.94558608531951904296875, -0.768907248973846435546875), float2(-0.094184100627899169921875, -0.929388701915740966796875), float2(0.34495937824249267578125, 0.29387760162353515625), float2(-0.91588580608367919921875, 0.4577143192291259765625), float2(-0.8154423236846923828125, -0.87912464141845703125), float2(-0.38277542591094970703125, 0.2767684459686279296875), float2(0.9748439788818359375, 0.7564837932586669921875), float2(0.4432332515716552734375, -0.9751155376434326171875), float2(0.5374298095703125, -0.473734200000762939453125), float2(-0.2649691104888916015625, -0.418930232524871826171875), float2(0.79197514057159423828125, 0.19090187549591064453125), float2(-0.24188840389251708984375, 0.997065067291259765625), float2(-0.8140995502471923828125, 0.91437590122222900390625), float2(0.1998412609100341796875, 0.786413669586181640625), float2(0.14383161067962646484375, -0.141007900238037109375) });
constant spvUnsafeArray<float, 4> _794 = spvUnsafeArray<float, 4>({ 8.0, 22.0, 50.0, 100.0 });

struct main0_out
{
    float4 FragColor [[color(0)]];
};

struct main0_in
{
    float2 inTexCoord [[user(locn0)]];
};

static inline __attribute__((always_inline))
float3 reconstructPosFromDepth(thread const float4x4& invViewProj, thread const float2& texCoord, thread const float& depth)
{
    float3 posInViewProj = float3((texCoord.x * 2.0) - 1.0, ((1.0 - texCoord.y) * 2.0) - 1.0, depth);
    float4 position = invViewProj * float4(posInViewProj, 1.0);
    float _147 = position.w;
    float4 _148 = position;
    float3 _151 = _148.xyz / float3(_147);
    position.x = _151.x;
    position.y = _151.y;
    position.z = _151.z;
    return position.xyz;
}

static inline __attribute__((always_inline))
float DistributionGGX(thread const float3& N, thread const float3& H, thread const float& roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = fast::max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0)) + 1.0;
    denom = (3.1415927410125732421875 * denom) * denom;
    return num / denom;
}

static inline __attribute__((always_inline))
float GeometrySchlickGGX(thread const float& NdotV, thread const float& roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float num = NdotV;
    float denom = (NdotV * (1.0 - k)) + k;
    return num / denom;
}

static inline __attribute__((always_inline))
float GeometrySmith(thread const float3& N, thread const float3& V, thread const float3& L, thread const float& roughness)
{
    float NdotV = fast::max(dot(N, V), 0.0);
    float NdotL = fast::max(dot(N, L), 0.0);
    float param = NdotV;
    float param_1 = roughness;
    float ggx2 = GeometrySchlickGGX(param, param_1);
    float param_2 = NdotL;
    float param_3 = roughness;
    float ggx1 = GeometrySchlickGGX(param_2, param_3);
    return ggx1 * ggx2;
}

static inline __attribute__((always_inline))
float3 fresnelSchlick(thread const float& cosTheta, thread const float3& F0, thread const float& roughness)
{
    return F0 + ((fast::max(float3(1.0 - roughness), F0) - F0) * pow(fast::clamp(1.0 - cosTheta, 0.0, 1.0), 5.0));
}

static inline __attribute__((always_inline))
float3 calcDirectLight(thread const float3& norm, thread const float3& viewDir, thread const float3& F0, thread const float3& albedo, thread const float& roughness, thread const float& metallic, constant LIGHT& u_light)
{
    float3 L = fast::normalize(-u_light.direction);
    float3 H = fast::normalize(viewDir + L);
    float cosTheta = fast::max(dot(norm, L), 0.0);
    float3 radiance = float3(1.0) * cosTheta;
    float3 param = norm;
    float3 param_1 = H;
    float param_2 = roughness;
    float NDF = DistributionGGX(param, param_1, param_2);
    float3 param_3 = norm;
    float3 param_4 = viewDir;
    float3 param_5 = L;
    float param_6 = roughness;
    float G = GeometrySmith(param_3, param_4, param_5, param_6);
    float param_7 = fast::max(dot(H, viewDir), 0.0);
    float3 param_8 = F0;
    float param_9 = roughness;
    float3 F = fresnelSchlick(param_7, param_8, param_9);
    float3 numerator = F * (NDF * G);
    float denominator = ((4.0 * fast::max(dot(norm, viewDir), 0.0)) * fast::max(dot(norm, L), 0.0)) + 9.9999997473787516355514526367188e-05;
    float3 spec = numerator / float3(denominator);
    float3 kS = F;
    float3 kD = float3(1.0) - kS;
    kD *= roughness;
    float NdotL = fast::max(dot(norm, L), 0.0);
    return ((((((kD * albedo) / float3(3.1415927410125732421875)) + spec) * radiance) * NdotL) * u_light.color.xyz) * u_light.color.w;
}

static inline __attribute__((always_inline))
float random(thread const float3& seed, thread const int& i)
{
    float dotProduct = dot(float4(seed, float(i)), float4(12.98980045318603515625, 78.233001708984375, 45.16400146484375, 94.67299652099609375));
    return fract(sin(dotProduct) * 43758.546875);
}

static inline __attribute__((always_inline))
float getVisibility(thread const float3& position, thread const float3& normal, thread spvUnsafeArray<float2, 16>& poissonDisk, constant LIGHT& u_light, constant SHADOW& u_shadow, texture2d_array<float> u_shadowMap, sampler u_shadowMapSmplr)
{
    int layer = -1;
    float4 shadowCoord;
    for (int i = 0; i < 4; i++)
    {
        shadowCoord = u_shadow.VPs[i] * float4(position, 1.0);
        float _430 = shadowCoord.w;
        float4 _431 = shadowCoord;
        float3 _434 = _431.xyz / float3(_430);
        shadowCoord.x = _434.x;
        shadowCoord.y = _434.y;
        shadowCoord.z = _434.z;
        float4 _441 = shadowCoord;
        float2 _446 = (_441.xy * 0.5) + float2(0.5);
        shadowCoord.x = _446.x;
        shadowCoord.y = _446.y;
        shadowCoord.y = 1.0 - shadowCoord.y;
        bool _458 = shadowCoord.x >= 0.00999999977648258209228515625;
        bool _465;
        if (_458)
        {
            _465 = shadowCoord.x <= 0.9900000095367431640625;
        }
        else
        {
            _465 = _458;
        }
        bool _471;
        if (_465)
        {
            _471 = shadowCoord.y >= 0.00999999977648258209228515625;
        }
        else
        {
            _471 = _465;
        }
        bool _477;
        if (_471)
        {
            _477 = shadowCoord.y <= 0.9900000095367431640625;
        }
        else
        {
            _477 = _471;
        }
        if (_477)
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
    float bias0 = 0.0500000007450580596923828125 * tan(acos(dot(normal, u_light.direction)));
    bias0 = fast::clamp(bias0, 0.0, 0.001000000047497451305389404296875);
    float occlusion = 0.0;
    float2 texelSize = float2(1.0) / float2(float3(int3(u_shadowMap.get_width(), u_shadowMap.get_height(), u_shadowMap.get_array_size())).xy);
    float _531;
    if (texelSize.x > texelSize.y)
    {
        _531 = texelSize.x;
    }
    else
    {
        _531 = texelSize.y;
    }
    texelSize /= float2(_531);
    for (int i_1 = 0; i_1 < 16; i_1++)
    {
        int index = i_1 % 16;
        float3 param = position;
        int param_1 = i_1;
        float2 coord = float2(shadowCoord.xy + (((((fast::normalize(poissonDisk[index]) * random(param, param_1)) * layerRatio) / float2(700.0)) * 12.0) * texelSize));
        float3 _593 = float3(coord, float(layer));
        float currentDist = (shadowCoord.z - bias0) - u_shadowMap.sample(u_shadowMapSmplr, _593.xy, uint(round(_593.z))).x;
        if (currentDist > 0.0)
        {
            occlusion += (1.0 / fast::clamp(currentDist, 1.0, 2.0));
        }
    }
    return 1.0 - ((occlusion / 16.0) * 0.89999997615814208984375);
}

fragment main0_out main0(main0_in in [[stage_in]], constant LIGHT& u_light [[buffer(0)]], constant SHADOW& u_shadow [[buffer(1)]], constant VP& u_vp [[buffer(2)]], texture2d_array<float> u_shadowMap [[texture(0)]], texture2d<float> u_depth [[texture(1)]], texture2d<float> u_normalRoughness [[texture(2)]], texture2d<float> u_albedoMetallic [[texture(3)]], texturecube<float> u_irradianceMap [[texture(4)]], texturecube<float> u_prefilteredMap [[texture(5)]], texture2d<float> u_brdfLutMap [[texture(6)]], texture2d<float> u_ssaoTex [[texture(7)]], sampler u_shadowMapSmplr [[sampler(0)]], sampler u_depthSmplr [[sampler(1)]], sampler u_normalRoughnessSmplr [[sampler(2)]], sampler u_albedoMetallicSmplr [[sampler(3)]], sampler u_irradianceMapSmplr [[sampler(4)]], sampler u_prefilteredMapSmplr [[sampler(5)]], sampler u_brdfLutMapSmplr [[sampler(6)]], sampler u_ssaoTexSmplr [[sampler(7)]])
{
    main0_out out = {};
    spvUnsafeArray<float2, 16> poissonDisk = spvUnsafeArray<float2, 16>({ float2(-0.94201624393463134765625, -0.39906215667724609375), float2(0.94558608531951904296875, -0.768907248973846435546875), float2(-0.094184100627899169921875, -0.929388701915740966796875), float2(0.34495937824249267578125, 0.29387760162353515625), float2(-0.91588580608367919921875, 0.4577143192291259765625), float2(-0.8154423236846923828125, -0.87912464141845703125), float2(-0.38277542591094970703125, 0.2767684459686279296875), float2(0.9748439788818359375, 0.7564837932586669921875), float2(0.4432332515716552734375, -0.9751155376434326171875), float2(0.5374298095703125, -0.473734200000762939453125), float2(-0.2649691104888916015625, -0.418930232524871826171875), float2(0.79197514057159423828125, 0.19090187549591064453125), float2(-0.24188840389251708984375, 0.997065067291259765625), float2(-0.8140995502471923828125, 0.91437590122222900390625), float2(0.1998412609100341796875, 0.786413669586181640625), float2(0.14383161067962646484375, -0.141007900238037109375) });
    float depth = u_depth.sample(u_depthSmplr, in.inTexCoord).x;
    float4x4 param = u_vp.invViewProj;
    float2 param_1 = in.inTexCoord;
    float param_2 = depth;
    float3 position = reconstructPosFromDepth(param, param_1, param_2);
    float4 normalRoughness = u_normalRoughness.sample(u_normalRoughnessSmplr, in.inTexCoord);
    float3 normal = normalRoughness.xyz;
    float roughness = normalRoughness.w;
    float4 albedoMetallic = u_albedoMetallic.sample(u_albedoMetallicSmplr, in.inTexCoord);
    float3 albedo = albedoMetallic.xyz;
    float metallic = albedoMetallic.w;
    float3 posToCamera = u_vp.cameraPos - position;
    float distToCamera = length(posToCamera);
    float3 viewDir = posToCamera / float3(distToCamera);
    float3 F0 = float3(0.039999999105930328369140625);
    F0 = mix(F0, albedo, float3(metallic));
    float3 param_3 = normal;
    float3 param_4 = viewDir;
    float3 param_5 = F0;
    float3 param_6 = albedo;
    float param_7 = roughness;
    float param_8 = metallic;
    float3 param_9 = position;
    float3 param_10 = normal;
    float3 result = calcDirectLight(param_3, param_4, param_5, param_6, param_7, param_8, u_light) * getVisibility(param_9, param_10, poissonDisk, u_light, u_shadow, u_shadowMap, u_shadowMapSmplr);
    float param_11 = fast::max(dot(normal, viewDir), 0.0);
    float3 param_12 = F0;
    float param_13 = roughness;
    float3 kS = fresnelSchlick(param_11, param_12, param_13);
    float3 kD = float3(1.0) - kS;
    float3 irradiance = u_irradianceMap.sample(u_irradianceMapSmplr, normal).xyz;
    float3 diffuse = irradiance * albedo;
    float3 R = reflect(viewDir, normal);
    R.y = -R.y;
    float3 prefilteredColor = u_prefilteredMap.sample(u_prefilteredMapSmplr, R, level(roughness * 4.0)).xyz;
    float2 brdf = u_brdfLutMap.sample(u_brdfLutMapSmplr, float2(fast::max(dot(normal, viewDir), 0.0), roughness)).xy;
    float3 specular = prefilteredColor * ((kS * brdf.x) + float3(brdf.y));
    float3 ambient = ((kD * diffuse) + specular) * u_ssaoTex.sample(u_ssaoTexSmplr, in.inTexCoord).x;
    result += ambient;
    out.FragColor = float4(result, 1.0);
    return out;
}

