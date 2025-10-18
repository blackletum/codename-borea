#pragma once


char glsl330_studiomdlsolid_vert[] = R"(

	uniform vec2 texturescale;

	layout(std140) uniform StudioSolidUBO
	{
		mat4 projviewmatrix;
		mat4 modelmatrix;
		vec4 light_pos; //x, y, z, light_radius
		ivec4 int_values; //x represents if we're rendering a static model or not

		mat3x4 bonematrixes[128];
	};

	out vec2 texcoord;
	out vec3 fragPos;

	vec3 translated_vertpos;
	
	vec3 VectorTransform(vec3 point, mat3x4 matrix){

	    return vec3( 
					dot(point, matrix[0].xyz) + matrix[0].w, 
					dot(point, matrix[1].xyz) + matrix[1].w, 
					dot(point, matrix[2].xyz) + matrix[2].w
					);
	}

	vec3 VectorRotate(vec3 point, mat3x4 matrix){

	    return vec3( 
					dot(point, matrix[0].xyz), 
					dot(point, matrix[1].xyz), 
					dot(point, matrix[2].xyz)
					);
	}

	void main() {

		int vertbone = (aBoneID) & 255;

		if(int_values.x == 0)
			translated_vertpos = VectorTransform(aPosition, bonematrixes[vertbone]);
		else
			translated_vertpos = vec4( modelmatrix * vec4(aPosition, 1)).xyz; //static prop

		texcoord = aTexCoord.xy / texturescale;

		gl_Position = projviewmatrix * vec4(translated_vertpos, 1);

		fragPos = vec4(translated_vertpos, 1).xyz;
	}



)";

char glsl330_studiomdlsolid_frag[] = R"(

	uniform sampler2D texture0;
	uniform bool bSunShadowMapPass;
	uniform bool alphatest;

	layout(std140) uniform StudioSolidUBO
	{
		//6224 bytes (atleast without padding)
		mat4 projviewmatrix;
		mat4 modelmatrix;
		vec4 light_pos; //x, y, z, light_radius
		ivec4 int_values; //x represents if we're rendering a static model or not

		mat3x4 bonematrixes[128];
	};

	in vec3 fragPos;
	in vec2 texcoord;

	void main()
	{
		if(alphatest)
			if(texture(texture0, texcoord).a < 0.5)
				discard;

		float depthvalue = length(fragPos - light_pos.xyz) / light_pos.w;
		gl_FragColor = vec4(depthvalue, depthvalue * depthvalue, 0, 1);

		if(bSunShadowMapPass)
			gl_FragDepth = 1.0 - gl_FragCoord.z;
	}

)";