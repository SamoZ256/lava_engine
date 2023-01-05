#version 460

#include "../Utility/utility.glsl"

layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec2 inTexCoord;

layout (set = 0, binding = 0) uniform sampler2D u_tex;
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

void main() {
    /*
    float depth = texture(u_depth, inTexCoord).x;
    vec3 normal = texture(u_normalRoughness, inTexCoord).xyz;

    vec3 position = reconstructPosFromDepth(inverse(u_vp.projection * u_vp.view), inTexCoord, depth);

    vec3 reflected = normalize(reflect(normalize(position - u_vp.cameraPos), normal));

    //Preparing for ray marching
    vec3 hitPos = position;

    //Ray marching
    vec3 coords = RayMarch(reflected * max(minRayStep, -depth), hitPos);
    */

    vec3 hdrColor = texture(u_tex, inTexCoord).rgb;
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
