//Position reconstruction
vec3 reconstructPosFromDepth(in mat4 invViewProj, in vec2 texCoord, in float depth) {
    vec3 posInViewProj = vec3(texCoord.x * 2.0 - 1.0, (1.0 - texCoord.y) * 2.0 - 1.0, depth);
    vec4 position = invViewProj * vec4(posInViewProj, 1.0);
    position.xyz /= position.w;

    return position.xyz;
}
