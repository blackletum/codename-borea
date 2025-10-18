#pragma once

//shader for blurring textures (kinda incomplete and also used for rendering debug quads into screen)

char glsl_gaussianblur_vp[] = R"(
	out vec2 frag_texcoord;

	uniform bool flipped;
	
	void main()
	{
		int vertIndex  = gl_VertexID % 6;

		vec2 uv[6] = vec2[](
		    vec2(1.0, 1.0),
		    vec2(0.0, 1.0),
		    vec2(0.0, 0.0),
		    vec2(0.0, 0.0),
		    vec2(1.0, 0.0),
		    vec2(1.0, 1.0)
		);

		vec2 uv_flipped[6] = vec2[](
		    vec2(1.0, 0.0),
		    vec2(0.0, 0.0),
		    vec2(0.0, 1.0),
		    vec2(0.0, 1.0),
		    vec2(1.0, 1.0),
		    vec2(1.0, 0.0)
		);

		if(!flipped)
			frag_texcoord = uv[vertIndex];
		else
			frag_texcoord = uv_flipped[vertIndex];
	
		gl_Position = vec4(aPosition, 1);
	}

)";

const char glsl_gaussianblur_fp[] = R"(

	uniform sampler2D texture_;
	uniform samplerCube cube_texture_;
	in vec2 frag_texcoord;
	
	uniform bool gaussian_pass;
	uniform bool horizontal;

	uniform bool cubemap;
	uniform int cubemap_layer;
	
	//unused for now
	//float gaussian(float x, float mean, float stddev) {
	//	float a = (x - mean) / stddev;
	//	
	//	return exp(-0.5 * a * a);
	//}
	
	vec3 gaussian_blur(sampler2D _texture, vec2 texcoord)
	{
		float weight[5];
		weight[0] = 0.227027;
		weight[1] =  0.1945946;
		weight[2] =  0.1216216;
		weight[3] =  0.054054;
		weight[4] =  0.016216;
	
		vec2 tex_offset = 1.0 / textureSize(_texture, 0); // gets size of single texel
		vec3 result = texture(_texture, texcoord).rgb * weight[0]; // current fragment's contribution
	
		if(horizontal)
		{
		    for(int i = 1; i < 5; ++i)
		    {
		        result += texture(_texture, texcoord + vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
		        result += texture(_texture, texcoord - vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
		    }
		}
		else
		{
		    for(int i = 1; i < 5; ++i)
		    {
		        result += texture(_texture, texcoord + vec2(0.0, tex_offset.y * i)).rgb * weight[i];
		        result += texture(_texture, texcoord - vec2(0.0, tex_offset.y * i)).rgb * weight[i];
		    }
		}
	
		return result;
	}

	vec3 sample_cubemap(samplerCube _cubetexture, vec2 texcoord)
	{
		const vec3 faceNormals[6] = vec3[6](
			vec3( 1, 0, 0),  // +X
			vec3(-1, 0, 0),  // -X
			vec3( 0, 1, 0),  // +Y
			vec3( 0,-1, 0),  // -Y
			vec3( 0, 0, 1),  // +Z
			vec3( 0, 0,-1)   // -Z
		);
		
		const vec3 faceUps[6] = vec3[6](
			vec3(0,-1, 0), // +X
			vec3(0,-1, 0), // -X
			vec3(0, 0, 1), // +Y
			vec3(0, 0,-1), // -Y
			vec3(0,-1, 0), // +Z
			vec3(0,-1, 0)  // -Z
		);
		
		const vec3 faceRights[6] = vec3[6](
			vec3(0,0,-1),  // +X
			vec3(0,0, 1),  // -X
			vec3(1,0, 0),  // +Y
			vec3(1,0, 0),  // -Y
			vec3(1,0, 0),  // +Z
			vec3(-1,0,0)   // -Z
		);
		
		vec2 uv = texcoord * 2.0 - 1.0;
		vec3 projcoord = normalize(	faceNormals[cubemap_layer] + uv.x * faceRights[cubemap_layer] + uv.y * faceUps[cubemap_layer] );

		return texture(_cubetexture, projcoord).xyz;
	}

	vec3 gaussian_blur_cubemap(samplerCube _cubetexture, vec2 texcoord)
	{
		float weight[5];
		weight[0] = 0.227027;
		weight[1] =  0.1945946;
		weight[2] =  0.1216216;
		weight[3] =  0.054054;
		weight[4] =  0.016216;
	
		vec2 tex_offset = 1.0 / textureSize(_cubetexture, 0); // gets size of single texel
		vec3 result = sample_cubemap(_cubetexture, texcoord) * weight[0]; // current fragment's contribution
	
		if(horizontal)
		{
		    for(int i = 1; i < 5; ++i)
		    {
		        result += sample_cubemap(_cubetexture, texcoord + vec2(tex_offset.x * i, 0.0)) * weight[i];
		        result += sample_cubemap(_cubetexture, texcoord - vec2(tex_offset.x * i, 0.0)) * weight[i];
		    }
		}
		else
		{
		    for(int i = 1; i < 5; ++i)
		    {
		        result += sample_cubemap(_cubetexture, texcoord + vec2(0.0, tex_offset.y * i)) * weight[i];
		        result += sample_cubemap(_cubetexture, texcoord - vec2(0.0, tex_offset.y * i)) * weight[i];
		    }
		}
	
		return result;
	}
	
	void main()
	{
		if(gaussian_pass)
		{
			if(!cubemap)
				gl_FragColor = vec4(gaussian_blur(texture_, frag_texcoord), 1);
			else
				gl_FragColor = vec4(gaussian_blur_cubemap(cube_texture_, frag_texcoord), 1);
		}
		else
			gl_FragColor = texture(texture_, frag_texcoord);
	}

)";