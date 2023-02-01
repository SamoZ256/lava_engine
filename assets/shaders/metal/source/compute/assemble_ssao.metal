#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

constant uint3 gl_WorkGroupSize [[maybe_unused]] = uint3(16u, 16u, 1u);

kernel void main0(texture2d<float> u_inSsaoText [[texture(0)]], texture2d<float, access::write> u_outSsaoText [[texture(1)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    float inSsao = u_inSsaoText.read(uint2(int2(gl_GlobalInvocationID.xy))).x;
    u_outSsaoText.write(float4(inSsao, 0.0, 0.0, 0.0), uint2(int2(gl_GlobalInvocationID.xy)));
}

