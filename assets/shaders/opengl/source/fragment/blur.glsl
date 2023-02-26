#version 410

uniform sampler2D u_tex;

layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out float FragColor;

void main()
{
    int n = 0;
    vec2 texelSize = vec2(1.0) / vec2(textureSize(u_tex, 0));
    float initValue = texture(u_tex, v_texCoord).x;
    float result = initValue;
    for (int x = -2; x <= 2; x++)
    {
        for (int y = -2; y <= 2; y++)
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            float value = texture(u_tex, v_texCoord + offset).x;
            result += value;
            n++;
        }
    }
    FragColor = result / float(n);
}

