#version 450

layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec2 v_texCoord;

layout (set = 0, binding = 0) uniform sampler2D u_colorTex;
layout (set = 0, binding = 1) uniform sampler2D u_bloomTex;
/*
layout (set = 0, binding = 1) uniform sampler2D u_depth;
layout (set = 0, binding = 2) uniform sampler2D u_normalRoughness;

layout (push_constant) uniform VP {
    mat4 projection;
    mat4 view;
    vec3 cameraPos;
} u_vp;
*/

const float exposure = 1.0;

/*
//Reflection constants
const float rayStep = 0.02;
const float minRayStep = 0.05;
const float maxSteps = 100;
const int numBinarySearchSteps = 10;
const float tolerance = 0.01;

//const float reflectionSpecularFalloffExponent = 3.0;

const float borderFog = 0.03;

#define Scale vec3(.8, .8, .8)
#define K 19.19

vec3 BinarySearch(inout vec3 dir, inout vec3 hitCoord) {
    float depth = 0.0;
    vec4 projectedCoord;
    float dDepth = 0.0;

    for (int i = 0; i < numBinarySearchSteps; i++) {
        projectedCoord = u_vp.projection * u_vp.view * vec4(hitCoord, 1.0);
        projectedCoord.xy /= projectedCoord.w;
        projectedCoord.xy = projectedCoord.xy * 0.5 + 0.5;
        projectedCoord.y = 1.0 - projectedCoord.y;

        depth = texture(u_depth, projectedCoord.xy).x;

        dDepth = projectedCoord.z - depth;
        dir *= 0.5;

        if (dDepth > 0.0)
            hitCoord -= dir;
        else
            hitCoord += dir;
    }

    projectedCoord = u_vp.projection * u_vp.view * vec4(hitCoord, 1.0);
    projectedCoord.xy /= projectedCoord.w;
    projectedCoord.xy = projectedCoord.xy * 0.5 + 0.5;
    projectedCoord.y = 1.0 - projectedCoord.y;

    return abs(dDepth) < tolerance ? vec3(projectedCoord.xy, depth) : vec3(-1);
}

vec3 RayMarch(vec3 dir, inout vec3 hitCoord) {
    dir *= rayStep;
    float depth = 0.0;
    vec4 projectedCoord;
    float dDepth = -1.0;

    for (int i = 0; i < maxSteps; i++) {
        hitCoord += dir;

        projectedCoord = u_vp.projection * u_vp.view * vec4(hitCoord, 1.0);
        projectedCoord.xy /= projectedCoord.w;
        projectedCoord.xy = projectedCoord.xy * 0.5 + 0.5;
        projectedCoord.y = 1.0 - projectedCoord.y;

        depth = texture(u_depth, projectedCoord.xy).x;
        if (depth == 1.0)
            continue;

        dDepth = projectedCoord.z - depth;

        if (dDepth >= 0.0)
            return BinarySearch(dir, hitCoord);
    }

    return vec3(-1);//vec3(projectedCoord.xy, depth);
}
*/

#define FXAA_REDUCE_MIN   (1.0/ 128.0)
#define FXAA_REDUCE_MUL   (1.0 / 8.0)
#define FXAA_SPAN_MAX     8.0

vec3 fxaa(vec2 fragCoord, vec2 resolution) {
    vec2 inverseVP = 1.0 / resolution.xy;
	vec2 v_rgbNW = (fragCoord + vec2(-1.0, -1.0)) * inverseVP;
	vec2 v_rgbNE = (fragCoord + vec2(1.0, -1.0)) * inverseVP;
	vec2 v_rgbSW = (fragCoord + vec2(-1.0, 1.0)) * inverseVP;
	vec2 v_rgbSE = (fragCoord + vec2(1.0, 1.0)) * inverseVP;
	vec2 v_rgbM = vec2(fragCoord * inverseVP);
    
    vec3 rgbNW = texture(u_colorTex, v_rgbNW).xyz;
    vec3 rgbNE = texture(u_colorTex, v_rgbNE).xyz;
    vec3 rgbSW = texture(u_colorTex, v_rgbSW).xyz;
    vec3 rgbSE = texture(u_colorTex, v_rgbSE).xyz;
    vec4 texColor = texture(u_colorTex, v_rgbM);
    vec3 rgbM  = texColor.xyz;
    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    
    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
    
    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) *
                          (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
    
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(vec2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
              max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
              dir * rcpDirMin)) * inverseVP;
    
    vec3 rgbA = 0.5 * (
        texture(u_colorTex, fragCoord * inverseVP + dir * (1.0 / 3.0 - 0.5)).xyz +
        texture(u_colorTex, fragCoord * inverseVP + dir * (2.0 / 3.0 - 0.5)).xyz);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(u_colorTex, fragCoord * inverseVP + dir * -0.5).xyz +
        texture(u_colorTex, fragCoord * inverseVP + dir * 0.5).xyz);

    float lumaB = dot(rgbB, luma);

    if ((lumaB < lumaMin) || (lumaB > lumaMax))
        return rgbA;
    else
        return rgbB;
}

void main() {
    /*
    float depth = texture(u_depth, v_texCoord).x;
    vec3 normal = texture(u_normalRoughness, v_texCoord).xyz;

    vec3 position = reconstructPosFromDepth(inverse(u_vp.projection * u_vp.view), v_texCoord, depth);

    vec3 reflected = normalize(reflect(normalize(position - u_vp.cameraPos), normal));

    //Preparing for ray marching
    vec3 hitPos = position;

    //Ray marching
    vec3 coords = RayMarch(reflected * max(minRayStep, -depth), hitPos);
    */

    //vec3 hdrColor;
    //if (v_texCoord.x < 0.5)
    vec3 hdrColor = fxaa(v_texCoord * vec2(1920, 1080), vec2(1920, 1080));//texture(u_colorTex, v_texCoord).rgb;
    hdrColor = mix(hdrColor, texture(u_bloomTex, v_texCoord).rgb, 0.04);
    //else
    //    hdrColor = texture(u_colorTex, v_texCoord).rgb;
    vec3 mapped = vec3(1.0) - exp(-hdrColor * exposure);
    
    FragColor = vec4(mapped, 1.0);
    /*
    if (coords.x > 0.0 && coords.x < 1.0 && coords.y > 0.0 && coords.y < 1.0)
        FragColor = vec4(texture(u_tex, coords.xy).rgb, 1.0);
    else
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    */
    //FragColor = vec4(coords.xy, 0.0, 1.0);
    //FragColor = vec4(vec3(depth), 1.0);
    //FragColor = vec4(hdrColor / (hdrColor + 1.0), 1.0);
    //FragColor = 1.0 - FragColor;
}
