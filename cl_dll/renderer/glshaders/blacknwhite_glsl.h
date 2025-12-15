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

	in vec2 frag_texcoord;

	void main()
	{
		vec3 pixelrgb = texture(texture0, frag_texcoord).rgb;
		float average = (pixelrgb.r + pixelrgb.g + pixelrgb.b) / 3;
		gl_FragColor = vec4(average, average, average, 1.0);
	}

)";