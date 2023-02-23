#version 460
layout(local_size_x = 64, local_size_y = 64, local_size_z = 1) in;

layout(binding = 0, r32f) uniform readonly image2D u_inDepthText;
layout(binding = 1, r16_snorm) uniform writeonly image2D u_outDepthTex;

void main()
{
    float inDepth = imageLoad(u_inDepthText, ivec2(gl_GlobalInvocationID.xy)).x;
    imageStore(u_outDepthTex, ivec2(gl_GlobalInvocationID.xy), vec4(inDepth, 0.0, 0.0, 0.0));
}

