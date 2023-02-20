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

struct VP
{
    float4x4 projection;
    float4x4 view;
    float4x4 invViewProj;
};

constant int aoType_tmp [[function_constant(0)]];
constant int aoType = is_function_constant_defined(aoType_tmp) ? aoType_tmp : 0;
constant bool _672 = (aoType == 0);
constant bool _678 = (aoType == 1);
constant bool _683 = (aoType == 2);

constant spvUnsafeArray<float3, 24> _297 = spvUnsafeArray<float3, 24>({ float3(-0.0622651018202304840087890625, -0.05167829990386962890625, 0.0374449007213115692138671875), float3(0.0306225009262561798095703125, -0.0203063003718852996826171875, 0.0168497003614902496337890625), float3(-0.0411803983151912689208984375, 0.0422951988875865936279296875, 0.0077766901813447475433349609375), float3(0.01100550033152103424072265625, 0.0414319001138210296630859375, 0.03961069881916046142578125), float3(0.063179798424243927001953125, 0.08304969966411590576171875, 0.060910500586032867431640625), float3(-0.0369299016892910003662109375, -0.013459700159728527069091796875, 0.08185990154743194580078125), float3(0.087206102907657623291015625, 0.10023699700832366943359375, 0.0178864002227783203125), float3(0.0455770008265972137451171875, -0.0230742990970611572265625, 0.0524416007101535797119140625), float3(-0.109747000038623809814453125, 0.00853306986391544342041015625, 0.113283000886440277099609375), float3(0.011327099986374378204345703125, 0.0026513501070439815521240234375, 0.01773869991302490234375), float3(0.07265789806842803955078125, -0.04495500028133392333984375, 0.19894100725650787353515625), float3(-0.001534670009277760982513427734375, 0.054966099560260772705078125, 0.0072182901203632354736328125), float3(-0.0411426015198230743408203125, 0.0417341999709606170654296875, 0.0062495102174580097198486328125), float3(0.0337328016757965087890625, 0.0228768996894359588623046875, 0.106853999197483062744140625), float3(-0.0528015010058879852294921875, -0.075990997254848480224609375, 0.3923600018024444580078125), float3(0.02374069951474666595458984375, -0.1035420000553131103515625, 0.10760299861431121826171875), float3(-0.00895407982170581817626953125, -0.08581610023975372314453125, 0.0130407996475696563720703125), float3(-0.0283148996531963348388671875, -0.22502100467681884765625, 0.12259300053119659423828125), float3(0.01623429916799068450927734375, 0.0357724018394947052001953125, 0.0095959603786468505859375), float3(0.100599996745586395263671875, -0.0333268009126186370849609375, 0.02885589934885501861572265625), float3(0.42938899993896484375, 0.4595960080623626708984375, 0.0531716011464595794677734375), float3(0.1520510017871856689453125, 0.2147389948368072509765625, 0.38186299800872802734375), float3(-0.3033629953861236572265625, -0.21289099752902984619140625, 0.3471370041370391845703125), float3(0.21230499446392059326171875, -0.122184999287128448486328125, 0.052975900471210479736328125) });

struct main0_out
{
    float FragColor [[color(0)]];
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
    float _65 = position.w;
    float4 _66 = position;
    float3 _69 = _66.xyz / float3(_65);
    position.x = _69.x;
    position.y = _69.y;
    position.z = _69.z;
    return position.xyz;
}

static inline __attribute__((always_inline))
float ssao(texture2d<float> u_depth, sampler u_depthSmplr, thread float2& inTexCoord, constant VP& u_vp, texture2d<float> u_normalRoughness, sampler u_normalRoughnessSmplr, texture2d<float> u_ssaoNoise, sampler u_ssaoNoiseSmplr)
{
    float depth = u_depth.sample(u_depthSmplr, inTexCoord).x;
    float4x4 param = u_vp.invViewProj;
    float2 param_1 = inTexCoord;
    float param_2 = depth;
    float3 fragPos = reconstructPosFromDepth(param, param_1, param_2);
    float3 normal = fast::normalize((u_normalRoughness.sample(u_normalRoughnessSmplr, inTexCoord).xyz * 2.0) - float3(1.0));
    int2 texDim = int2(u_depth.get_width(), u_depth.get_height());
    int2 noiseDim = int2(u_ssaoNoise.get_width(), u_ssaoNoise.get_height());
    float2 noiseUV = float2(float(texDim.x) / float(noiseDim.x), float(texDim.y) / float(noiseDim.y)) * inTexCoord;
    float3 randomVec = u_ssaoNoise.sample(u_ssaoNoiseSmplr, noiseUV).xyz;
    float3 tangent = fast::normalize(randomVec - (normal * dot(randomVec, normal)));
    float3 bitangent = cross(tangent, normal);
    float3x3 TBN = float3x3(float3(tangent), float3(bitangent), float3(normal));
    float occlusion = 0.0;
    for (int i = 0; i < 24; i++)
    {
        float3 samplePos = TBN * _297[i];
        samplePos = fragPos + (samplePos * 0.800000011920928955078125);
        float4 viewSamplePos = u_vp.view * float4(samplePos, 1.0);
        float4 offset = u_vp.projection * viewSamplePos;
        float _325 = offset.w;
        float4 _326 = offset;
        float2 _329 = _326.xy / float2(_325);
        offset.x = _329.x;
        offset.y = _329.y;
        float4 _334 = offset;
        float2 _339 = (_334.xy * 0.5) + float2(0.5);
        offset.x = _339.x;
        offset.y = _339.y;
        float sampleDepth = u_depth.sample(u_depthSmplr, float2(offset.x, 1.0 - offset.y)).x;
        float4x4 param_3 = u_vp.invViewProj;
        float2 param_4 = float2(offset.x, 1.0 - offset.y);
        float param_5 = sampleDepth;
        float sampleLinearDepth = (u_vp.view * float4(reconstructPosFromDepth(param_3, param_4, param_5), 1.0)).z;
        float rangeCheck = smoothstep(0.0, 1.0, 0.800000011920928955078125 / abs(sampleLinearDepth - viewSamplePos.z));
        occlusion += ((viewSamplePos.z <= (sampleLinearDepth - 0.039999999105930328369140625)) ? rangeCheck : 0.0);
    }
    return 1.0 - (occlusion / 24.0);
}

static inline __attribute__((always_inline))
float length2(thread const float3& a)
{
    return ((a.x * a.x) + (a.y * a.y)) + (a.z * a.z);
}

static inline __attribute__((always_inline))
float computeAO(thread const float3& normal, thread const float2& direction, thread const float2& texelSize, thread const float3& fragPos, texture2d<float> u_depth, sampler u_depthSmplr, thread float2& inTexCoord, constant VP& u_vp)
{
    float3 viewVector = fast::normalize(fragPos);
    float3 leftDirection = cross(viewVector, float3(direction, 0.0));
    float3 tangent = cross(normal, leftDirection);
    float tangentAngle = atan(tangent.z / length(tangent.xy));
    float sinTangentAngle = sin(tangentAngle + 0.087266445159912109375);
    float horizonAngle = tangentAngle + 0.087266445159912109375;
    float3 horizonVector;
    for (int i = 2; i < 10; i++)
    {
        float2 marchPosition = inTexCoord + (((texelSize * float(i)) * direction) * 8.0);
        float depth = u_depth.sample(u_depthSmplr, float2(marchPosition.x, marchPosition.y)).x;
        float4x4 param = u_vp.invViewProj;
        float2 param_1 = marchPosition;
        float param_2 = depth;
        float3 fragPosMarch = (u_vp.view * float4(reconstructPosFromDepth(param, param_1, param_2), 1.0)).xyz;
        float3 crntVector = fragPosMarch - fragPos;
        float3 param_3 = crntVector;
        if (length2(param_3) < 16.0)
        {
            horizonAngle = fast::max(horizonAngle, atan(crntVector.z / length(crntVector.xy)));
            horizonVector = crntVector;
        }
    }
    float sinHorizonAngle = sin(horizonAngle);
    float norm = length(horizonVector) / 4.0;
    float attenuation = 1.0 - (norm * norm);
    return attenuation * (sinHorizonAngle - sinTangentAngle);
}

static inline __attribute__((always_inline))
float hbao(texture2d<float> u_depth, sampler u_depthSmplr, thread float2& inTexCoord, constant VP& u_vp, texture2d<float> u_normalRoughness, sampler u_normalRoughnessSmplr, texture2d<float> u_ssaoNoise, sampler u_ssaoNoiseSmplr)
{
    float2 screenSize = float2(int2(u_depth.get_width(), u_depth.get_height()));
    float2 noiseScale = float2(screenSize.x / 8.0, screenSize.y / 8.0);
    float2 noisePos = inTexCoord * noiseScale;
    float depth = u_depth.sample(u_depthSmplr, inTexCoord).x;
    float4x4 param = u_vp.invViewProj;
    float2 param_1 = inTexCoord;
    float param_2 = depth;
    float3 fragPos = (u_vp.view * float4(reconstructPosFromDepth(param, param_1, param_2), 1.0)).xyz;
    float3 normal = fast::normalize((u_vp.view * float4(u_normalRoughness.sample(u_normalRoughnessSmplr, inTexCoord).xyz, 1.0)).xyz);
    float2 randomVec = u_ssaoNoise.sample(u_ssaoNoiseSmplr, noisePos).xy;
    float2 texelSize = float2(1.0) / screenSize;
    float rez = 0.0;
    float3 param_3 = normal;
    float2 param_4 = float2(randomVec);
    float2 param_5 = texelSize;
    float3 param_6 = fragPos;
    rez += computeAO(param_3, param_4, param_5, param_6, u_depth, u_depthSmplr, inTexCoord, u_vp);
    float3 param_7 = normal;
    float2 param_8 = -float2(randomVec);
    float2 param_9 = texelSize;
    float3 param_10 = fragPos;
    rez += computeAO(param_7, param_8, param_9, param_10, u_depth, u_depthSmplr, inTexCoord, u_vp);
    float3 param_11 = normal;
    float2 param_12 = float2(-randomVec.y, -randomVec.x);
    float2 param_13 = texelSize;
    float3 param_14 = fragPos;
    rez += computeAO(param_11, param_12, param_13, param_14, u_depth, u_depthSmplr, inTexCoord, u_vp);
    float3 param_15 = normal;
    float2 param_16 = float2(randomVec.y, randomVec.x);
    float2 param_17 = texelSize;
    float3 param_18 = fragPos;
    rez += computeAO(param_15, param_16, param_17, param_18, u_depth, u_depthSmplr, inTexCoord, u_vp);
    return 1.0 - (rez * 0.25);
}

fragment main0_out main0(main0_in in [[stage_in]], constant VP& u_vp [[buffer(0)]], texture2d<float> u_depth [[texture(0)]], texture2d<float> u_normalRoughness [[texture(1)]], texture2d<float> u_ssaoNoise [[texture(2)]], sampler u_depthSmplr [[sampler(0)]], sampler u_normalRoughnessSmplr [[sampler(1)]], sampler u_ssaoNoiseSmplr [[sampler(2)]])
{
    main0_out out = {};
    if (_672)
    {
        out.FragColor = 1.0;
    }
    else
    {
        if (_678)
        {
            out.FragColor = ssao(u_depth, u_depthSmplr, in.inTexCoord, u_vp, u_normalRoughness, u_normalRoughnessSmplr, u_ssaoNoise, u_ssaoNoiseSmplr);
        }
        else
        {
            if (_683)
            {
                out.FragColor = hbao(u_depth, u_depthSmplr, in.inTexCoord, u_vp, u_normalRoughness, u_normalRoughnessSmplr, u_ssaoNoise, u_ssaoNoiseSmplr);
            }
        }
    }
    return out;
}

