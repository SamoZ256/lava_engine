#version 450

const vec3 _297[24] = vec3[](vec3(-0.0622651018202304840087890625, -0.05167829990386962890625, 0.0374449007213115692138671875), vec3(0.0306225009262561798095703125, -0.0203063003718852996826171875, 0.0168497003614902496337890625), vec3(-0.0411803983151912689208984375, 0.0422951988875865936279296875, 0.0077766901813447475433349609375), vec3(0.01100550033152103424072265625, 0.0414319001138210296630859375, 0.03961069881916046142578125), vec3(0.063179798424243927001953125, 0.08304969966411590576171875, 0.060910500586032867431640625), vec3(-0.0369299016892910003662109375, -0.013459700159728527069091796875, 0.08185990154743194580078125), vec3(0.087206102907657623291015625, 0.10023699700832366943359375, 0.0178864002227783203125), vec3(0.0455770008265972137451171875, -0.0230742990970611572265625, 0.0524416007101535797119140625), vec3(-0.109747000038623809814453125, 0.00853306986391544342041015625, 0.113283000886440277099609375), vec3(0.011327099986374378204345703125, 0.0026513501070439815521240234375, 0.01773869991302490234375), vec3(0.07265789806842803955078125, -0.04495500028133392333984375, 0.19894100725650787353515625), vec3(-0.001534670009277760982513427734375, 0.054966099560260772705078125, 0.0072182901203632354736328125), vec3(-0.0411426015198230743408203125, 0.0417341999709606170654296875, 0.0062495102174580097198486328125), vec3(0.0337328016757965087890625, 0.0228768996894359588623046875, 0.106853999197483062744140625), vec3(-0.0528015010058879852294921875, -0.075990997254848480224609375, 0.3923600018024444580078125), vec3(0.02374069951474666595458984375, -0.1035420000553131103515625, 0.10760299861431121826171875), vec3(-0.00895407982170581817626953125, -0.08581610023975372314453125, 0.0130407996475696563720703125), vec3(-0.0283148996531963348388671875, -0.22502100467681884765625, 0.12259300053119659423828125), vec3(0.01623429916799068450927734375, 0.0357724018394947052001953125, 0.0095959603786468505859375), vec3(0.100599996745586395263671875, -0.0333268009126186370849609375, 0.02885589934885501861572265625), vec3(0.42938899993896484375, 0.4595960080623626708984375, 0.0531716011464595794677734375), vec3(0.1520510017871856689453125, 0.2147389948368072509765625, 0.38186299800872802734375), vec3(-0.3033629953861236572265625, -0.21289099752902984619140625, 0.3471370041370391845703125), vec3(0.21230499446392059326171875, -0.122184999287128448486328125, 0.052975900471210479736328125));
#ifndef SPIRV_CROSS_CONSTANT_ID_0
#define SPIRV_CROSS_CONSTANT_ID_0 0
#endif
const int aoType = SPIRV_CROSS_CONSTANT_ID_0;
const bool _672 = (aoType == 0);
const bool _678 = (aoType == 1);
const bool _683 = (aoType == 2);

struct VP
{
    mat4 projection;
    mat4 view;
    mat4 invViewProj;
};

uniform VP u_vp;

layout(binding = 0) uniform sampler2D u_depth;
layout(binding = 1) uniform sampler2D u_normalRoughness;
layout(binding = 2) uniform sampler2D u_ssaoNoise;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out float FragColor;

vec3 reconstructPosFromDepth(mat4 invViewProj, vec2 texCoord, float depth)
{
    vec3 posInViewProj = vec3((texCoord.x * 2.0) - 1.0, ((1.0 - texCoord.y) * 2.0) - 1.0, depth);
    vec4 position = invViewProj * vec4(posInViewProj, 1.0);
    float _65 = position.w;
    vec4 _66 = position;
    vec3 _69 = _66.xyz / vec3(_65);
    position.x = _69.x;
    position.y = _69.y;
    position.z = _69.z;
    return position.xyz;
}

float ssao()
{
    float depth = texture(u_depth, inTexCoord).x;
    mat4 param = u_vp.invViewProj;
    vec2 param_1 = inTexCoord;
    float param_2 = depth;
    vec3 fragPos = reconstructPosFromDepth(param, param_1, param_2);
    vec3 normal = normalize((texture(u_normalRoughness, inTexCoord).xyz * 2.0) - vec3(1.0));
    ivec2 texDim = textureSize(u_depth, 0);
    ivec2 noiseDim = textureSize(u_ssaoNoise, 0);
    vec2 noiseUV = vec2(float(texDim.x) / float(noiseDim.x), float(texDim.y) / float(noiseDim.y)) * inTexCoord;
    vec3 randomVec = texture(u_ssaoNoise, noiseUV).xyz;
    vec3 tangent = normalize(randomVec - (normal * dot(randomVec, normal)));
    vec3 bitangent = cross(tangent, normal);
    mat3 TBN = mat3(vec3(tangent), vec3(bitangent), vec3(normal));
    float occlusion = 0.0;
    for (int i = 0; i < 24; i++)
    {
        vec3 samplePos = TBN * _297[i];
        samplePos = fragPos + (samplePos * 0.800000011920928955078125);
        vec4 viewSamplePos = u_vp.view * vec4(samplePos, 1.0);
        vec4 offset = u_vp.projection * viewSamplePos;
        float _325 = offset.w;
        vec4 _326 = offset;
        vec2 _329 = _326.xy / vec2(_325);
        offset.x = _329.x;
        offset.y = _329.y;
        vec4 _334 = offset;
        vec2 _339 = (_334.xy * 0.5) + vec2(0.5);
        offset.x = _339.x;
        offset.y = _339.y;
        float sampleDepth = texture(u_depth, vec2(offset.x, 1.0 - offset.y)).x;
        mat4 param_3 = u_vp.invViewProj;
        vec2 param_4 = vec2(offset.x, 1.0 - offset.y);
        float param_5 = sampleDepth;
        float sampleLinearDepth = (u_vp.view * vec4(reconstructPosFromDepth(param_3, param_4, param_5), 1.0)).z;
        float rangeCheck = smoothstep(0.0, 1.0, 0.800000011920928955078125 / abs(sampleLinearDepth - viewSamplePos.z));
        occlusion += ((viewSamplePos.z <= (sampleLinearDepth - 0.039999999105930328369140625)) ? rangeCheck : 0.0);
    }
    return 1.0 - (occlusion / 24.0);
}

float length2(vec3 a)
{
    return ((a.x * a.x) + (a.y * a.y)) + (a.z * a.z);
}

float computeAO(vec3 normal, vec2 direction, vec2 texelSize, vec3 fragPos)
{
    vec3 viewVector = normalize(fragPos);
    vec3 leftDirection = cross(viewVector, vec3(direction, 0.0));
    vec3 tangent = cross(normal, leftDirection);
    float tangentAngle = atan(tangent.z / length(tangent.xy));
    float sinTangentAngle = sin(tangentAngle + 0.087266445159912109375);
    float horizonAngle = tangentAngle + 0.087266445159912109375;
    vec3 horizonVector;
    for (int i = 2; i < 10; i++)
    {
        vec2 marchPosition = inTexCoord + (((texelSize * float(i)) * direction) * 8.0);
        float depth = texture(u_depth, vec2(marchPosition.x, marchPosition.y)).x;
        mat4 param = u_vp.invViewProj;
        vec2 param_1 = marchPosition;
        float param_2 = depth;
        vec3 fragPosMarch = (u_vp.view * vec4(reconstructPosFromDepth(param, param_1, param_2), 1.0)).xyz;
        vec3 crntVector = fragPosMarch - fragPos;
        vec3 param_3 = crntVector;
        if (length2(param_3) < 16.0)
        {
            horizonAngle = max(horizonAngle, atan(crntVector.z / length(crntVector.xy)));
            horizonVector = crntVector;
        }
    }
    float sinHorizonAngle = sin(horizonAngle);
    float norm = length(horizonVector) / 4.0;
    float attenuation = 1.0 - (norm * norm);
    return attenuation * (sinHorizonAngle - sinTangentAngle);
}

float hbao()
{
    vec2 screenSize = vec2(textureSize(u_depth, 0));
    vec2 noiseScale = vec2(screenSize.x / 8.0, screenSize.y / 8.0);
    vec2 noisePos = inTexCoord * noiseScale;
    float depth = texture(u_depth, inTexCoord).x;
    mat4 param = u_vp.invViewProj;
    vec2 param_1 = inTexCoord;
    float param_2 = depth;
    vec3 fragPos = (u_vp.view * vec4(reconstructPosFromDepth(param, param_1, param_2), 1.0)).xyz;
    vec3 normal = normalize((u_vp.view * vec4(texture(u_normalRoughness, inTexCoord).xyz, 1.0)).xyz);
    vec2 randomVec = texture(u_ssaoNoise, noisePos).xy;
    vec2 texelSize = vec2(1.0) / screenSize;
    float rez = 0.0;
    vec3 param_3 = normal;
    vec2 param_4 = vec2(randomVec);
    vec2 param_5 = texelSize;
    vec3 param_6 = fragPos;
    rez += computeAO(param_3, param_4, param_5, param_6);
    vec3 param_7 = normal;
    vec2 param_8 = -vec2(randomVec);
    vec2 param_9 = texelSize;
    vec3 param_10 = fragPos;
    rez += computeAO(param_7, param_8, param_9, param_10);
    vec3 param_11 = normal;
    vec2 param_12 = vec2(-randomVec.y, -randomVec.x);
    vec2 param_13 = texelSize;
    vec3 param_14 = fragPos;
    rez += computeAO(param_11, param_12, param_13, param_14);
    vec3 param_15 = normal;
    vec2 param_16 = vec2(randomVec.y, randomVec.x);
    vec2 param_17 = texelSize;
    vec3 param_18 = fragPos;
    rez += computeAO(param_15, param_16, param_17, param_18);
    return 1.0 - (rez * 0.25);
}

void main()
{
    if (_672)
    {
        FragColor = 1.0;
    }
    else
    {
        if (_678)
        {
            FragColor = ssao();
        }
        else
        {
            if (_683)
            {
                FragColor = hbao();
            }
        }
    }
}

