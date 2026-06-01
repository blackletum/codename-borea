#pragma once

//main world(brush) shader

char glsl330_world_vp[] = R"(

	vec3 vert_position;


	uniform mat4 projectionmatrix;
	uniform mat4 viewmatrix;
	uniform mat4 modelmatrix;

	uniform mat4 spotlight_texturematrix;

	uniform vec3 renderorigin;
	uniform vec3 renderforward;
	uniform vec3 renderright;

	uniform int renderamt; // 0 -- 255
	uniform ivec3 rendercolor; //0 -- 255

	uniform bool shadow;
	uniform bool spotlight;
	uniform bool wireframe;

	uniform float fltime;
	uniform bool waterpolys;
	uniform bool scrollingpolys;
	uniform bool specular;

	uniform vec4 clipplane;


	out vec2 frag_texcoord_lightmap;
	out vec2 frag_texcoord_texture;
	out vec4 projTexCoord;
	out vec3 fragPos;
	out vec3 fragNormal;


	void vert_HandleWireframe()
	{
		gl_Position = projectionmatrix * viewmatrix * modelmatrix * vec4(vert_position, 1);
	}

	float turbsin(int index)
	{
		//	turbsin is simply a lookup table with 
		// 256 values such that turbsin[n] = 8 * sin( 2 * Pi * n/256).

		return 8.0 * sin( M_PI2 * (index & 255) / 256);
	}

	void vert_MakeWaterCoords()
	{
		float os = aTexCoord.x;
		float ot = aTexCoord.y;
		
		float s = os + turbsin(	int( (ot * 0.125 + fltime) * 40 ) );
		float t = ot + turbsin(	int( (os * 0.125 + fltime) * 40 ) );

		float ssin = turbsin(	int( (os * 0.125 + (fltime * 3.2) ) * 40) );
		float tsin = turbsin(	int( (ot * 0.125 + (fltime * 3.2) ) * 40) );

		float height = (ssin - tsin) * 0.2; // replace 0.2 with entity scale in the future

		frag_texcoord_texture = vec2(s * (1.0 / 64), t * (1.0 / 64));
		
		vert_position = vec3(aPosition.x, aPosition.y, aPosition.z + height);
	}

	void vert_HandleScrollingPolyUV()
	{
		float speed = float(rendercolor.b);
		speed += float(rendercolor.g << 8);
		speed *= fltime * 0.0625 * 0.007325;

		if (rendercolor.r == 0)
			speed *= -1;

		frag_texcoord_texture = vec2(aTexCoord.x + speed, aTexCoord.y);
	}

	void vert_HandleSpecularPolyUV()
	{
		if (aNormal.x == 1) //PLANE_X
		{
			frag_texcoord_texture.x = aTexCoordSpecular.x - (renderorigin.y * aTexCoord.x);
			frag_texcoord_texture.y = aTexCoordSpecular.y + (renderorigin.z * aTexCoord.y);
		}
		else if (aNormal.y == 1) //PLANE_Y
		{
			frag_texcoord_texture.x = aTexCoordSpecular.x - (renderorigin.x * aTexCoord.x);
			frag_texcoord_texture.y = aTexCoordSpecular.y + (renderorigin.z * aTexCoord.y);
		}
		else if (aNormal.z == 1) //PLANE_Z
		{
			frag_texcoord_texture.x = aTexCoordSpecular.x - (renderorigin.x * aTexCoord.x);
			frag_texcoord_texture.y = aTexCoordSpecular.y + (renderorigin.y * aTexCoord.y);
		}
		else
		{
			if (aNormal.z == 0) //vertical walls with angle other then 0 or 45 degrees...
			{
				if (aNormal.x == aNormal.y) //45 degree matching plane
				{
					frag_texcoord_texture.x = aTexCoordSpecular.x + (renderorigin.y * ((-aNormal.y) * aTexCoord.x)) - (renderorigin.x * ((-aNormal.x) * aTexCoord.x));
				}
				else if ( abs(aNormal.x) == abs(aNormal.y) ) //45 degree matching plane but ones negative
				{
					frag_texcoord_texture.x = aTexCoordSpecular.x + (renderorigin.y * ((aNormal.y) * aTexCoord.x)) - (renderorigin.x * ((aNormal.x) * aTexCoord.x));
				}
				else
				{
					if (abs(aNormal.y) > abs(aNormal.x))
						frag_texcoord_texture.x = aTexCoordSpecular.x - (renderorigin.x * aTexCoord.x);
					else
						frag_texcoord_texture.x = aTexCoordSpecular.x - (renderorigin.y * aTexCoord.x);
				}
				frag_texcoord_texture.y = aTexCoordSpecular.y + (renderorigin.z * aTexCoord.y);
			}
			else //slanted walls
			{
				if (abs(aNormal.z) > abs(aNormal.x) && abs(aNormal.z) > abs(aNormal.x)) //slanted wall but less than 45 upright
				{
					frag_texcoord_texture.x = aTexCoordSpecular.x - (renderorigin.x * aTexCoord.x);
					frag_texcoord_texture.y = aTexCoordSpecular.y + (renderorigin.y * aTexCoord.y);

					if (aNormal.x > 0 && aNormal.y > 0)
					{
						frag_texcoord_texture.x += renderorigin.z * (aTexCoord.x * 0.5);
						frag_texcoord_texture.y -= renderorigin.z * (aTexCoord.y * 0.5);
					}
					else if (aNormal.x < 0 && aNormal.y > 0)
					{
						frag_texcoord_texture.x -= renderorigin.z * (aTexCoord.x * 0.5);
						frag_texcoord_texture.y -= renderorigin.z * (aTexCoord.y * 0.5);
					}
					else if (aNormal.x < 0 && aNormal.y < 0)
					{
						frag_texcoord_texture.x -= renderorigin.z * (aTexCoord.x * 0.5);
						frag_texcoord_texture.y += renderorigin.z * (aTexCoord.y * 0.5);
					}
					else if (aNormal.x > 0 && aNormal.y < 0)
					{
						frag_texcoord_texture.x += renderorigin.z * (aTexCoord.x * 0.5);
						frag_texcoord_texture.y += renderorigin.z * (aTexCoord.y * 0.5);
					}
					else if (aNormal.x > 0)
						frag_texcoord_texture.x += renderorigin.z * aTexCoord.x;
					else if (aNormal.x < 0)
						frag_texcoord_texture.x -= renderorigin.z * aTexCoord.x;
					else if (aNormal.y > 0)
						frag_texcoord_texture.y -= renderorigin.z * aTexCoord.y;
					else
						frag_texcoord_texture.y += renderorigin.z * aTexCoord.y;

				}
				else if (abs(aNormal.z) < abs(aNormal.x) && abs(aNormal.z) < abs(aNormal.y))
				{
					frag_texcoord_texture.y = aTexCoordSpecular.y;

					if (aNormal.x > 0 && aNormal.y > 0 && aNormal.z > 0)
					{
						frag_texcoord_texture.x = aTexCoordSpecular.x - (renderorigin.x * aTexCoord.x);

						frag_texcoord_texture.x += renderorigin.y * aTexCoord.x;
						frag_texcoord_texture.x += renderorigin.z * (aTexCoord.x * 0.5);
						frag_texcoord_texture.y += renderorigin.z * aTexCoord.x;
					}
					else if (aNormal.x > 0 && aNormal.y > 0 && aNormal.z < 0)
					{
						frag_texcoord_texture.x = aTexCoordSpecular.x - (renderorigin.y * aTexCoord.x);

						frag_texcoord_texture.x += renderorigin.x * aTexCoord.x;
						frag_texcoord_texture.x -= renderorigin.z * (aTexCoord.x * 0.5);
						frag_texcoord_texture.y += renderorigin.z * aTexCoord.x;
					}
					else
					{
						frag_texcoord_texture.x = aTexCoordSpecular.x - (renderorigin.y * aTexCoord.x);

						frag_texcoord_texture.x -= renderorigin.x * aTexCoord.x;
						frag_texcoord_texture.x -= renderorigin.z * (aTexCoord.x * 0.5);
						frag_texcoord_texture.y += renderorigin.z * aTexCoord.x;
					}
				}
				else if (abs(aNormal.x) > 0)
				{
					frag_texcoord_texture.x = aTexCoordSpecular.x - (renderorigin.y * aTexCoord.x);

					if (aNormal.z > 0)
						frag_texcoord_texture.y = aTexCoordSpecular.y - (renderorigin.x * aTexCoord.x);
					else
						frag_texcoord_texture.y = aTexCoordSpecular.y + (renderorigin.x * aTexCoord.x);
					frag_texcoord_texture.y += renderorigin.z * aTexCoord.x;
				}
				else if (abs(aNormal.y) > 0)
				{
					frag_texcoord_texture.x = aTexCoordSpecular.x - (renderorigin.x * aTexCoord.x);

					if (aNormal.z > 0)
						frag_texcoord_texture.y = aTexCoordSpecular.y - (renderorigin.y * aTexCoord.x);
					else
						frag_texcoord_texture.y = aTexCoordSpecular.y + (renderorigin.y * aTexCoord.x);
					frag_texcoord_texture.y += renderorigin.z * aTexCoord.x;
				}
			}
		}
	}
	
	void main()
	{
		if(waterpolys)
		{
			vert_MakeWaterCoords();
		}
		else if (scrollingpolys)
		{
			vert_HandleScrollingPolyUV();
			vert_position = aPosition;
		}
		else if (specular)
		{
			vert_HandleSpecularPolyUV();
			vert_position = aPosition;
		}
		else
		{
			frag_texcoord_texture = aTexCoord;
			vert_position = aPosition;
		}

		if(wireframe)
		{
			vert_HandleWireframe();
			return;
		}

		frag_texcoord_lightmap = aTexCoordLM;


		gl_Position = projectionmatrix * viewmatrix * modelmatrix * vec4(vert_position, 1);

		vec4 worldPos = modelmatrix * vec4(vert_position, 1);

		if(spotlight || shadow)
			projTexCoord = spotlight_texturematrix * worldPos;

		fragPos = worldPos.xyz;
		fragNormal = normalize( transpose(inverse(mat3(modelmatrix))) * aNormal );

		gl_ClipDistance[0] = dot(modelmatrix * vec4(vert_position, 1), clipplane);
	}



)";

const char glsl330_world_fp[] = R"(

	in vec2 frag_texcoord_lightmap;
	in vec2 frag_texcoord_texture;
	in vec3 frag_normal;
	in vec4 projTexCoord;
	in vec3 fragPos;
	in vec3 fragNormal;

	uniform sampler2D lightmap_texture;
	uniform sampler2D base_texture;
	uniform sampler2D spotlight_texture;
	uniform sampler2D shadow_texture;
	uniform samplerCube cubemap_texture;

	uniform int renderamt; // 0 -- 255
	uniform ivec3 rendercolor; //0 -- 255

	uniform vec3 light_pos;
	uniform vec3 light_color;
	uniform float light_radius;

	uniform int sunshadow_fadedist;
	uniform int sunshadow_strength;
	
	uniform bool lightmap_pass;
	uniform bool texture_pass;
	uniform bool spotlight;
	uniform bool pointlight;
	uniform bool shadow;
	uniform bool onlyshadow;
	uniform bool wireframe;

	uniform bool waterpolys;
	uniform bool scrollingpolys;

	uniform float dt_opacity;

	uniform bool alphatest;

	uniform vec3 renderorigin;
	uniform vec3 renderforward;

	uniform vec3 sunDir;

	uniform vec3 fogcolor;
	uniform float fogstart;
	uniform float fogend;
	uniform bool fog_active;

	uniform float lightgamma;
	uniform float texgamma;

	float sampleShadowVariance(sampler2D shadowMap, vec2 projCoord)
	{
		float linearfragdepth = length(fragPos - light_pos) / light_radius;

		vec2 moments = texture(shadowMap, projCoord).xy;
		
		float p = step(linearfragdepth, moments.x); 
		float variance = max(moments.y - sqr(moments.x), 0.00002);	//(variance = σ² = E[X²]−(E[X])²)
		
		float d = linearfragdepth - moments.x;						//(d "distance" = t = X - μ)
		float pMax = variance  / ( variance + sqr(d) );				//(pMax = ( σ² / (σ² + t²) )
		
		return min(max(p, pMax), 1.0);
	}

	float sampleSunShadow(sampler2D shadowMap, vec2 projCoord)
	{
		
		float linearfragdepth = length(fragPos - light_pos) / light_radius;

		vec2 moments = texture(shadowMap, projCoord).xy;

		float depthDifference = linearfragdepth - moments.x;
		
		float shadow = depthDifference > 0.0 ? 1.0 : 0.0;
		
		float fadeDistance = float(sunshadow_fadedist) / 1000.0;
		float shadowFade = 1.0 - smoothstep(0.0, fadeDistance, abs(depthDifference));
		
		shadow *= shadowFade;
		
		return 1.0 - shadow;
	}

	float sampleSunShadowPCF(sampler2D shadowMap, vec2 projCoord)
	{
		float shadow = 0.0;
		vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
		//5x5 kernel
		for(int x = -2; x <= 2; ++x)
		{
		    for(int y = -2; y <= 2; ++y)
		    {
		        shadow += sampleSunShadow(shadowMap, projCoord + vec2(x, y) * texelSize);        
		    }    
		}
		return max(shadow / 25.0, 1.0 - float(sunshadow_strength) / 100.0);
	}

	float sampleCubeShadowVariance(samplerCube shadowMap, vec3 projCoord)
	{ 

		float linearfragdepth = length(fragPos - light_pos) / light_radius;

		vec2 moments = texture(shadowMap, projCoord).xy;
		
		float bias = 0.002;

		float p = step(linearfragdepth + 0.002, moments.x);
		float variance = max(moments.y - sqr(moments.x), 0.00002);
		
		float d = abs(moments.x - linearfragdepth);
		float pMax = variance  / ( variance + sqr(d) );
		
		return min(max(p, pMax), 1.0);
	}

	void DiscardLightFragment()
	{
		gl_FragColor = onlyshadow ? vec4(1, 1, 1, 1) : vec4(0, 0, 0, 0);
	}

	void frag_HandleSunShadow()
	{
		//sunlight is directional, meaning it suffers no perspective distortion, so just use sun direction (which is hardcoded for now)
		vec3 viewDir = -sunDir;
		float dotprod = dot(fragNormal, viewDir);
		if(dotprod <= 0.0)	//IMPORTANT !!! only render shadows on surfaces that are facing the sun.
		{
			gl_FragColor = vec4(1, 1, 1, 1);
			return;
		}

		vec3 projCoord = projTexCoord.xyz / projTexCoord.w;
		projCoord.xyz *= 0.5;
		projCoord.xyz += 0.5;

		if (projCoord.x < 0.0 || projCoord.x > 1.0 ||
			projCoord.y < 0.0 || projCoord.y > 1.0 ||
			projCoord.z < 0.0 || projCoord.z > 1.0 ) 
		{
			gl_FragColor = vec4(1, 1, 1, 1);
			return;
		}

		float shadowpixel = sampleSunShadowPCF(shadow_texture, projCoord.xy);

		vec4 shadowPixel = vec4(vec3( shadowpixel ), 1);

		gl_FragColor = shadowPixel;

		
	}

	void frag_HandleSpotlight()
	{
		float distance = length(fragPos - light_pos);
		if(distance > light_radius)
		{
			DiscardLightFragment();
			return;
		}

		float dotprod = dot(-fragNormal, fragPos - light_pos);
		if(dotprod <= 0.0)
		{
			DiscardLightFragment();
			return;
		}

		vec3 projCoord = projTexCoord.xyz / projTexCoord.w;
		projCoord.xyz *= 0.5;
		projCoord.xyz += 0.5;

		if (projCoord.x < 0.0 || projCoord.x > 1.0 ||
			projCoord.y < 0.0 || projCoord.y > 1.0 ||
			projCoord.z < 0.0 || projCoord.z > 1.0 ) 
		{
			DiscardLightFragment();
			return;
		}

		vec4 pixel = texture2D(spotlight_texture, projCoord.xy);
		pixel.rgb *= pixel.w;
		if(shadow)
		{
			float shadowPixel = sampleShadowVariance(shadow_texture, projCoord.xy);
			
			pixel.rgb *= vec3(shadowPixel);
		}		

		float attenuation = clamp(1.0 - (distance / light_radius), 0.0, 1.0);
		pixel.rgb *= vec3(attenuation);
		pixel.rgb *= light_color;

		gl_FragColor = pixel;
	}

	void frag_HandleDynLight()
	{
		float dotprod = dot(-fragNormal, fragPos - light_pos);
		if(dotprod <= 0.0)
		{
			DiscardLightFragment();
			return;
		}

		float distance = length(fragPos - light_pos);
		if(distance > light_radius)
		{
			DiscardLightFragment();
			return;
		}
		
		float attenuation = clamp(1.0 - (distance / light_radius), 0.0, 1.0);
		vec3 fragtolight = fragPos - light_pos;

		if(shadow)
		{
			float shadow = sampleCubeShadowVariance(cubemap_texture, fragtolight);

			if(!onlyshadow)
				attenuation *= shadow;
			else
				attenuation = mix(1, shadow, attenuation);
		}
		
		gl_FragColor = onlyshadow ? vec4( vec3(attenuation), 1 ) : vec4(vec3(light_color * attenuation), 1);
	}

	float GetFogFactor()
	{
		float dist = length(renderorigin - fragPos);
		
		return clamp((fogend - dist) / (fogend - fogstart), 0.0, 1.0);
	}

	void frag_HandleWireframe()
	{
		gl_FragColor = vec4(1, 0, 0, 1); //solid red for lines
	}

	void frag_HandleTextureLightMapPass()
	{
		//goldsrc subdivides water surfaces, thus messing up lightmap coords (and texture coords)
		if(waterpolys)
		{
			gl_FragColor = vec4(0.65, 0.65, 0.65, 1);
			return;
		}

		vec4 basetex_pixel = texture2D(base_texture, frag_texcoord_texture);
		if(basetex_pixel.a < 0.5)
			discard;

		basetex_pixel.a *= float(renderamt) / 255;

		vec4 lightmap_pixel = texture2D(lightmap_texture, frag_texcoord_lightmap);

		//gamma
		lightmap_pixel.rgb = pow(lightmap_pixel.rgb, vec3( clamp(1.0 / lightgamma, 0.1, 5) ) );
		basetex_pixel.rgb = pow(basetex_pixel.rgb, vec3( clamp(1.0 / texgamma, 0.1, 5) ) );

		//fog
		if(fog_active)
		{
			lightmap_pixel.rgb = mix( lightmap_pixel.rgb, fogcolor, 1.0 - GetFogFactor() );
			basetex_pixel.rgb = mix(basetex_pixel.rgb, fogcolor, 1.0 - GetFogFactor() );
		}

		gl_FragColor = (basetex_pixel * lightmap_pixel) * 2;
		gl_FragColor.a = basetex_pixel.a;

	}

	void frag_HandleTexturePass()
	{
		vec4 basetex_pixel = texture2D(base_texture, frag_texcoord_texture); 

		//alpha test
		if(basetex_pixel.a < 0.5)
			discard;

		//gamma
		basetex_pixel.rgb = pow(basetex_pixel.rgb, vec3( clamp(1.0 / texgamma, 0.1, 5) ) );

		//entity render vars
		if(!scrollingpolys)
		{
			basetex_pixel.rgb *= vec3(rendercolor) / 255;
		}

		basetex_pixel.a *= float(renderamt) / 255;

		//fog
		if(fog_active)
		{
			basetex_pixel.rgb = mix( basetex_pixel.rgb, fogcolor, 1.0 - GetFogFactor() );
		}

		gl_FragColor = basetex_pixel;
		gl_FragColor.a = basetex_pixel.a;

	}

	void frag_HandleLightmapPass()
	{
		//goldsrc subdivides water surfaces, thus messing up lightmap coords (and texture coords)
		if(waterpolys)
		{
			gl_FragColor = vec4(0.65, 0.65, 0.65, 1);
			return;
		}
		if(alphatest)
		{
			vec4 basetex_pixel = texture2D(base_texture, frag_texcoord_texture);
			if(basetex_pixel.a < 0.5)
				discard;
		}

		vec4 lightmap_pixel = texture2D(lightmap_texture, frag_texcoord_lightmap);

		//gamma
		lightmap_pixel.rgb = pow(lightmap_pixel.rgb, vec3( clamp(1.0 / lightgamma, 0.1, 5) ) );

		//fog
		if(fog_active)
		{
			lightmap_pixel.rgb = mix( lightmap_pixel.rgb, fogcolor, 1.0 - GetFogFactor() );
		}

		gl_FragColor = vec4(lightmap_pixel.rgb, lightmap_pixel.a * float(renderamt) / 255);
	}

	void main()
	{

		if			(wireframe)						frag_HandleWireframe();
		else if		(spotlight)						frag_HandleSpotlight();
		else if		(pointlight)					frag_HandleDynLight();
		else if		(shadow)						frag_HandleSunShadow();
		else if		(texture_pass && lightmap_pass)	frag_HandleTextureLightMapPass(); //this result is darker than with opengl pipeline multiplication for some reason (glblendfunc(gl_src_color, gl_dst_color);
		else if		(texture_pass)					frag_HandleTexturePass();
		else if	(lightmap_pass)						frag_HandleLightmapPass();
	}

)";