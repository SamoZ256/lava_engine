#version 460

layout(binding = 0, std140) uniform VP
{
    mat4 viewProj;
    int layerIndex;
} u_vp;

layout(binding = 1) uniform sampler2D hdrMap;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 FragColor;

vec2 sampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= vec2(0.159099996089935302734375, 0.3183000087738037109375);
    uv += vec2(0.5);
    return uv;
}

void main()
{
    vec4 position = u_vp.viewProj * vec4((inTexCoord * 2.0) - vec2(1.0), 1.0, 1.0);
    if (abs(position.y) == 1.0)
    {
        position.x = -position.x;
        position.z = -position.z;
        position.z -= 0.20000000298023223876953125;
    }
    vec3 param = normalize(position.xyz);
    vec2 uv = sampleSphericalMap(param);
    vec3 color = texture(hdrMap, uv).xyz;
    FragColor = vec4(color, 1.0);
}

