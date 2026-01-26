/*
Trinity Rendering Engine - Copyright Andrew Lucas 2009-2012

The Trinity Engine is free software, distributed in the hope th-
at it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU Lesser General Public License for more det-
ails.

Water Shader
Written by Andrew Lucas
*/

#if !defined(WATERSHADER_H)
#define WATERSHADER_H
#if defined(_WIN32)
#pragma once
#endif

#include "PlatformHeaders.h"
#include "pm_defs.h"
#include "cl_entity.h"
#include "ref_params.h"
#include "dlight.h"
#include "parsemsg.h"
#include "cvardef.h"
#include "textureloader.h"
#include "rendererdefs.h"

class GL_FBOHandler;
class GL_RBOHandler;
class GL_ShaderProgram;

//==================================================
//				WATER SHADER DEFS
//
//==================================================
#define MAX_WATER_ENTITIES 64
#define MAX_WATER_VERTEX_SHADERS 2
#define MAX_WATER_FRAGMENT_SHADERS 4
#define WATER_RESOLUTION 512

//==================================================
//				WATER SHADER STRUCTS
//
//==================================================

struct cl_waterinfo_t
{
	cl_entity_s* entity;
	Vector waterfog_color;
	int waterfog_start;
	int waterfog_end;
	float watertex_scale;
	float refraction_scale, reflection_scale;
	float normal_scale;
	float fresnel;
};

struct cl_water_t
{
	int index;
	cl_entity_t* entity;

	mplane_t wplane;

	Vector mins;
	Vector maxs;
	Vector origin;
	bool draw;

	GL_TextureHandler* refract;
	GL_TextureHandler* reflect;

	clientmsurface_t** surfaces;
	int numsurfaces;

	bool rendered;
};

/*
====================
CWaterShader

====================
*/
class CWaterShader
{
public:
	void Init(void);
	void Shutdown(void);
	void VidInit(void);
	void Restore(void);
	void ClearEntities(void);

	void AddEntity(cl_entity_t* entity);
	void DrawWater(void);

	void DrawWaterPasses(ref_params_t* pparams);
	void DrawScene(ref_params_t* pparams, bool forcemodels);

	void SetupRefract(void);
	void FinishRefract(void);

	void SetupReflect(void);
	void FinishReflect(void);

	void SetupClipping(ref_params_t* pparams, bool isrefracting);
	void LoadScript(void);

	bool ViewInWater(void);
	bool ShouldReflect(int index);

	Vector GetWaterOrigin(cl_water_t* pwater = nullptr);

	int MsgWaterInfo(const char* pszName, int iSize, void* pbuf);

public:
	bool m_bViewInWater;
	Vector m_vViewOrigin;

	cl_waterinfo_t m_pWaterEntInfo[MAX_WATER_ENTITIES]; // each func_water can control how the shader works :)
	int m_iNumWaterData;

	cl_water_t m_pWaterEntities[MAX_WATER_ENTITIES];
	int m_iNumWaterEntities;

	cvar_t* m_pCvarWaterShader;
	cvar_t* m_pCvarWaterDebug;
	cvar_t* m_pCvarWaterResolution;

	cvar_t* m_pCvarWaterFogStart;
	cvar_t* m_pCvarWaterFogEnd;
	cvar_t* m_pCvarWaterTexscale;
	cvar_t* m_pCvarWaterRefractScale;
	cvar_t* m_pCvarWaterReflectScale;
	cvar_t* m_pCvarWaterNormalScale;
	cvar_t* m_pCvarWaterFresnel;
	cvar_t* m_pCvarWaterForceExpensive;
	cvar_t* m_pCvarWaterForceReflectEntities;

	cl_texture_t* m_pNormalTexture;
	cl_water_t* m_pCurWater;

	ref_params_t* m_pViewParams;
	ref_params_t m_pWaterParams;

	Vector m_vWaterOrigin;
	Vector m_vWaterPlaneMins;
	Vector m_vWaterPlaneMaxs;
	Vector m_vWaterEntMins;
	Vector m_vWaterEntMaxs;

	int m_iNumPasses;
	double m_fRenderTime;

public:
	GL_ShaderProgram *m_WaterFragmentShader;

	GL_FBOHandler* m_waterFBO;
	GL_RBOHandler* m_waterDepthBuffer;
	
	enum watershader_uniforms
	{
		watershader_renderorigin,

		watershader_projviewmodelmatrix,

		watershader_underwater,

		watershader_waterfog, // program.local[1] = (r, g, b)
		watershader_fogstart, 
		watershader_fogend,
		watershader_m_flFresnelTerm, // program.local[2] = float
		watershader_flTime,			 // program.local[3] = client time

		watershader_normalscale,
		watershader_watertex_scale,
		watershader_refraction_scale,
		watershader_reflection_scale,

		_watershader_locsize
		
	};

	GLuint m_WaterShader_locs[_watershader_locsize];


public:
	fog_settings_t m_pMainFogSettings;
	fog_settings_t m_pWaterFogSettings;

	float m_flFresnelTerm;
	int m_iLastWaterRes = 0;
};

extern CWaterShader gWaterShader;
#endif