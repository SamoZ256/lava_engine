#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];
    
    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }
    
    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }
    
    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }
    
    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
};

struct VP
{
    float4x4 viewProj;
    int layerIndex;
};

constant spvUnsafeArray<float2, 3> _20 = spvUnsafeArray<float2, 3>({ float2(0.0, 1.0), float2(0.0, -1.0), float2(2.0, 1.0) });

struct main0_out
{
    float2 outTexCoord [[user(locn0)]];
    float4 gl_Position [[position]];
    uint gl_Layer [[render_target_array_index]];
};

vertex main0_out main0(constant VP& u_vp [[buffer(0)]], uint gl_VertexIndex [[vertex_id]])
{
    main0_out out = {};
    out.outTexCoord = _20[int(gl_VertexIndex)];
    out.gl_Position = float4((out.outTexCoord * 2.0) - float2(1.0), 0.0, 1.0);
    out.gl_Layer = uint(u_vp.layerIndex);
    return out;
}

