#pragma once

const char* water_depth_vertex =
	R"(

	uniform vec3 renderorigin;

	uniform mat4 projviewmodelmatrix;

	uniform int underwater;

	out vec4 texcoord0;
	out vec4 texcoord1;
	out vec3 texcoord2;
	out vec3 vectorToCamera;

	vec4 R1;

	void main() {

		vec4 vertpos = projviewmodelmatrix * vec4(aPosition, 1);

		gl_Position = vertpos;

		texcoord0.xy = aTexCoord / 128;

		//todo: use surface texture scale or func_water keyvalue
		//water_texcoord = texcoord0.xy * texture3_scale;

		texcoord1 = vertpos;

		texcoord2.xyz = aPosition;
		
		vectorToCamera = renderorigin - texcoord2.xyz;

	}
	
	)";

const char* water_fragment_water_regular =
	R"(

	uniform vec3 renderorigin;   // program.local[0] = (x, y, z)
	uniform vec3 waterfog;       // program.local[1] = (r, g, b)
	uniform float fogstart, fogend;
	uniform float m_flFresnelTerm; // program.local[2] = float
	uniform float flTime;        // program.local[3] = client time
	
	uniform sampler2D texture0;  // normal texture
	uniform sampler2D texture1;  // refraction texture
	uniform sampler2D texture2;  // reflection texture
	uniform sampler2D texture3;  // original water texture

	uniform float normalscale;
	uniform float watertex_scale;
	uniform float refraction_scale;
	uniform float reflection_scale;

	uniform int underwater;
	
	in vec4 texcoord0;
	in vec4 texcoord1;
	in vec3 texcoord2;
	in vec3 vectorToCamera;
	
	float GetFogFactor()
	{
		float dist = length(vectorToCamera);

		float fogFactor = (fogend - dist) / (fogend - fogstart);
		fogFactor = clamp(fogFactor, 0.0, 1.0);
		
		return fogFactor;
	}

	void main()
	{
		vec2 normal_offset1 = texcoord0.xy + flTime * vec2(0.2, 0.15);
		vec2 normal_offset2 = texcoord0.xy + vec2(0.11 * flTime, 0.13 * -flTime);
		vec2 normal_offset3 = texcoord0.xy + vec2(0.17 * flTime, 0.14 * flTime);
		vec2 normal_offset4 = texcoord0.xy + vec2(-0.16 * flTime, 0.14 * flTime);
		
		vec3 normalpixel1 = texture(texture0, normal_offset1).xyz;
		vec3 normalpixel2 = texture(texture0, normal_offset2).xyz;
		vec3 normalpixel3 = texture(texture0, normal_offset3).xyz;
		vec3 normalpixel4 = texture(texture0, normal_offset4).xyz;

		vec3 combinedNormal = (normalpixel1 + normalpixel2 + normalpixel3 + normalpixel4) * 0.5;
		combinedNormal -= 1.0;
		

		float normalLen = length(combinedNormal);
		vec2 finalNormal = combinedNormal.xy / normalLen;
		finalNormal *= normalscale;

		float depthFactor = clamp(-finalNormal.y * 4, 0.0, 1.0);


		vec2 refractionCoord = texcoord1.xy / texcoord1.w;
		refractionCoord *= 0.5;
		refractionCoord += 0.5;
		refractionCoord += finalNormal;

		vec2 reflectionCoord = vec2(texcoord1.x, -texcoord1.y) / texcoord1.w;
		reflectionCoord *= 0.5;
		reflectionCoord += 0.5;
		reflectionCoord += finalNormal;

		vec2 basetex_texcoord = texcoord0.xy;
		basetex_texcoord += finalNormal;

		vec4 refractionPixel = texture2D(texture1, refractionCoord);
		vec4 reflectionPixel = texture2D(texture2, reflectionCoord);

		refractionPixel.rgb *= exp(-depthFactor * 4);
		reflectionPixel.rgb *= exp(-depthFactor * 4);
		
		float refractiveFactor = dot( normalize(vectorToCamera), vec3(0.0, 0.0, 1.0) );
		refractiveFactor = clamp(refractiveFactor * m_flFresnelTerm, 0.0, 1.0);

		if(underwater > 0)
			refractiveFactor = 1.0; //dont reflect underwater

		vec4 water_result = mix(reflectionPixel, refractionPixel, refractiveFactor);
		vec4 base_watertex =  texture2D(texture3, basetex_texcoord);

		vec4 endresult = mix(water_result, base_watertex, refractiveFactor * clamp(watertex_scale, 0.0, 1.0));

		if(underwater > 0)
		{
			endresult = mix(endresult, vec4(waterfog, 1), 1.0 - GetFogFactor() );
		}

		gl_FragColor = endresult;
	}
	
	)";