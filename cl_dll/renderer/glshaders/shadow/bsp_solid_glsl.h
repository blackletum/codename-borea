#pragma once

//
//shader used to render brush models into a shadowmap framebuffer
//

const char glsl330_worldsolid_vp[] = R"(

	uniform mat4 projviewmatrix;
	uniform vec4 light_pos; //x, y, z, light_radius

	uniform mat4 modelmatrix; //this gets updated frequently so just use gluniform4fmatrix

	out vec2 texcoord;
	out vec3 fragPos;

	void main() {

		gl_Position = projviewmatrix * modelmatrix * vec4(aPosition, 1);

		fragPos = vec4(modelmatrix * vec4(aPosition, 1)).xyz;
		texcoord = aTexCoord;
	}

)";

const char glsl330_worldsolid_fp[] = R"(

	in vec2 texcoord;
	in vec3 fragPos;

	uniform sampler2D base_texture;

	uniform mat4 projviewmatrix;
	uniform vec4 light_pos; //x, y, z, light_radius
	uniform bool alphatest;

	void main() {

		if(alphatest) //i would leave it hard coded but for some reason the fucking entire shadowmap gets filled with holes after some map reloads even if i bind the surface texture unit to  texture id 0 ??????? what the fuck ?
		{
			if(texture(base_texture, texcoord).a < 0.5)
				discard;
		}

		float depthvalue = length(fragPos - light_pos.xyz) / light_pos.w;
		gl_FragColor = vec4(depthvalue, depthvalue * depthvalue, 0, 1);

	}

)";