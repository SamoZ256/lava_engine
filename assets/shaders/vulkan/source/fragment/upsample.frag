#version 450

layout (location = 0) out vec3 FragColor;

layout (location = 0) in vec2 v_texCoord;

// This shader performs upsampling on a texture,
// as taken from Call Of Duty method, presented at ACM Siggraph 2014.

// Remember to add bilinear minification filter for this texture!
// Remember to use a floating-point texture format (for HDR)!
// Remember to use edge clamping for this texture!
layout (set = 0, binding = 0) uniform sampler2D srcTexture;

const float filterRadius = 0.005;

void main()
{
    // The filter kernel is applied with a radius, specified in texture
    // coordinates, so that the radius will vary across mip resolutions.
    const float x = filterRadius;
    const float y = filterRadius;

    // Take 9 samples around current texel:
    // a - b - c
    // d - e - f
    // g - h - i
    // === ('e' is the current texel) ===
    vec3 a = texture(srcTexture, vec2(v_texCoord.x - x, v_texCoord.y + y)).rgb;
    vec3 b = texture(srcTexture, vec2(v_texCoord.x,     v_texCoord.y + y)).rgb;
    vec3 c = texture(srcTexture, vec2(v_texCoord.x + x, v_texCoord.y + y)).rgb;

    vec3 d = texture(srcTexture, vec2(v_texCoord.x - x, v_texCoord.y)).rgb;
    vec3 e = texture(srcTexture, vec2(v_texCoord.x,     v_texCoord.y)).rgb;
    vec3 f = texture(srcTexture, vec2(v_texCoord.x + x, v_texCoord.y)).rgb;

    vec3 g = texture(srcTexture, vec2(v_texCoord.x - x, v_texCoord.y - y)).rgb;
    vec3 h = texture(srcTexture, vec2(v_texCoord.x,     v_texCoord.y - y)).rgb;
    vec3 i = texture(srcTexture, vec2(v_texCoord.x + x, v_texCoord.y - y)).rgb;

    // Apply weighted distribution, by using a 3x3 tent filter:
    //  1   | 1 2 1 |
    // -- * | 2 4 2 |
    // 16   | 1 2 1 |
    FragColor = e*4.0;
    FragColor += (b+d+f+h)*2.0;
    FragColor += (a+c+g+i);
    FragColor *= 1.0 / 16.0;
}
