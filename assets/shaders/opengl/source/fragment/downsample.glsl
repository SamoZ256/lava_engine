#version 460

layout(binding = 0) uniform sampler2D srcTexture;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec3 FragColor;

void main()
{
    vec2 srcResolution = vec2(textureSize(srcTexture, 0));
    vec2 srcTexelSize = vec2(1.0) / srcResolution;
    float x = srcTexelSize.x * 0.5;
    float y = srcTexelSize.y * 0.5;
    vec3 a = texture(srcTexture, vec2(inTexCoord.x - (2.0 * x), inTexCoord.y + (2.0 * y))).xyz;
    vec3 b = texture(srcTexture, vec2(inTexCoord.x, inTexCoord.y + (2.0 * y))).xyz;
    vec3 c = texture(srcTexture, vec2(inTexCoord.x + (2.0 * x), inTexCoord.y + (2.0 * y))).xyz;
    vec3 d = texture(srcTexture, vec2(inTexCoord.x - (2.0 * x), inTexCoord.y)).xyz;
    vec3 e = texture(srcTexture, vec2(inTexCoord.x, inTexCoord.y)).xyz;
    vec3 f = texture(srcTexture, vec2(inTexCoord.x + (2.0 * x), inTexCoord.y)).xyz;
    vec3 g = texture(srcTexture, vec2(inTexCoord.x - (2.0 * x), inTexCoord.y - (2.0 * y))).xyz;
    vec3 h = texture(srcTexture, vec2(inTexCoord.x, inTexCoord.y - (2.0 * y))).xyz;
    vec3 i = texture(srcTexture, vec2(inTexCoord.x + (2.0 * x), inTexCoord.y - (2.0 * y))).xyz;
    vec3 j = texture(srcTexture, vec2(inTexCoord.x - x, inTexCoord.y + y)).xyz;
    vec3 k = texture(srcTexture, vec2(inTexCoord.x + x, inTexCoord.y + y)).xyz;
    vec3 l = texture(srcTexture, vec2(inTexCoord.x - x, inTexCoord.y - y)).xyz;
    vec3 m = texture(srcTexture, vec2(inTexCoord.x + x, inTexCoord.y - y)).xyz;
    FragColor = e * 0.125;
    FragColor += ((((a + c) + g) + i) * 0.03125);
    FragColor += ((((b + d) + f) + h) * 0.0625);
    FragColor += ((((j + k) + l) + m) * 0.125);
    FragColor = max(FragColor, vec3(9.9999997473787516355514526367188e-05));
}

