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
		vec4 skytex = texture(texture0, frag_texcoord);

		skytex = mix(skytex, vec4(fogcolor, 1), int(skyfog) * 0.4);

		gl_FragColor = skytex;
	}

)";