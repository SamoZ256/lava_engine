#version 410

uniform samplerCube environmentMap;

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec3 v_texCoord;

void main()
{
    FragColor = texture(environmentMap, v_texCoord);
}

