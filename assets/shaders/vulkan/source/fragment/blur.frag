#version 450

//Macros
#define BLUR_RANGE 2

layout (location = 0) out float FragColor;

layout (location = 0) in vec2 v_texCoord;

layout (set = 0, binding = 0) uniform sampler2D u_tex;
//layout (set = 0, binding = 1) uniform sampler2D u_positionDepth;

void main()  {
	int n = 0;
	vec2 texelSize = 1.0 / vec2(textureSize(u_tex, 0));
    //float depth = texture(u_positionDepth, v_texCoord).w;
    float initValue = texture(u_tex, v_texCoord).r;
	float result = initValue;
	for (int x = -BLUR_RANGE; x <= BLUR_RANGE; x++) {
		for (int y = -BLUR_RANGE; y <= BLUR_RANGE; y++) {
			vec2 offset = vec2(x, y) * texelSize;
            //float rangeCheck = smoothstep(0.0, 1.0, 1.0 / abs(depth - texture(u_positionDepth, v_texCoord + offset).w));
            float value = texture(u_tex, v_texCoord + offset).r;
            //if (abs(value - initValue) < 0.5)
			    result += value;
			n++;
		}
	}
	
	FragColor = result / (float(n));
}
