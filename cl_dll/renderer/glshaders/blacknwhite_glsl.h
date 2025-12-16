#pragma once

char glsl_blacknwhite_vp[] = R"(
	out vec2 frag_texcoord;

	uniform bool flipped;
	
	void main()
	{
		int vertIndex  = gl_VertexID % 6;

		vec2 uv[6] = vec2[](
		    vec2(1.0, 1.0),
		    vec2(0.0, 1.0),
		    vec2(0.0, 0.0),
		    vec2(0.0, 0.0),
		    vec2(1.0, 0.0),
		    vec2(1.0, 1.0)
		);

		vec2 uv_flipped[6] = vec2[](
		    vec2(1.0, 0.0),
		    vec2(0.0, 0.0),
		    vec2(0.0, 1.0),
		    vec2(0.0, 1.0),
		    vec2(1.0, 1.0),
		    vec2(1.0, 0.0)
		);

		if(!flipped)
			frag_texcoord = uv[vertIndex];
		else
			frag_texcoord = uv_flipped[vertIndex];
	
		gl_Position = vec4(aPosition, 1);
	}

)";

const char glsl_blacknwhite_fp[] = R"(

	uniform sampler2D texture0;
	uniform float time;
	uniform int grain_strength;

	in vec2 frag_texcoord;

	float rand(vec2 co){
	    return fract(sin(dot(co.xy ,vec2(12.9898, 78.233))) * 43758.5453);
	}

	float filmgrain_test(vec2 offset)
	{
		vec2 seed = vec2(rand(offset), rand(offset));

		float x = (seed.x / 3.14159 + 4) * (seed.y / 5.49382 + 4) * ((fract(time) + 1));
		return mod((mod(x, 13) + 1) * (mod(x, 123) + 1), 0.01) - 0.005;
	}

	void main()
	{
		vec3 pixelrgb = texture(texture0, frag_texcoord).rgb;
		float average = (pixelrgb.r + pixelrgb.g + pixelrgb.b) / 3;
		average += (filmgrain_test(frag_texcoord * 6) * grain_strength);
		gl_FragColor = vec4(average, average, average, 1.0);
	}

)";