#pragma once


char glsl330_studiomdl_vert[] = R"(

	#define MAX_MODEL_LIGHTS 12 // 2x(up, down, left, right, front, back)

	// lighting options
	#define STUDIO_NF_FLATSHADE 1
	#define STUDIO_NF_CHROME 2
	#define STUDIO_NF_ADDITIVE 32  // buz
	#define STUDIO_NF_ALPHATEST 64 // buz
	#define STUDIO_NF_NOMIPMAP 256
	#define STUDIO_NF_FULLBRIGHT 512



	//for gpu skinning
	layout(std140) uniform BonesUBO
	{								
		//6144 bytes
		mat3x4 bonematrixes[128];
	};

	layout(std140) uniform studiomdl_PerFrame
	{	
		mat4 projviewmatrix;
		mat4 VMprojviewmatrix;
		vec4 fogcolor_n_fogstart; //w = fogstart
		vec4 fogend_n_fogactive_n_lightdebug; //x = fogend, y = fogactive, z = light debug cvar
	
		vec4 renderorigin;
		vec4 renderright;
	};

	#define _NUMLIGHTS 0
	#define _CHROMESHELL_BOOL 1
	#define _ISSTATIC_BOOL 2

	layout(std140) uniform studiomdl_PerEntity
	{
		vec4 lightdir; // 16 bytes
		vec4 ambientlight; //16 bytes
		vec4 diffuselight; //16 bytes
	
		mat4 modelmatrix; //64 bytes
	
		ivec4 int_values; //x = numlights; y = chromeshell boolean; z = is this entity is static (prop_static) or not |||| 16 bytes

		vec4 rendervalues; //rendercolor.r, rendercolor.g, rendercolor.b, renderamt
		
		mat3x4 modellight_info[MAX_MODEL_LIGHTS]; // 1536 bytes

		//modellight_info[i][0] = modellight_origin[i]
		//modellight_info[i][1] = modellight_color[i]
		//modellight_info[i][2] = modellight_forward[i]
	};

	uniform sampler2D texture0;
	uniform int texture_flags;
	uniform bool wireframe;
	uniform bool viewmodel;

	uniform bool studiodecal;
	uniform vec2 decalsize;

	
	out vec4 vertexdiffusecolor;
	out vec4 vertexspecularcolor;
	out vec4 projectedCoord;
	out vec2 texcoord;
	out vec3 fragPos;

	vec3 translated_vertpos;
	vec3 translated_normal;
	
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

	vec3 VectorIRotate(vec3 point, mat3x4 matrix){

	    return vec3( 
					dot(point, vec3(matrix[0][0], matrix[1][0], matrix[2][0])), 
					dot(point, vec3(matrix[0][1], matrix[1][1], matrix[2][1])), 
					dot(point, vec3(matrix[0][2], matrix[1][2], matrix[2][2]))
					);
	}

	void CalcGLPosition()
	{
		mat4 pvmatrix = mat4(1.0);
		if(!viewmodel)
			pvmatrix = projviewmatrix;
		else
			pvmatrix = VMprojviewmatrix;

		gl_Position = pvmatrix * vec4(translated_vertpos, 1);
	}

	vec2 ChromeTexCoords(vec3 norm, mat3x4 matrix)
	{
		vec3 tmp = renderorigin.xyz * -1;
		tmp.x += matrix[0].w;
		tmp.y += matrix[1].w;
		tmp.z += matrix[2].w;

		tmp = normalize(tmp);
		vec3 chromeupvec = cross(tmp, renderright.xyz);
		vec3 chromerightvec = cross(tmp, chromeupvec);

		vec3 vChromeUp = VectorIRotate(chromeupvec, matrix);
		vec3 vChromeRight = VectorIRotate(chromerightvec, matrix);
	
		return vec2((dot(norm, vChromeRight) + 1.0) * 0.5, (dot(norm, vChromeUp) + 1.0) * 0.5);

	}

	void CalcTexCoords(int normboneid)
	{
		if ( (texture_flags & STUDIO_NF_CHROME) > 0)
			texcoord = ChromeTexCoords(aNormal, bonematrixes[normboneid]);
		else if(studiodecal)
			texcoord = aTexCoord.xy / decalsize;
		else
			texcoord = aTexCoord.xy;
	}

	vec3 DefaultDiffuseLight(int index)
	{
		vec3 lightDir = normalize(modellight_info[index][0].xyz - translated_vertpos);
		float lightDistance = length(modellight_info[index][0].xyz - translated_vertpos);
		
		float spotattenuation = 1.0;
		if (modellight_info[index][2].w < 1.0) //spotlight
		{
		    float cosangle = dot(-lightDir, modellight_info[index][2].xyz);
		    float spotcosedge = modellight_info[index][2].w;
		    float spotcosdiff = max(cosangle - spotcosedge, 0.0);
		    float spotcosrange = 1.0 - spotcosedge;
		    spotattenuation = clamp(spotcosdiff / spotcosrange, 0.0, 1.0);
		}
		
		float radiussquared = sqr(modellight_info[index][1].w);
		float distattenuation = max(1.0 - (sqr(lightDistance) / radiussquared), 0.0);
		
		float lightfactor = spotattenuation * distattenuation;
		
		float surfacedot = max(-dot(translated_normal, -lightDir), 0.0);
		float finallight = lightfactor * surfacedot;
		
		return modellight_info[index][1].rgb * finallight;
	}

	vec3 DefaultSpecularLight(int index)
	{
		float specularStrength = 0.6;
		float specularFactor = 16;
		if ((texture_flags & STUDIO_NF_CHROME) > 0)
		{
			specularStrength = 1.5;
			specularFactor = 32;
		}

		vec3 lightDir = normalize(modellight_info[index][0].xyz - translated_vertpos);
		float lightDistance = length(modellight_info[index][0].xyz - translated_vertpos);
		vec3 viewDir = normalize(renderorigin.xyz - translated_vertpos);
		vec3 reflectDir = reflect(-lightDir, translated_normal);

		float spotattenuation = 1.0;
		if (modellight_info[index][2].w < 1.0) //spotlight
		{
		    float cosangle = dot(-lightDir, modellight_info[index][2].xyz);
		    float spotcosedge = modellight_info[index][2].w;
		    float spotcosdiff = max(cosangle - spotcosedge, 0.0);
		    float spotcosrange = 1.0 - spotcosedge;
		    spotattenuation = clamp(spotcosdiff / spotcosrange, 0.0, 1.0);
		}

		float radiussquared = sqr(modellight_info[index][1].w);
		float distattenuation = max(1.0 - (sqr(lightDistance) / radiussquared), 0.0);

		float specularpower = pow( max( dot(viewDir, reflectDir), 0.0), 16);
		
		return modellight_info[index][1].rgb * (specularStrength * specularpower) * (spotattenuation * distattenuation);
	}


	void NormalVertexLight()
	{
		//add basic lighting
		float diffusefactor = dot(translated_normal, lightdir.xyz);
		vec3 diffuselighting = ambientlight.xyz + (diffuselight.xyz * max(-diffusefactor, 0.0));
		vec3 specularlighting = vec3(0, 0, 0);

		//now add complex lighting

		for (int i = 0; i < int_values[_NUMLIGHTS]; ++i) 
		{
			diffuselighting += DefaultDiffuseLight(i);
			specularlighting += DefaultSpecularLight(i);
		}
		
		//

		diffuselighting = clamp(diffuselighting, 0.0, 1.0);
		specularlighting = clamp(specularlighting, 0.0, 1.0);
		vertexdiffusecolor = vec4(diffuselighting, 1);
		vertexspecularcolor = vec4(specularlighting, 1);
	}

	void Vertex_NoLight()
	{
		vertexdiffusecolor = vec4(1.0);
	}

	void main() {

		int vertbone = (aBoneID) & 255;
		int normbone = (aBoneID >> 8) & 255;

		if(int_values.z == 0)
		{
			translated_vertpos = VectorTransform(aPosition, bonematrixes[vertbone]);
			translated_normal = VectorRotate(aNormal, bonematrixes[vertbone]);
		}
		else //static prop
		{
			translated_vertpos = vec4( modelmatrix * vec4(aPosition, 1)).xyz;
			translated_normal = normalize( transpose(inverse(mat3(modelmatrix))) * aNormal );
		}

		if( (texture_flags & STUDIO_NF_FULLBRIGHT) > 0 || int_values[_CHROMESHELL_BOOL] != 0 )
			Vertex_NoLight();
		else if(!wireframe)
			NormalVertexLight();

		CalcGLPosition();
		
		CalcTexCoords(normbone);

		fragPos = translated_vertpos;
	}



)";

char glsl330_studiomdl_frag[] = R"(

	in vec3 fragPos;
	in vec2 texcoord;
	in vec4 projectedCoord;
	in vec4 vertexdiffusecolor;
	in vec4 vertexspecularcolor;

	#define MAX_MODEL_LIGHTS 12 // 2*(up, down, left, right, front, back)

	// lighting options
	#define STUDIO_NF_FLATSHADE 1
	#define STUDIO_NF_CHROME 2
	#define STUDIO_NF_ADDITIVE 32  // buz
	#define STUDIO_NF_ALPHATEST 64 // buz
	#define STUDIO_NF_FULLBRIGHT 512
	#define STUDIO_NF_NOMIPMAP 256


	layout(std140) uniform studiomdl_PerFrame
	{	
		mat4 projviewmatrix;
		mat4 VMprojviewmatrix;
		vec4 fogcolor_n_fogstart; //w = fogstart
		vec4 fogend_n_fogactive_n_lightdebug; //x = fogend, y = fogactive
	
		vec4 renderorigin;
		vec4 renderright;
	};


	layout(std140) uniform studiomdl_PerEntity
	{
		vec4 lightdir;
		vec4 ambientlight;
		vec4 diffuselight;
	
		mat4 modelmatrix;
	
		ivec4 int_values; //x = numlights; y = chromeshell boolean; z = is this entity is static (prop_static) or not

		vec4 rendervalues; //rendercolor.r, rendercolor.g, rendercolor.b, renderamt
		
		mat3x4 modellight_info[MAX_MODEL_LIGHTS];

		//modellight_info[i][0] = modellight_origin[i]
		//modellight_info[i][1] = modellight_color[i]
		//modellight_info[i][2] = modellight_forward[i]
	};



	uniform sampler2D texture0;

	uniform bool wireframe;

	uniform int texture_flags;

	uniform bool studiodecal;
	uniform vec2 decalsize;

	float GetFogFactor()
	{
		float dist = length(renderorigin.xyz - fragPos);

		float fogFactor = (fogend_n_fogactive_n_lightdebug.x - dist) / (fogend_n_fogactive_n_lightdebug.x - fogcolor_n_fogstart.w);
		fogFactor = clamp(fogFactor, 0.0, 1.0);
		
		return fogFactor;
	}

	void frag_HandleWireframe()
	{
		if(!studiodecal)
			gl_FragColor = vec4(0, 1, 0, 1); //solid green for models
		else
			gl_FragColor = vec4(1, 0, 0, 1); //solid red for studiomdl decals
	}

	void main()
	{
		if(wireframe)
		{
			frag_HandleWireframe();
			return;
		}
		vec4 texcolor = vec4(1.0, 1.0, 1.0, 1.0);

		if(texture(texture0, texcoord).a < 0.5)
			discard;

		if(fogend_n_fogactive_n_lightdebug.z == 0)
		{
			texcolor = texture(texture0, texcoord);
			if(!studiodecal)
				texcolor.rgb *= 2;
		}

		if (fogend_n_fogactive_n_lightdebug.y != 0)  // fog blend
		{
			if(!studiodecal)
				gl_FragColor = mix( (texcolor * vertexdiffusecolor) + (texcolor * vertexspecularcolor), vec4(fogcolor_n_fogstart.xyz, 1), 1.0 - GetFogFactor() );
			else
				gl_FragColor = mix( texcolor, vec4(fogcolor_n_fogstart.xyz, 1), 1.0 - GetFogFactor() );
		}
		else
		{
			if(!studiodecal)
				gl_FragColor = (texcolor * vertexdiffusecolor)  + (texcolor * vertexspecularcolor);
			else
				gl_FragColor = texcolor;
		}

		gl_FragColor.a = rendervalues.a;
		
	}

)";