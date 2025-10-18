#pragma once

//simple shader for decals (should probably make one definite shader for simple quad rendering like this)

char glsl_decal_vp[] = R"(

	uniform mat4 projviewmatrix;

	out vec2 frag_texcoord;
	out vec4 frag_color;
	
	void main()
	{
		frag_texcoord = aTexCoord;
		gl_Position = projviewmatrix * vec4(aPosition, 1);
	}



)";

const char glsl_decal_fp[] = R"(

	uniform sampler2D texture0;
	uniform bool wireframe;

	in vec2 frag_texcoord;

	void main()
	{
		if(wireframe)
		{
			gl_FragColor = vec4(0, 1, 0, 1); //green
			return;
		}
		vec4 texture = texture(texture0, frag_texcoord);
		if(texture.a < 0.1)
			discard;

		gl_FragColor = texture;
	}

)";