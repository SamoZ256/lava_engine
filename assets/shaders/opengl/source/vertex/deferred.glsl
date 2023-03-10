#version 410

layout(std140) uniform VP
{
    mat4 viewProj;
} u_vp;

layout(std140) uniform MODEL
{
    mat4 model;
    mat4 normalMatrix;
} u_model;

layout(location = 0) in vec3 aPosition;
layout(location = 0) out vec2 v_texCoord;
layout(location = 1) in vec2 aTexCoord;
layout(location = 3) in vec4 aTangent;
layout(location = 2) in vec3 aNormal;
layout(location = 1) out mat3 v_TBN;

void main()
{
    gl_Position = (u_vp.viewProj * u_model.model) * vec4(aPosition, 1.0);
    v_texCoord = aTexCoord;
    vec3 T = normalize(mat3(u_model.normalMatrix[0].xyz, u_model.normalMatrix[1].xyz, u_model.normalMatrix[2].xyz) * aTangent.xyz);
    vec3 N = normalize(mat3(u_model.normalMatrix[0].xyz, u_model.normalMatrix[1].xyz, u_model.normalMatrix[2].xyz) * aNormal);
    T = normalize(T - (N * dot(T, N)));
    vec3 B = cross(N, T) * aTangent.w;
    v_TBN = mat3(vec3(T), vec3(B), vec3(N));
}

