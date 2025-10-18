#pragma once

char glsl_particle_vp[] = R"(

	uniform mat4 projviewmatrix;

	out vec2 frag_texcoord;
	out vec4 frag_color;
	
	void main()
	{
		frag_texcoord = aTexCoord;
		frag_color = aColor;
		gl_Position = projviewmatrix * vec4(aPosition, 1);
	}



)";

const char glsl_particle_fp[] = R"(

	uniform sampler2D texture0;

	in vec2 frag_texcoord;
	in vec4 frag_color;

	void main()
	{
		gl_FragColor = texture(texture0, frag_texcoord) * frag_color;
	}

)";