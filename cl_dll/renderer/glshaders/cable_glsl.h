#pragma once

char glsl_cable_vp[] = R"(

	uniform mat4 projviewmatrix;
	uniform vec3 renderorigin;
	
	void main()
	{

		//BoneID = cable width (integer)
		//aNormal = cable point tangent

		vec3 vDir = aPosition - renderorigin;
		vec3 vRight = cross(aNormal, -vDir);
		vRight = normalize(vRight);

		int width = aBoneID;

		if ( (gl_VertexID % 2) == 0)
			width *= -1;

		vec3 vertpos = aPosition + (width * vRight);



		gl_Position = projviewmatrix * vec4(vertpos, 1);
	}



)";

const char glsl_cable_fp[] = R"(

	uniform bool wireframe;

	void main()
	{
		gl_FragColor = mix(vec4(0, 0, 0, 1), vec4(0, 0, 1, 1), float(wireframe));
	}

)";
