#pragma once

const char* mirror_vertex =
	R"(

	uniform vec3 renderorigin;

	uniform mat4 projectionMatrix;

	uniform mat4 modelMatrix;

	uniform mat4 viewMatrix;

	out vec4 texcoord0;

	void main() {

		vec4 vertpos = projectionMatrix * viewMatrix * modelMatrix * vec4(aPosition, 1);

		gl_Position = vertpos;

		texcoord0 = vertpos;

	}
	
	)";

const char* mirror_fragment =
	R"(
	
	uniform sampler2D texture0;  // mirror reflection texture
	
	in vec4 texcoord0;


	void main()
	{
		vec2 reflectionCoord = vec2(-texcoord0.x, texcoord0.y) / texcoord0.w;
		reflectionCoord *= 0.5;
		reflectionCoord += 0.5;

		vec4 reflectionPixel = texture2D(texture0, reflectionCoord);

		gl_FragColor = reflectionPixel;
		//gl_FragColor = vec4(1, 0, 0, 1);
	}
	
	)";