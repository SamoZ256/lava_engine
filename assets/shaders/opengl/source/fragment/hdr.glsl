#version 410

uniform sampler2D u_colorTex;
uniform sampler2D u_bloomTex;

layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out vec4 FragColor;

vec3 fxaa(vec2 fragCoord, vec2 resolution)
{
    vec2 inverseVP = vec2(1.0) / resolution;
    vec2 v_rgbNW = (fragCoord + vec2(-1.0)) * inverseVP;
    vec2 v_rgbNE = (fragCoord + vec2(1.0, -1.0)) * inverseVP;
    vec2 v_rgbSW = (fragCoord + vec2(-1.0, 1.0)) * inverseVP;
    vec2 v_rgbSE = (fragCoord + vec2(1.0)) * inverseVP;
    vec2 v_rgbM = vec2(fragCoord * inverseVP);
    vec3 rgbNW = texture(u_colorTex, v_rgbNW).xyz;
    vec3 rgbNE = texture(u_colorTex, v_rgbNE).xyz;
    vec3 rgbSW = texture(u_colorTex, v_rgbSW).xyz;
    vec3 rgbSE = texture(u_colorTex, v_rgbSE).xyz;
    vec4 texColor = texture(u_colorTex, v_rgbM);
    vec3 rgbM = texColor.xyz;
    vec3 luma = vec3(0.2989999949932098388671875, 0.58700001239776611328125, 0.114000000059604644775390625);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM = dot(rgbM, luma);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = (lumaNW + lumaSW) - (lumaNE + lumaSE);
    float dirReduce = max((((lumaNW + lumaNE) + lumaSW) + lumaSE) * 0.03125, 0.0078125);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(vec2(8.0), max(vec2(-8.0), dir * rcpDirMin)) * inverseVP;
    vec3 rgbA = (texture(u_colorTex, (fragCoord * inverseVP) + (dir * (-0.16666667163372039794921875))).xyz + texture(u_colorTex, (fragCoord * inverseVP) + (dir * 0.16666667163372039794921875)).xyz) * 0.5;
    vec3 rgbB = (rgbA * 0.5) + ((texture(u_colorTex, (fragCoord * inverseVP) + (dir * (-0.5))).xyz + texture(u_colorTex, (fragCoord * inverseVP) + (dir * 0.5)).xyz) * 0.25);
    float lumaB = dot(rgbB, luma);
    if ((lumaB < lumaMin) || (lumaB > lumaMax))
    {
        return rgbA;
    }
    else
    {
        return rgbB;
    }
}

void main()
{
    vec2 param = v_texCoord * vec2(1920.0, 1080.0);
    vec2 param_1 = vec2(1920.0, 1080.0);
    vec3 hdrColor = fxaa(param, param_1);
    hdrColor = mix(hdrColor, texture(u_bloomTex, v_texCoord).xyz, vec3(0.039999999105930328369140625));
    vec3 mapped = vec3(1.0) - exp((-hdrColor) * 1.0);
    FragColor = vec4(mapped, 1.0);
}

