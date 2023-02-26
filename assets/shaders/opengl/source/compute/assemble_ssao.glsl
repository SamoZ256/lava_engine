#version 410
#extension GL_ARB_compute_shader : require
#extension GL_ARB_shader_image_load_store : require
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(r32f) uniform readonly image2D u_inSsaoText;
layout(r32f) uniform writeonly image2D u_outSsaoText;

void main()
{
    float inSsao = imageLoad(u_inSsaoText, ivec2(gl_GlobalInvocationID.xy)).x;
    imageStore(u_outSsaoText, ivec2(gl_GlobalInvocationID.xy), vec4(inSsao, 0.0, 0.0, 0.0));
}

