#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main0_out
{
    float FragColor [[color(0)]];
};

struct main0_in
{
    float2 inTexCoord [[user(locn0)]];
};

fragment main0_out main0(main0_in in [[stage_in]], texture2d<float> u_tex [[texture(0)]], sampler u_texSmplr [[sampler(0)]])
{
    main0_out out = {};
    int n = 0;
    float2 texelSize = float2(1.0) / float2(int2(u_tex.get_width(), u_tex.get_height()));
    float initValue = u_tex.sample(u_texSmplr, in.inTexCoord).x;
    float result = initValue;
    for (int x = -2; x <= 2; x++)
    {
        for (int y = -2; y <= 2; y++)
        {
            float2 offset = float2(float(x), float(y)) * texelSize;
            float value = u_tex.sample(u_texSmplr, (in.inTexCoord + offset)).x;
            result += value;
            n++;
        }
    }
    out.FragColor = result / float(n);
    return out;
}

