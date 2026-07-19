#pragma once

char glsl_debug_vp[] = R"(

	uniform mat4 projviewmatrix;

	out vec4 frag_color;
	
	void main()
	{
		frag_color = aColor;
		gl_Position = projviewmatrix * vec4(aPosition, 1);
	}



)";

const char glsl_debug_fp[] = R"(

	in vec4 frag_color;

	void main()
	{
		gl_FragColor = frag_color;
	}

)";