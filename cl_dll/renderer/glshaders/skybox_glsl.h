#pragma once

//simple shader for rendering the skybox cube

char glsl_skybox_vp[] = R"(

	uniform mat4 projviewmatrix;

	out vec2 frag_texcoord;
	
	void main()
	{
		frag_texcoord = aTexCoord;

		gl_Position = vec4(projviewmatrix * vec4(aPosition, 1)).xyww;
	}



)";

const char glsl_skybox_fp[] = R"(

	uniform sampler2D texture0;
	uniform bool skyfog;
	uniform vec3 fogcolor;

	in vec2 frag_texcoord;

	void main()
	{
		vec3 skytex = mix(texture(texture0, frag_texcoord).rgb, fogcolor, int(skyfog) * 0.4);

		gl_FragColor = vec4(skytex, 1);
	}

)";