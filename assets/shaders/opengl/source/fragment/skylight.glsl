#version 460

layout(binding = 0) uniform samplerCube environmentMap;

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec3 inTexCoord;

void main()
{
    FragColor = texture(environmentMap, inTexCoord);
}

