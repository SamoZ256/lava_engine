#version 460

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec3 aTangent;
//layout (location = 4) in vec3 aBitangent;

//layout (location = 0) out vec3 outPosition;
layout (location = 0) out vec2 outTexCoord;
layout (location = 1) out vec3 outNormal;
//layout (location = 2) out mat3 outTBN;

layout (set = 0, binding = 0) uniform VP {
    mat4 viewProj;
    //vec3 cameraPos;
} u_vp;

layout (push_constant) uniform MODEL {
    mat4 model;
    mat4 normalMatrix;
} u_model;

void main() {
    vec4 gloablPos = u_model.model * vec4(aPosition, 1.0);
    gl_Position = u_vp.viewProj * gloablPos;
    //gl_Position.z *= gl_Position.w;

    //outPosition = gloablPos.xyz;
    outTexCoord = aTexCoord;
    //outNormal = normalize(mat3(u_model.normalMatrix) * aNormal);
    //mat3 normalMatrix = transpose(inverse(mat3(u_model.model)));
    vec3 T = normalize(mat3(u_model.normalMatrix) * aTangent);
    /*
    vec3 N = normalize(mat3(u_model.normalMatrix) * aNormal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    //vec3 B = normalize(mat3(u_model.normalMatrix) * aBitangent);

    outTBN = mat3(T, B, N);
    */
    outNormal = normalize(mat3(u_model.normalMatrix) * aNormal);
    //outDepth = gl_Position.z / gl_Position.w;
}
