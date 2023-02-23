#version 460
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 0, r32f) uniform readonly image2D u_inSsaoText;
layout(binding = 1, r32f) uniform writeonly image2D u_outSsaoText;

void main()
{
    float inSsao = imageLoad(u_inSsaoText, ivec2(gl_GlobalInvocationID.xy)).x;
    imageStore(u_outSsaoText, ivec2(gl_GlobalInvocationID.xy), vec4(inSsao, 0.0, 0.0, 0.0));
}

