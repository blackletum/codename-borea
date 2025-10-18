#pragma once

char glsl_sprite_vp[] = R"(

	uniform mat4 projviewmatrix;

	out vec2 frag_texcoord;
	out vec4 frag_color;
	
	void main()
	{
		
		//calculate texture coordinates dynamically, since all goldsrc sprite entities stretch the texture to fit inside the quad
		int vertIndex  = gl_VertexID % 4; //example: |60 = 0| |61 = 1| |62 = 2| |63 = 3| |64 = 0|

		vec2 uv[4] = vec2[](
		    vec2(0.0, 1.0),
		    vec2(0.0, 0.0),
		    vec2(1.0, 0.0),
		    vec2(1.0, 1.0)
		);

		frag_texcoord = uv[vertIndex];
		frag_color = aColor;
		gl_Position = projviewmatrix * vec4(aPosition, 1);
	}



)";

const char glsl_sprite_fp[] = R"(

	uniform sampler2D texture0;

	in vec2 frag_texcoord;
	in vec4 frag_color;

	void main()
	{
		vec4 tex = texture(texture0, frag_texcoord);

		if(tex.a < 0.5)
			discard;

		gl_FragColor = tex * frag_color;
	}

)";