#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct VP
{
    float4x4 viewProj;
    int layerIndex;
};

struct main0_out
{
    float4 FragColor [[color(0)]];
};

struct main0_in
{
    float2 inTexCoord [[user(locn0)]];
};

fragment main0_out main0(main0_in in [[stage_in]], constant VP& u_vp [[buffer(0)]], texturecube<float> environmentMap [[texture(0)]], sampler environmentMapSmplr [[sampler(0)]])
{
    main0_out out = {};
    float4 position = u_vp.viewProj * float4((in.inTexCoord * 2.0) - float2(1.0), 1.0, 1.0);
    if (abs(position.y) == 1.0)
    {
        position.x = -position.x;
        position.z = -position.z;
        position.z -= 0.20000000298023223876953125;
    }
    position.y = -position.y;
    float3 normal = fast::normalize(position.xyz);
    float3 irradiance = float3(0.0);
    float3 up = float3(0.0, 1.0, 0.0);
    float3 right = fast::normalize(cross(up, normal));
    up = fast::normalize(cross(normal, right));
    float sampleDelta = 0.02500000037252902984619140625;
    float nbSamples = 0.0;
    for (float phi = 0.0; phi < 6.283185482025146484375; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 1.57079637050628662109375; theta += sampleDelta)
        {
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            float3 sampleVec = ((right * tangentSample.x) + (up * tangentSample.y)) + (normal * tangentSample.z);
            irradiance += ((environmentMap.sample(environmentMapSmplr, sampleVec).xyz * cos(theta)) * sin(theta));
            nbSamples += 1.0;
        }
    }
    irradiance = (irradiance * 3.1415927410125732421875) / float3(nbSamples);
    out.FragColor = float4(irradiance, 1.0);
    return out;
}

