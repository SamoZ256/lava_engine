#version 410

uniform sampler2D srcTexture;

layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out vec3 FragColor;

void main()
{
    vec3 a = texture(srcTexture, vec2(v_texCoord.x - 0.004999999888241291046142578125, v_texCoord.y + 0.004999999888241291046142578125)).xyz;
    vec3 b = texture(srcTexture, vec2(v_texCoord.x, v_texCoord.y + 0.004999999888241291046142578125)).xyz;
    vec3 c = texture(srcTexture, vec2(v_texCoord.x + 0.004999999888241291046142578125, v_texCoord.y + 0.004999999888241291046142578125)).xyz;
    vec3 d = texture(srcTexture, vec2(v_texCoord.x - 0.004999999888241291046142578125, v_texCoord.y)).xyz;
    vec3 e = texture(srcTexture, vec2(v_texCoord.x, v_texCoord.y)).xyz;
    vec3 f = texture(srcTexture, vec2(v_texCoord.x + 0.004999999888241291046142578125, v_texCoord.y)).xyz;
    vec3 g = texture(srcTexture, vec2(v_texCoord.x - 0.004999999888241291046142578125, v_texCoord.y - 0.004999999888241291046142578125)).xyz;
    vec3 h = texture(srcTexture, vec2(v_texCoord.x, v_texCoord.y - 0.004999999888241291046142578125)).xyz;
    vec3 i = texture(srcTexture, vec2(v_texCoord.x + 0.004999999888241291046142578125, v_texCoord.y - 0.004999999888241291046142578125)).xyz;
    FragColor = e * 4.0;
    FragColor += ((((b + d) + f) + h) * 2.0);
    FragColor += (((a + c) + g) + i);
    FragColor *= 0.0625;
}

