#version 410

layout(std140) uniform VP
{
    mat4 viewProj;
    int layerIndex;
} u_vp;

uniform samplerCube environmentMap;

layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out vec4 FragColor;

void main()
{
    vec4 position = u_vp.viewProj * vec4((v_texCoord * 2.0) - vec2(1.0), 1.0, 1.0);
    if (abs(position.y) == 1.0)
    {
        position.x = -position.x;
        position.z = -position.z;
        position.z -= 0.20000000298023223876953125;
    }
    position.y = -position.y;
    vec3 normal = normalize(position.xyz);
    vec3 irradiance = vec3(0.0);
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));
    float sampleDelta = 0.02500000037252902984619140625;
    float nbSamples = 0.0;
    for (float phi = 0.0; phi < 6.283185482025146484375; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 1.57079637050628662109375; theta += sampleDelta)
        {
            vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            vec3 sampleVec = ((right * tangentSample.x) + (up * tangentSample.y)) + (normal * tangentSample.z);
            irradiance += ((texture(environmentMap, sampleVec).xyz * cos(theta)) * sin(theta));
            nbSamples += 1.0;
        }
    }
    irradiance = (irradiance * 3.1415927410125732421875) / vec3(nbSamples);
    FragColor = vec4(irradiance, 1.0);
}

