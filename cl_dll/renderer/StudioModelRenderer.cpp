/*
Trinity Rendering Engine - Copyright Andrew Lucas 2009-2012

The Trinity Engine is free software, distributed in the hope th-
at it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU Lesser General Public License for more det-
ails.

Model Renderer
Original code by Valve
Additional code written by Andrew Lucas
Transparency code by Neil "Jed" Jedrzejewski
*/

#include <windows.h>

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "studio.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "dlight.h"
#include "triangleapi.h"

#include "event_api.h"
#include "pmtrace.h"

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <math.h>
#include <algorithm>    // std::sort

#include "studio_util.h"
#include "r_studioint.h"

#include "../renderer/rendererdefs.h"
#include "../renderer/propmanager.h"
#include "../renderer/bsprenderer.h"
#include "../renderer/opengl_utils/GL_Buffers.h"
#include "../renderer/opengl_utils/GL_ShaderProgram.h"
#include "../renderer/opengl_utils/GL_StateHandler.h"
#include "../renderer/opengl_utils/GL_VertexArrayObject.h"
#include "../renderer/StudioMDL_MeshGen.h"

#include "StudioModelRenderer.h"

#include "BSPModel_Gen.h"

#include "Exports.h"


matrix3x4_t(*CStudioModelRenderer::m_protationmatrix);
matrix3x4_t(*CStudioModelRenderer::m_paliastransform);


matrix3x4_t (*CStudioModelRenderer::m_pbonetransform)[MAXSTUDIOBONES];
matrix3x4_t (*CStudioModelRenderer::m_plighttransform)[MAXSTUDIOBONES];

int CStudioModelRenderer::m_nCachedBones;
char CStudioModelRenderer::m_nCachedBoneNames[MAXSTUDIOBONES][32];
matrix3x4_t CStudioModelRenderer::m_rgCachedBoneTransform[MAXSTUDIOBONES];

double CStudioModelRenderer::m_fStudioMDLRenderTime;

cvar_t* CStudioModelRenderer::m_pCvarHiModels;
cvar_t* CStudioModelRenderer::m_pCvarDeveloper;
cvar_t* CStudioModelRenderer::m_pCvarDrawEntities;
cvar_t* CStudioModelRenderer::m_pCvarDrawViewmodel;

cvar_t* CStudioModelRenderer::m_pCvarDrawStudioModels;
cvar_t* CStudioModelRenderer::m_pCvarStudioModelBBox;
cvar_t* CStudioModelRenderer::m_pCvarStudioModelLightDebug;
cvar_t* CStudioModelRenderer::m_pCvarStudioModelDecals;
cvar_t* CStudioModelRenderer::m_pCvarGlowShellFreq;

cvar_t* CStudioModelRenderer::m_pCvarSkyVecX;
cvar_t* CStudioModelRenderer::m_pCvarSkyVecY;
cvar_t* CStudioModelRenderer::m_pCvarSkyVecZ;

cvar_t* CStudioModelRenderer::m_pCvarSkyColorX;
cvar_t* CStudioModelRenderer::m_pCvarSkyColorY;
cvar_t* CStudioModelRenderer::m_pCvarSkyColorZ;

cvar_t* CStudioModelRenderer::m_pCvarViewmodelFov;

player_info_t*	pPlayerInfo		= nullptr;
int				nPlayerIndex	= -1;

static cl_entity_t*				m_pCurrentEntity		= nullptr;
static entextradata_t*			m_pCurrentExtraData		= nullptr;
static studiohdr_t*				m_pStudioHeader			= nullptr;
static StudioMDL_Model*			m_pCurrentStudioMDL		= nullptr;
static studioentity_data_t*		m_pCurrentStudioEntData = nullptr;
static model_t*					m_pRenderModel			= nullptr;

// Pointers to current body part and submodel
static mstudiobodyparts_t*		m_pBodyPart		= nullptr;
static mstudiomodel_t*			m_pSubModel		= nullptr;

static model_t*					m_pChromeSprite = nullptr;// Sprite model used for drawing studio model chrome

static Vector					v_bboxMins;
static Vector					v_bboxMaxs;

static Vector					vVertexTransform[MAXSTUDIOVERTS]; // transformed vertices
static Vector					vNormalTransform[MAXSTUDIOVERTS]; // transformed normals

static lighting_ext				m_pLighting; // buz

static mlight_t*				pModelLights[MAX_MODEL_LIGHTS];
static int						iNumModelLights;
static entextrainfo_t			pExtraInfo[MAXRENDERENTS];
static int						iNumExtraInfo;

static std::unordered_map<model_t*, std::vector<cl_entity_t*>> vStudioDrawList;

static bool						bExternalEntity = false;
static bool						bChromeShell	= false;
static bool						bDoInterp		= true;// Do interpolation?
static bool						bGaitEstimation = true;// Do gait estimation?

//===========================================
// OPENGL START
//
//===========================================

#include "glshaders/studiomdl_glsl.h"
#include "glshaders/shadow/studiomdl_solid_glsl.h"

extern GL_Mesh* g_pStaticMeshBuffer; //propmanager.cpp

static GL_BufferHandler*		pModel_PerEntityBuffer	= nullptr;
static GL_BufferHandler*		pModel_PerFrameBuffer	= nullptr;
static GL_BufferHandler*		pModel_BonesBuffer		= nullptr;
static GL_BufferHandler*		pModel_SolidBuffer		= nullptr;
static GL_BufferHandler*		pModel_DecalBuffer		= nullptr;

static GL_VertexArrayObject*	pModel_DecalVAO			= nullptr;

static GL_ShaderProgram			s_MDLShader(glsl330_studiomdl_vert, glsl330_studiomdl_frag);
static GL_ShaderProgram			s_MDLSolidShader(glsl330_studiomdlsolid_vert, glsl330_studiomdlsolid_frag);

static glm::mat4				VMProjectionMatrix;

enum{
	mdlshader_viewmodel,

	mdlshader_texturematrix,

	mdlshader_wireframe,
	mdlshader_texture_flags,

	mdlshader_studiodecal,
	mdlshader_decalsize,

	_mdlshader_uniformsize //must be last
}; static GLuint eModel_ShaderLocs[_mdlshader_uniformsize];

enum{
	mdlshadersolid_sunshadow,
	mdlshadersolid_texture_flags,

	_mdlshadersolid_uniformsize //must be last
}; static GLuint eModel_ShaderSolidLocs[_mdlshadersolid_uniformsize];

struct mdlshadersolid_data_t{
	glm::mat4 projviewmatrix;
	glm::mat4 modelmatrix;
	glm::vec4 light_pos;
	glm::ivec4 int_values; // x represents if we're rendering a static model or not
} model_SolidData;
struct mdlshader_perframedata_t{
	glm::mat4 projviewmatrix;
	glm::mat4 VMprojviewmatrix;
	glm::vec4 fogcolor_n_fogstart; // w = fogstart
	glm::vec4 fogend_n_fogactive_n_lightdebug;  // x = fogend, y = fogactive, z = light debug cvar

	// w is empty for both these :( wasted space

	glm::vec4 renderorigin;
	glm::vec4 renderright;
} model_PerFrameData;
struct mdlshader_perentitydata_t{
	glm::vec4 lightdir;
	glm::vec4 ambientlight;
	glm::vec4 diffuselight;

	glm::mat4 modelmatrix;

	glm::ivec4 int_values; // x = numlights; y = chromeshell boolean; z = is this entity is static (prop_static) or not

	glm::vec4 rendervalues; //rendercolor.r, rendercolor.g, rendercolor.b, renderamt

	glm::mat3x4 modellight_info[MAX_MODEL_LIGHTS];
} model_PerEntityData;

//===========================================
// OPENGL END
//
//===========================================

static vboheader_t*					pVBOHeader		= nullptr;
static vbosubmodel_t*				pVBOSubModel	= nullptr;
static std::vector<studiovert_t>	pRefArray;
static std::vector<Default3DVert_t>	pVBOVerts;
static std::vector<unsigned int>	usIndexes;
static int iCurStart = 0;

static std::vector<std::unique_ptr<studiodecal_t>> pStudioDecals;
static int iNumStudioDecalVerts = 0;
static std::vector<std::unique_ptr<studioentity_data_t>> pStudioEntityData;



















// Global engine <-> studio model rendering code interface
engine_studio_api_t IEngineStudio;

CStudioModelRenderer g_StudioRenderer;

extern model_t* cl_sprite_muzzleflash[3];
extern model_t* cl_sprite_ricochet;
extern model_t* cl_sprite_dot;
extern model_t* cl_sprite_lightning;
extern model_t* cl_sprite_white;
extern model_t* cl_sprite_glow;


/*
====================
Init

====================
*/
void CStudioModelRenderer::Init(void)
{
	// Set up some variables shared with engine
	m_pCvarHiModels = IEngineStudio.GetCvar("cl_himodels");
	m_pCvarDeveloper = IEngineStudio.GetCvar("developer");
	m_pCvarDrawEntities = IEngineStudio.GetCvar("r_drawentities");
	m_pCvarDrawViewmodel = IEngineStudio.GetCvar("r_drawviewmodel");

	m_pCvarSkyColorX = IEngineStudio.GetCvar("sv_skycolor_r");
	m_pCvarSkyColorY = IEngineStudio.GetCvar("sv_skycolor_g");
	m_pCvarSkyColorZ = IEngineStudio.GetCvar("sv_skycolor_b");

	m_pCvarSkyVecX = IEngineStudio.GetCvar("sv_skyvec_x");
	m_pCvarSkyVecY = IEngineStudio.GetCvar("sv_skyvec_y");
	m_pCvarSkyVecZ = IEngineStudio.GetCvar("sv_skyvec_z");

	m_pCvarGlowShellFreq = IEngineStudio.GetCvar("r_glowshellfreq");

	m_pCvarDrawStudioModels = gEngfuncs.pfnRegisterVariable("r_drawstudiomdl", "1", 0);
	m_pCvarStudioModelDecals = gEngfuncs.pfnRegisterVariable("r_drawstudiomdl_decals", "1", 0);

	m_pCvarStudioModelBBox = gEngfuncs.pfnRegisterVariable("r_drawstudiomdl_bbox", "0", 0);
	m_pCvarStudioModelLightDebug = gEngfuncs.pfnRegisterVariable("r_drawstudiomdl_debuglight", "0", 0);

	m_pCvarViewmodelFov = gEngfuncs.pfnRegisterVariable("viewmodel_fov", "70", FCVAR_ARCHIVE); // best matches the half-life 2 viewmodel fov

	cl_sprite_muzzleflash[0] = IEngineStudio.Mod_ForName("sprites/muzzleflash1.spr", false);
	cl_sprite_muzzleflash[1] = IEngineStudio.Mod_ForName("sprites/muzzleflash2.spr", false);
	cl_sprite_muzzleflash[2] = IEngineStudio.Mod_ForName("sprites/muzzleflash3.spr", false);

	cl_sprite_ricochet = IEngineStudio.Mod_ForName("sprites/richo1.spr", false);
	cl_sprite_dot = IEngineStudio.Mod_ForName("sprites/dot.spr", false);

	// Get pointers to engine data structures
	m_pbonetransform = (matrix3x4_t(*)[MAXSTUDIOBONES])IEngineStudio.StudioGetBoneTransform();
	m_plighttransform = (matrix3x4_t(*)[MAXSTUDIOBONES])IEngineStudio.StudioGetLightTransform();
	m_paliastransform = (matrix3x4_t(*))IEngineStudio.StudioGetAliasTransform();
	m_protationmatrix = (matrix3x4_t(*))IEngineStudio.StudioGetRotationMatrix();

	m_pChromeSprite = IEngineStudio.GetChromeSprite();

	//
	// Load GLSL shader(s)
	//

	eModel_ShaderLocs[mdlshader_viewmodel] = s_MDLShader.GetUniformLoc("viewmodel");

	//eModel_ShaderLocs[mdlshader_texturematrix] = s_MDLShader.GetUniformLoc("texturematrix");

	eModel_ShaderLocs[mdlshader_wireframe] = s_MDLShader.GetUniformLoc("wireframe");
	eModel_ShaderLocs[mdlshader_texture_flags] = s_MDLShader.GetUniformLoc("texture_flags");

	eModel_ShaderLocs[mdlshader_studiodecal] = s_MDLShader.GetUniformLoc("studiodecal");
	eModel_ShaderLocs[mdlshader_decalsize] = s_MDLShader.GetUniformLoc("decalsize");

	eModel_ShaderSolidLocs[mdlshadersolid_sunshadow] = s_MDLSolidShader.GetUniformLoc("bSunShadowMapPass");
	eModel_ShaderSolidLocs[mdlshadersolid_texture_flags] = s_MDLSolidShader.GetUniformLoc("texture_flags");

	s_MDLShader.Bind();
	s_MDLShader.Uniform1i(s_MDLShader.GetUniformLoc("texture0"), 0);

	pModel_BonesBuffer = new GL_BufferHandler();
	pModel_BonesBuffer->Bind(GL_BufferHandler::UniformBuffer);
	pModel_BonesBuffer->BufferData(GL_BufferHandler::UniformBuffer, (sizeof(matrix3x4_t) * 128) * 2048, nullptr, GL_BufferHandler::DynamicDraw);
	pModel_BonesBuffer->BindRange(GL_BufferHandler::UniformBuffer, s_MDLShader.GetUBOIndex("BonesUBO"), 0, sizeof(matrix3x4_t) * 128);

	s_MDLSolidShader.Bind();
	s_MDLSolidShader.Uniform1i(s_MDLSolidShader.GetUniformLoc("texture0"), 0);

	pModel_SolidBuffer = new GL_BufferHandler();
	pModel_SolidBuffer->Bind(GL_BufferHandler::UniformBuffer);
	pModel_SolidBuffer->BufferData(GL_BufferHandler::UniformBuffer, sizeof(mdlshadersolid_data_t), nullptr, GL_BufferHandler::DynamicDraw);
	pModel_SolidBuffer->BindRange(GL_BufferHandler::UniformBuffer, s_MDLSolidShader.GetUBOIndex("StudioSolidUBO"), 0, sizeof(mdlshadersolid_data_t));

	pModel_PerFrameBuffer = new GL_BufferHandler();
	pModel_PerFrameBuffer->Bind(GL_BufferHandler::UniformBuffer);
	pModel_PerFrameBuffer->BufferData(GL_BufferHandler::UniformBuffer, sizeof(mdlshader_perframedata_t), nullptr, GL_BufferHandler::DynamicDraw);
	pModel_PerFrameBuffer->BindRange(GL_BufferHandler::UniformBuffer, s_MDLShader.GetUBOIndex("studiomdl_PerFrame"), 0, sizeof(mdlshader_perframedata_t));

	pModel_PerEntityBuffer = new GL_BufferHandler();
	pModel_PerEntityBuffer->Bind(GL_BufferHandler::UniformBuffer);
	pModel_PerEntityBuffer->BufferData(GL_BufferHandler::UniformBuffer, sizeof(mdlshader_perentitydata_t), nullptr, GL_BufferHandler::DynamicDraw);
	pModel_PerEntityBuffer->BindRange(GL_BufferHandler::UniformBuffer, s_MDLShader.GetUBOIndex("studiomdl_PerEntity"), 0, sizeof(mdlshader_perentitydata_t));

	pModel_DecalVAO = new GL_VertexArrayObject();
	pModel_DecalVAO->BindVAO();

	pModel_DecalBuffer = new GL_BufferHandler();
	pModel_DecalBuffer->Bind(GL_BufferHandler::ArrayBuffer);
	pModel_DecalBuffer->BufferData(GL_BufferHandler::ArrayBuffer, (sizeof(studiomdl_vertbufferdata_t) * 3) * 65536, nullptr, GL_BufferHandler::DynamicDraw);

	pModel_DecalVAO->SetVertexAttributes(studiomdl_vertbufferdata_t::GetAttribLayout());

	GL_VertexArrayObject::ResetVAOBinding();
	GL_ShaderProgram::ResetShaderBind();
	GL_BufferHandler::ResetBufferBinding(GL_BufferHandler::ArrayBuffer);
}

/*
====================
VidInit

====================
*/
void CStudioModelRenderer::VidInit(void)
{
	if (!pStudioDecals.empty())
	{
		for (auto &studiodecal : pStudioDecals)
		{
			if (studiodecal->numpolys)
			{
				for (int j = 0; j < studiodecal->numpolys; j++)
					delete[] studiodecal->polys[j].verts;

				delete[] studiodecal->polys;
			}

			if (studiodecal->numverts)
				delete[] studiodecal->verts;
		}
	}
	pStudioDecals.clear();

	iNumStudioDecalVerts = 0;

	if (iNumExtraInfo)
	{
		memset(pExtraInfo, 0, sizeof(pExtraInfo));
		iNumExtraInfo = 0;
	}
}

/*
====================
StudioCalcBoneAdj

====================
*/
void CStudioModelRenderer::StudioCalcBoneAdj(float dadt, float* adj, const byte* pcontroller1, const byte* pcontroller2, byte mouthopen)
{
	int i, j;
	float value;
	mstudiobonecontroller_t* pbonecontroller;

	pbonecontroller = (mstudiobonecontroller_t*)((byte*)m_pStudioHeader + m_pStudioHeader->bonecontrollerindex);

	for (j = 0; j < m_pStudioHeader->numbonecontrollers; j++)
	{
		i = pbonecontroller[j].index;
		if (i <= 3)
		{
			// check for 360% wrapping
			if (pbonecontroller[j].type & STUDIO_RLOOP)
			{
				if (abs(pcontroller1[i] - pcontroller2[i]) > 128)
				{
					int a, b;
					a = (pcontroller1[j] + 128) % 256;
					b = (pcontroller2[j] + 128) % 256;
					value = ((a * dadt) + (b * (1 - dadt)) - 128) * (360.0 / 256.0) + pbonecontroller[j].start;
				}
				else
				{
					value = ((pcontroller1[i] * dadt + (pcontroller2[i]) * (1.0 - dadt))) * (360.0 / 256.0) + pbonecontroller[j].start;
				}
			}
			else
			{
				value = (pcontroller1[i] * dadt + pcontroller2[i] * (1.0 - dadt)) / 255.0;
				if (value < 0)
					value = 0;
				if (value > 1.0)
					value = 1.0;
				value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			}
			// Con_DPrintf( "%d %d %f : %f\n", m_pCurrentEntity->curstate.controller[j], m_pCurrentEntity->latched.prevcontroller[j], value, dadt );
		}
		else
		{
			value = mouthopen / 64.0;
			if (value > 1.0)
				value = 1.0;
			value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			// Con_DPrintf("%d %f\n", mouthopen, value );
		}
		switch (pbonecontroller[j].type & STUDIO_TYPES)
		{
		case STUDIO_XR:
		case STUDIO_YR:
		case STUDIO_ZR:
			adj[j] = value * (M_PI / 180.0);
			break;
		case STUDIO_X:
		case STUDIO_Y:
		case STUDIO_Z:
			adj[j] = value;
			break;
		}
	}
}


/*
====================
StudioCalcBoneQuaterion

====================
*/
void CStudioModelRenderer::StudioCalcBoneQuaterion(int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* q)
{
	int j, k;
	vec4_t q1, q2;
	Vector angle1, angle2;
	mstudioanimvalue_t* panimvalue;

	for (j = 0; j < 3; j++)
	{
		if (panim->offset[j + 3] == 0)
		{
			angle2[j] = angle1[j] = pbone->value[j + 3]; // default;
		}
		else
		{
			panimvalue = (mstudioanimvalue_t*)((byte*)panim + panim->offset[j + 3]);
			k = frame;
			// DEBUG
			if (panimvalue->num.total < panimvalue->num.valid)
				k = 0;
			while (panimvalue->num.total <= k)
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
				// DEBUG
				if (panimvalue->num.total < panimvalue->num.valid)
					k = 0;
			}
			// Bah, missing blend!
			if (panimvalue->num.valid > k)
			{
				angle1[j] = panimvalue[k + 1].value;

				if (panimvalue->num.valid > k + 1)
				{
					angle2[j] = panimvalue[k + 2].value;
				}
				else
				{
					if (panimvalue->num.total > k + 1)
						angle2[j] = angle1[j];
					else
						angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
				}
			}
			else
			{
				angle1[j] = panimvalue[panimvalue->num.valid].value;
				if (panimvalue->num.total > k + 1)
				{
					angle2[j] = angle1[j];
				}
				else
				{
					angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
				}
			}
			angle1[j] = pbone->value[j + 3] + angle1[j] * pbone->scale[j + 3];
			angle2[j] = pbone->value[j + 3] + angle2[j] * pbone->scale[j + 3];
		}

		if (pbone->bonecontroller[j + 3] != -1)
		{
			angle1[j] += adj[pbone->bonecontroller[j + 3]];
			angle2[j] += adj[pbone->bonecontroller[j + 3]];
		}
	}

	if (!VectorCompare(angle1, angle2))
	{
		AngleQuaternion(angle1, q1);
		AngleQuaternion(angle2, q2);
		QuaternionSlerp(q1, q2, s, q);
	}
	else
	{
		AngleQuaternion(angle1, q);
	}
}

/*
====================
StudioCalcBonePosition

====================
*/
void CStudioModelRenderer::StudioCalcBonePosition(int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* pos)
{
	int j, k;
	mstudioanimvalue_t* panimvalue;

	for (j = 0; j < 3; j++)
	{
		pos[j] = pbone->value[j]; // default;
		if (panim->offset[j] != 0)
		{
			panimvalue = (mstudioanimvalue_t*)((byte*)panim + panim->offset[j]);

			k = frame;
			// DEBUG
			if (panimvalue->num.total < panimvalue->num.valid)
				k = 0;
			// find span of values that includes the frame we want
			while (panimvalue->num.total <= k)
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
				// DEBUG
				if (panimvalue->num.total < panimvalue->num.valid)
					k = 0;
			}
			// if we're inside the span
			if (panimvalue->num.valid > k)
			{
				// and there's more data in the span
				if (panimvalue->num.valid > k + 1)
				{
					pos[j] += (panimvalue[k + 1].value * (1.0 - s) + s * panimvalue[k + 2].value) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[k + 1].value * pbone->scale[j];
				}
			}
			else
			{
				// are we at the end of the repeating values section and there's another section with data?
				if (panimvalue->num.total <= k + 1)
				{
					pos[j] += (panimvalue[panimvalue->num.valid].value * (1.0 - s) + s * panimvalue[panimvalue->num.valid + 2].value) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[panimvalue->num.valid].value * pbone->scale[j];
				}
			}
		}
		if (pbone->bonecontroller[j] != -1 && adj)
		{
			pos[j] += adj[pbone->bonecontroller[j]];
		}
	}
}

/*
====================
StudioSlerpBones

====================
*/
void CStudioModelRenderer::StudioSlerpBones(vec4_t q1[], float pos1[][3], vec4_t q2[], float pos2[][3], float s)
{
	int i;
	vec4_t q3;
	float s1;

	if (s < 0)
		s = 0;
	else if (s > 1.0)
		s = 1.0;

	s1 = 1.0 - s;

	for (i = 0; i < m_pStudioHeader->numbones; i++)
	{
		QuaternionSlerp(q1[i], q2[i], s, q3);
		q1[i][0] = q3[0];
		q1[i][1] = q3[1];
		q1[i][2] = q3[2];
		q1[i][3] = q3[3];
		pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * s;
		pos1[i][1] = pos1[i][1] * s1 + pos2[i][1] * s;
		pos1[i][2] = pos1[i][2] * s1 + pos2[i][2] * s;
	}
}

/*
====================
StudioGetAnim

====================
*/
mstudioanim_t* CStudioModelRenderer::StudioGetAnim(model_t* m_pModel, mstudioseqdesc_t* pseqdesc)
{
	mstudioseqgroup_t* pseqgroup;
	cache_user_t* paSequences;

	pseqgroup = (mstudioseqgroup_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqgroupindex) + pseqdesc->seqgroup;

	if (pseqdesc->seqgroup == 0)
	{
		return (mstudioanim_t*)((byte*)m_pStudioHeader + pseqgroup->data + pseqdesc->animindex);
	}

	paSequences = (cache_user_t*)m_pModel->submodels;

	if (paSequences == NULL)
	{
		paSequences = (cache_user_t*)IEngineStudio.Mem_Calloc(16, sizeof(cache_user_t)); // UNDONE: leak!
		m_pModel->submodels = (dmodel_t*)paSequences;
	}

	//cache_check basically just returns the data back so just check the data instead

	if (!paSequences[pseqdesc->seqgroup].data)
	{
		IEngineStudio.Cache_Check((struct cache_user_s*)&(paSequences[pseqdesc->seqgroup])); //cache_check moves the cache header/pointer/whatever
		gEngfuncs.Con_DPrintf("loading %s\n", pseqgroup->name);
		IEngineStudio.LoadCacheFile(pseqgroup->name, (struct cache_user_s*)&paSequences[pseqdesc->seqgroup]);
	}
	return (mstudioanim_t*)((byte*)paSequences[pseqdesc->seqgroup].data + pseqdesc->animindex);
}

/*
====================
StudioPlayerBlend

====================
*/
void CStudioModelRenderer::StudioPlayerBlend(mstudioseqdesc_t* pseqdesc, int* pBlend, float* pPitch)
{
	// calc up/down pointing
	*pBlend = (*pPitch * 3);
	if (*pBlend < pseqdesc->blendstart[0])
	{
		*pPitch -= pseqdesc->blendstart[0] / 3.0;
		*pBlend = 0;
	}
	else if (*pBlend > pseqdesc->blendend[0])
	{
		*pPitch -= pseqdesc->blendend[0] / 3.0;
		*pBlend = 255;
	}
	else
	{
		if (pseqdesc->blendend[0] - pseqdesc->blendstart[0] < 0.1) // catch qc error
			*pBlend = 127;
		else
			*pBlend = 255 * (*pBlend - pseqdesc->blendstart[0]) / (pseqdesc->blendend[0] - pseqdesc->blendstart[0]);
		*pPitch = 0;
	}
}

/*
====================
StudioPreFrame

====================
*/
void CStudioModelRenderer::StudioPreFrame(ref_params_t* pparams)
{
	model_PerFrameData.renderorigin = glm::vec4(pparams->vieworg.x, pparams->vieworg.y, pparams->vieworg.z, 0);
	model_PerFrameData.renderright = glm::vec4(pparams->right.x, pparams->right.y, pparams->right.z, 0);
	model_PerFrameData.projviewmatrix = gBSPRenderer.m_ProjectionMatrix * gBSPRenderer.m_ViewMatrix;
	model_PerFrameData.VMprojviewmatrix = VMProjectionMatrix * gBSPRenderer.m_ViewMatrix;
	model_PerFrameData.fogcolor_n_fogstart = glm::vec4(gHUD.m_pFogSettings.color.x, gHUD.m_pFogSettings.color.y, gHUD.m_pFogSettings.color.z, gHUD.m_pFogSettings.start);
	model_PerFrameData.fogend_n_fogactive_n_lightdebug = glm::vec4(gHUD.m_pFogSettings.end, gHUD.m_pFogSettings.active, m_pCvarStudioModelLightDebug->value, 0);

	pModel_PerFrameBuffer->Bind(GL_BufferHandler::UniformBuffer);
	pModel_PerFrameBuffer->BufferSubData(GL_BufferHandler::UniformBuffer, 0, sizeof(model_PerFrameData), &model_PerFrameData);
}

static int gl_bonearrayoffset = 0;
static std::vector<float> gl_bonetransforms;

inline size_t BoneData_Align(size_t value)
{
	return (value + GL_ShaderProgram::GetDriverUBOAlignment()  - 1) & ~(GL_ShaderProgram::GetDriverUBOAlignment() - 1);
}

void InsertBones(const matrix3x4_t* bones, int numbones)
{
	size_t boneDataSize = numbones * sizeof(matrix3x4_t);

	gl_bonearrayoffset = BoneData_Align(gl_bonearrayoffset);

	if (gl_bonearrayoffset > gl_bonetransforms.size() * sizeof(float))
	{
		size_t padBytes = gl_bonearrayoffset - gl_bonetransforms.size() * sizeof(float);
		size_t padMatrices = (padBytes + sizeof(float) - 1) / sizeof(float);
		gl_bonetransforms.resize(gl_bonetransforms.size() + padMatrices);
	}

	m_pCurrentStudioEntData->bonearrayoffset = gl_bonearrayoffset;

	gl_bonetransforms.resize(gl_bonetransforms.size() + (numbones * 3 * 4));
	memcpy(gl_bonetransforms.data() + (gl_bonearrayoffset / sizeof(float)),
		bones,
		boneDataSize);

	gl_bonearrayoffset += boneDataSize;
}

void CStudioModelRenderer::StudioClearDrawList()
{
	vStudioDrawList.clear();
	gl_bonetransforms.clear();
	gl_bonearrayoffset = 0;
}

void CStudioModelRenderer::StudioUploadRenderData()
{
	StudioSetupViewmodel();

#if _DEBUG
	if ((gl_bonetransforms.size() * sizeof(matrix3x4_t)) > pModel_BonesBuffer->GetBufferSize())
		gEngfuncs.Con_Printf("jesus h christ! more than 262 thousand bones are visible on screen! that's about 2048 entities with 128 bones each ! what are you doing ?\n");
#endif

	pModel_BonesBuffer->Bind(GL_BufferHandler::UniformBuffer);
	//send all bone data on the scene in one single upload
	pModel_BonesBuffer->BufferSubData(GL_BufferHandler::UniformBuffer, 0, V_min(gl_bonetransforms.size() * sizeof(float), 128 * 2048), gl_bonetransforms.data());

	GL_BufferHandler::ResetBufferBinding(GL_BufferHandler::UniformBuffer);
}

void CStudioModelRenderer::StudioSetupViewmodel()
{
	SetCurrentEntity(&engine_cl->viewent);

	if (!m_pCurrentEntity->model || !m_pCvarDrawViewmodel->value)
		return;

	SetCurrentMDL(m_pCurrentEntity->model);
	
	CheckVMProjection();

	if (!engine_cl->weaponstarttime)
		engine_cl->weaponstarttime = engine_cl->time;

	m_pCurrentEntity->curstate.framerate = 1.0f;
	m_pCurrentEntity->curstate.sequence = engine_cl->weaponsequence;
	m_pCurrentEntity->curstate.animtime = engine_cl->weaponstarttime;

	for (int i = 0; i < 4; i++)
	{
		VectorCopy(m_pCurrentEntity->origin, m_pCurrentEntity->attachment[i]);
	}

	StudioSetUpTransform(0);
	StudioSetupBones();

	m_pCurrentStudioEntData->rotationmatrix = (*m_protationmatrix);

	memcpy(m_pCurrentStudioEntData->bonematrix, (*m_pbonetransform), sizeof(matrix3x4_t) * m_pStudioHeader->numbones);

	InsertBones((*m_pbonetransform), m_pStudioHeader->numbones);
}

void CStudioModelRenderer::StudioPushEntityToDraw(cl_entity_s* pEnt)
{
	//every entity that gets passed to this function is guaranteed to have a model

	if (m_pCvarDrawStudioModels->value < 1)
		return;

	SetCurrentEntity(pEnt);
	SetCurrentMDL(pEnt->model);
	if (!m_pCurrentStudioMDL)
		return;

	if (IsEntityTransparent(m_pCurrentEntity) && m_pCurrentEntity->curstate.renderamt == NULL)
		return;

	entity_state_t* pplayer = nullptr;

	if (m_pCurrentEntity->player && m_pCurrentEntity->index == (engine_cl->playernum + 1))
	{
		pplayer = R_StudioGetPlayerState((m_pCurrentEntity->index - 1));

		nPlayerIndex = pplayer->number - 1;

		if (nPlayerIndex < 0 || nPlayerIndex >= engine_cl->maxclients)
			return;

		if (m_pCurrentEntity->index == (engine_cl->playernum + 1))
		{
			VectorCopy(engine_cl->simorg, m_pCurrentEntity->origin);

			VectorCopy(m_pCurrentEntity->origin, m_pCurrentEntity->attachment[0]);
			VectorCopy(m_pCurrentEntity->origin, m_pCurrentEntity->attachment[1]);
			VectorCopy(m_pCurrentEntity->origin, m_pCurrentEntity->attachment[2]);
			VectorCopy(m_pCurrentEntity->origin, m_pCurrentEntity->attachment[3]);
		}

		pPlayerInfo = pfnPlayerInfo(nPlayerIndex);
		pPlayerInfo->renderframe = gBSPRenderer.m_iFrameCount;

		if (pplayer->gaitsequence)
		{
			Vector orig_angles;

			VectorCopy(m_pCurrentEntity->angles, orig_angles);

			StudioProcessGait(pplayer);

			StudioSetUpTransform(0);
			StudioSetupBones();

			pPlayerInfo->gaitsequence = pplayer->gaitsequence;
			pPlayerInfo = NULL;

			VectorCopy(orig_angles, m_pCurrentEntity->angles);
		}
		else
		{
			memset(m_pCurrentEntity->curstate.controller, 127, 4);
			memcpy(m_pCurrentEntity->latched.prevcontroller, m_pCurrentEntity->curstate.controller, 4);

			pPlayerInfo->gaitsequence = 0;

			StudioSetUpTransform(0);
			StudioSetupBones();
		}

		StudioSaveBones();
	}
	else
	{
		StudioSetUpTransform(0);
		StudioSetupBones();
	}

	m_pCurrentStudioEntData->rotationmatrix = (*m_protationmatrix);
	memcpy(m_pCurrentStudioEntData->bonematrix, (*m_pbonetransform), sizeof(matrix3x4_t) * m_pStudioHeader->numbones);

	// new: static bone buffer. holds enough space for 2048 skeletons with 128 bones (around 12 mb in gpu). much better than
	// constantly uploading bone data for every single entity
	// CLEAN THIS UP !!!

	InsertBones((*m_pbonetransform), m_pStudioHeader->numbones);

	if (pplayer && pplayer->weaponmodel)
	{
		model_t* savedmdl = m_pRenderModel;

		model_t* pweaponmodel = CL_GetModelByIndex(pplayer->weaponmodel);
		SetCurrentMDL(pweaponmodel);

		m_pCurrentEntity->model = pweaponmodel;

		StudioMergeBones(pweaponmodel);

		StudioCalcAttachments();

		m_pCurrentEntity->model = savedmdl;
	}


	vStudioDrawList[m_pCurrentEntity->model].push_back(m_pCurrentEntity);
}


/*
====================
StudioDrawModels

====================
*/
void CStudioModelRenderer::StudioDrawModels(bool bDrawLocalPlayer)
{
	if (vStudioDrawList.empty() || !m_pCvarDrawStudioModels->value)
		return;

	bExternalEntity = false;

	s_MDLShader.Bind();

	g_GlobalGLState.SetBlend(false);

#if defined(GL_ONE_BIG_BUFFER_FOR_STUDIOMDLS)
	StudioMDL_Model::BindGlobalMesh();
#endif

 	for(const auto& model : vStudioDrawList)
	{
		SetCurrentMDL(model.first);
		for (int i = 0; i < model.second.size(); i++)
		{
			SetCurrentEntity(model.second[i]);
			if (m_pCurrentEntity->player)
			{
				if (m_pCurrentEntity == gEngfuncs.GetLocalPlayer() && !bDrawLocalPlayer)
					continue;

				StudioDrawPlayer(STUDIO_RENDER, R_StudioGetPlayerState((m_pCurrentEntity->index - 1)));
			}
			else
				StudioDrawModel(STUDIO_RENDER);
		}
	}

#if defined(GL_ONE_BIG_BUFFER_FOR_STUDIOMDLS)
	StudioMDL_Model::UnbindGlobalMesh();
#endif

	GL_ShaderProgram::ResetShaderBind();
}

viewinfo_s g_viewinfo;

/*
====================
StudioDrawModels

====================
*/
void CStudioModelRenderer::StudioDrawViewmodel()
{
	g_viewinfo.phdr = nullptr;

	if (!engine_cl->viewent.model)
		return;

	if (!m_pCvarDrawStudioModels->value || !m_pCvarDrawViewmodel->value)
		return;

	SetCurrentEntity(&engine_cl->viewent);
	SetCurrentMDL(engine_cl->viewent.model);

	bExternalEntity = false;

	s_MDLShader.Bind();
	pModel_BonesBuffer->Bind(GL_BufferHandler::UniformBuffer);
	pModel_PerEntityBuffer->Bind(GL_BufferHandler::UniformBuffer);
	pModel_PerFrameBuffer->Bind(GL_BufferHandler::UniformBuffer);

	s_MDLShader.Uniform1i(eModel_ShaderLocs[mdlshader_viewmodel], 1);

	g_GlobalGLState.SetBlend(false);

	glClear(GL_DEPTH_BUFFER_BIT);

#if !defined(GL_ONE_BIG_BUFFER_FOR_STUDIOMDLS)
	m_pCurrentStudioMDL->BindMesh();
#else
	StudioMDL_Model::BindGlobalMesh();
#endif

	m_pCurrentStudioEntData = (studioentity_data_t*)m_pCurrentEntity->efrag;
	m_pStudioHeader = (studiohdr_t*)m_pCurrentEntity->model->cache.data;

	StudioDrawModel(STUDIO_RENDER | STUDIO_EVENTS);

#if !defined(GL_ONE_BIG_BUFFER_FOR_STUDIOMDLS)
	StudioMDL_Model::UnbindGlobalMesh();
#endif

	s_MDLShader.Uniform1i(eModel_ShaderLocs[mdlshader_viewmodel], 0);


	//camera anim stuff

	//g_viewinfo.phdr = m_pStudioHeader;
	//
	//
	//gHUD.m_prevstate.sequence = m_pCurrentEntity->curstate.sequence;
	//cl_entity_s temp = engine_cl->viewent;
	//temp.angles = temp.curstate.angles = temp.origin = temp.curstate.origin = Vector(0, 0, 0);
	//m_pCurrentEntity = &temp;
	//m_pRenderModel = m_pCurrentEntity->model;
	//m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(m_pRenderModel);
	//
	//StudioSetUpTransform(false);
	//StudioSetupBones();
	//for (int i = 0; i < m_pStudioHeader->numbones; i++)
	//{
	//	MatrixAngles((*m_pbonetransform)[i], g_viewinfo.boneangles[i], g_viewinfo.bonepos[i]);
	//	NormalizeAngles((float*)&g_viewinfo.boneangles[i]);
	//}
	//temp = engine_cl->viewent;
	//temp.angles = temp.curstate.angles = temp.origin = temp.curstate.origin = Vector(0, 0, 0);
	//temp.curstate.sequence = 0;
	//temp.curstate.frame = 0;
	//temp.curstate.animtime = 0;
	//temp.latched.prevframe = 0;
	//m_pCurrentEntity = &temp;
	//
	//StudioSetUpTransform(false);
	//StudioSetupBones();
	//for (int i = 0; i < m_pStudioHeader->numbones; i++)
	//{
	//	MatrixAngles((*m_pbonetransform)[i], g_viewinfo.prevboneangles[i], g_viewinfo.prevbonepos[i]);
	//	NormalizeAngles((float*)&g_viewinfo.prevboneangles[i]);
	//}

	SetCurrentEntity(nullptr);
}

/*
====================
StudioDrawModels

====================
*/
void CStudioModelRenderer::StudioDrawModelsSolid()
{
	if (vStudioDrawList.empty() || !m_pCvarDrawStudioModels->value)
		return;

	bExternalEntity = false;
	model_SolidData.projviewmatrix = gBSPRenderer.m_ProjectionMatrix * gBSPRenderer.m_ViewMatrix;
	model_SolidData.modelmatrix = gBSPRenderer.m_ModelMatrix;

	cl_dlight_t* dynl = gBSPRenderer.m_pCurrentDynLight;
	model_SolidData.light_pos = glm::vec4(dynl->origin.x, dynl->origin.y, dynl->origin.z, dynl->radius);

	s_MDLSolidShader.Bind();
	pModel_SolidBuffer->Bind(GL_BufferHandler::UniformBuffer);
	pModel_SolidBuffer->BindRange(GL_BufferHandler::UniformBuffer, s_MDLSolidShader.GetUBOIndex("StudioSolidUBO"), 0, sizeof(mdlshadersolid_data_t));
	pModel_SolidBuffer->BufferSubData(GL_BufferHandler::UniformBuffer, 0, sizeof(mdlshadersolid_data_t), &model_SolidData);


	if (gBSPRenderer.m_bSunShadowMapPass)
	{
		// flip depth because shadow pixels fade from ground and yada yada
		s_MDLSolidShader.Uniform1i(eModel_ShaderSolidLocs[mdlshadersolid_sunshadow], gBSPRenderer.m_bSunShadowMapPass);
		glCullFace(GL_BACK);
	}

#if defined(GL_ONE_BIG_BUFFER_FOR_STUDIOMDLS)
	StudioMDL_Model::BindGlobalMesh();
#endif

	for (auto& model : vStudioDrawList)
	{
		SetCurrentMDL(model.first);
		for (auto ents : model.second)
		{
			SetCurrentEntity(ents);
			if (ents->player)
			{
				if (m_pCurrentEntity == gEngfuncs.GetLocalPlayer()) //no player shadows according to team's directions
					continue;

				StudioDrawPlayerSolid(R_StudioGetPlayerState((ents->index - 1)));
			}
			else
				StudioDrawModelSolid();
		}
	}

#if defined(GL_ONE_BIG_BUFFER_FOR_STUDIOMDLS)
	StudioMDL_Model::UnbindGlobalMesh();
#endif

	if (gBSPRenderer.m_bSunShadowMapPass)
	{
		// unflip
		s_MDLSolidShader.Uniform1i(eModel_ShaderSolidLocs[mdlshadersolid_sunshadow], 0);
		glCullFace(GL_FRONT);
	}
}

/*
====================
StudioSetUpTransform

====================
*/
void CStudioModelRenderer::StudioSetUpTransform(int trivial_accept)
{
	int i;
	Vector angles;
	Vector modelpos;


	VectorCopy(m_pCurrentEntity->origin, modelpos);

	// TODO: should really be stored with the entity instead of being reconstructed
	// TODO: should use a look-up table
	// TODO: could cache lazily, stored in the entity
	angles[ROLL] = m_pCurrentEntity->curstate.angles[ROLL];
	angles[PITCH] = m_pCurrentEntity->curstate.angles[PITCH];
	angles[YAW] = m_pCurrentEntity->curstate.angles[YAW];

	// Con_DPrintf("Angles %4.2f prev %4.2f for %i\n", angles[PITCH], m_pCurrentEntity->index);
	// Con_DPrintf("movetype %d %d\n", m_pCurrentEntity->movetype, m_pCurrentEntity->aiment );
	if (m_pCurrentEntity->curstate.movetype == MOVETYPE_STEP)
	{
		float f = 0;
		float d;

		mstudioseqdesc_t* pseqdesc; // acess to studio flags
		pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

		// don't do it if the goalstarttime hasn't updated in a while.

		// NOTE:  Because we need to interpolate multiplayer characters, the interpolation time limit
		//  was increased to 1.0 s., which is 2x the max lag we are accounting for.

		if ((engine_cl->time < m_pCurrentEntity->curstate.animtime + 1.0f) &&
			(m_pCurrentEntity->curstate.animtime != m_pCurrentEntity->latched.prevanimtime))
		{
			f = (engine_cl->time - m_pCurrentEntity->curstate.animtime) / (m_pCurrentEntity->curstate.animtime - m_pCurrentEntity->latched.prevanimtime);
			// Con_DPrintf("%4.2f %.2f %.2f\n", f, m_pCurrentEntity->curstate.animtime, m_clTime);
		}

		if (bDoInterp)
		{
			// ugly hack to interpolate angle, position. current is reached 0.1 seconds after being set
			f = f - 1.0;
		}
		else
		{
			f = 0;
		}

		if (pseqdesc->motiontype & STUDIO_LX || m_pCurrentEntity->curstate.eflags & EFLAG_SLERP) // uncle misha
		{
			for (i = 0; i < 3; i++)
			{
				modelpos[i] += (m_pCurrentEntity->origin[i] - m_pCurrentEntity->latched.prevorigin[i]) * f;
			}
		}

		// NOTE:  Because multiplayer lag can be relatively large, we don't want to cap
		//  f at 1.5 anymore.
		// if (f > -1.0 && f < 1.5) {}

		//			gEngfuncs.Con_DPrintf("%.0f %.0f\n",m_pCurrentEntity->angles[0][YAW], m_pCurrentEntity->angles[1][YAW] );
		for (i = 0; i < 3; i++)
		{
			float ang1, ang2;

			ang1 = m_pCurrentEntity->angles[i];
			ang2 = m_pCurrentEntity->latched.prevangles[i];

			d = ang1 - ang2;
			if (d > 180)
			{
				d -= 360;
			}
			else if (d < -180)
			{
				d += 360;
			}

			angles[i] += d * f;
		}
		// Con_DPrintf("%.3f \n", f );
	}
	else if (m_pCurrentEntity->curstate.movetype != MOVETYPE_NONE)
	{
		VectorCopy(m_pCurrentEntity->angles, angles);
	}

	// Con_DPrintf("%.0f %0.f %0.f\n", modelpos[0], modelpos[1], modelpos[2] );
	//	gEngfuncs.Con_DPrintf("%.0f %0.f %0.f\n", angles[0], angles[1], angles[2] );


	angles[PITCH] = -angles[PITCH];
	AngleMatrix(angles, (*m_protationmatrix));

	(*m_protationmatrix)[0][3] = modelpos[0];
	(*m_protationmatrix)[1][3] = modelpos[1];
	(*m_protationmatrix)[2][3] = modelpos[2];
}


/*
====================
StudioEstimateInterpolant

====================
*/
float CStudioModelRenderer::StudioEstimateInterpolant(void)
{
	float dadt = 1.0;

	if (bDoInterp && (m_pCurrentEntity->curstate.animtime >= m_pCurrentEntity->latched.prevanimtime + 0.01))
	{
		dadt = (engine_cl->time - m_pCurrentEntity->curstate.animtime) / 0.1;
		if (dadt > 2.0)
		{
			dadt = 2.0;
		}
	}
	return dadt;
}

/*
====================
StudioCalcRotations

====================
*/
void CStudioModelRenderer::StudioCalcRotations(float pos[][3], vec4_t* q, mstudioseqdesc_t* pseqdesc, mstudioanim_t* panim, float f)
{
	int i;
	int frame;
	mstudiobone_t* pbone;

	float s;
	float adj[MAXSTUDIOCONTROLLERS];
	float dadt;

	if (f > pseqdesc->numframes - 1)
	{
		f = 0; // bah, fix this bug with changing sequences too fast
	}
	// BUG ( somewhere else ) but this code should validate this data.
	// This could cause a crash if the frame # is negative, so we'll go ahead
	//  and clamp it here
	else if (f < -0.01)
	{
		f = -0.01;
	}

	frame = (int)f;

	dadt = StudioEstimateInterpolant();
	s = (f - frame);

	// add in programtic controllers
	pbone = (mstudiobone_t*)((byte*)m_pStudioHeader + m_pStudioHeader->boneindex);

	StudioCalcBoneAdj(dadt, adj, m_pCurrentEntity->curstate.controller, m_pCurrentEntity->latched.prevcontroller, m_pCurrentEntity->mouth.mouthopen);

	for (i = 0; i < m_pStudioHeader->numbones; i++, pbone++, panim++)
	{
		StudioCalcBoneQuaterion(frame, s, pbone, panim, adj, q[i]);

		StudioCalcBonePosition(frame, s, pbone, panim, adj, pos[i]);
	}

	if (pseqdesc->motiontype & STUDIO_X)
	{
		pos[pseqdesc->motionbone][0] = 0.0;
	}
	if (pseqdesc->motiontype & STUDIO_Y)
	{
		pos[pseqdesc->motionbone][1] = 0.0;
	}
	if (pseqdesc->motiontype & STUDIO_Z)
	{
		pos[pseqdesc->motionbone][2] = 0.0;
	}

	s = 0 * ((1.0 - (f - (int)(f))) / (pseqdesc->numframes)) * m_pCurrentEntity->curstate.framerate;

	if (pseqdesc->motiontype & STUDIO_LX)
	{
		pos[pseqdesc->motionbone][0] += s * pseqdesc->linearmovement[0];
	}
	if (pseqdesc->motiontype & STUDIO_LY)
	{
		pos[pseqdesc->motionbone][1] += s * pseqdesc->linearmovement[1];
	}
	if (pseqdesc->motiontype & STUDIO_LZ)
	{
		pos[pseqdesc->motionbone][2] += s * pseqdesc->linearmovement[2];
	}
}

/*
====================
Studio_FxTransform

====================
*/
void CStudioModelRenderer::StudioFxTransform(cl_entity_t* ent, matrix3x4_t &transform)
{
	if (ent->curstate.renderfx != kRenderFxExplode && ent->curstate.scale > 0)
	{
		transform[0][0] *= ent->curstate.scale;
		transform[1][0] *= ent->curstate.scale;
		transform[2][0] *= ent->curstate.scale;

		transform[0][1] *= ent->curstate.scale;
		transform[1][1] *= ent->curstate.scale;
		transform[2][1] *= ent->curstate.scale;

		transform[0][2] *= ent->curstate.scale;
		transform[1][2] *= ent->curstate.scale;
		transform[2][2] *= ent->curstate.scale;
	}

	switch (ent->curstate.renderfx)
	{
	case kRenderFxDistort:
	case kRenderFxHologram:
		if (gEngfuncs.pfnRandomLong(0, 49) == 0)
		{
			int axis = gEngfuncs.pfnRandomLong(0, 1);
			if (axis == 1) // Choose between x & z
				axis = 2;
			VectorScale(transform[axis], gEngfuncs.pfnRandomFloat(1, 1.484), transform[axis]);
		}
		else if (gEngfuncs.pfnRandomLong(0, 49) == 0)
		{
			float offset;
			int axis = gEngfuncs.pfnRandomLong(0, 1);
			if (axis == 1) // Choose between x & z
				axis = 2;
			offset = gEngfuncs.pfnRandomFloat(-10, 10);
			transform[gEngfuncs.pfnRandomLong(0, 2)][3] += offset;
		}
		break;
	case kRenderFxExplode:
	{
		float scale;

		scale = 1.0 + (engine_cl->time - ent->curstate.animtime) * 10.0;
		if (scale > 2) // Don't blow up more than 200%
			scale = 2;
		transform[0][1] *= scale;
		transform[1][1] *= scale;
		transform[2][1] *= scale;
	}
	break;
	}
}

/*
====================
StudioEstimateFrame

====================
*/
float CStudioModelRenderer::StudioEstimateFrame(mstudioseqdesc_t* pseqdesc)
{
	double dfdt, f;

	if (bDoInterp)
	{
		if (engine_cl->time < m_pCurrentEntity->curstate.animtime)
		{
			dfdt = 0;
		}
		else
		{
			dfdt = (engine_cl->time - m_pCurrentEntity->curstate.animtime) * m_pCurrentEntity->curstate.framerate * pseqdesc->fps;
		}
	}
	else
	{
		dfdt = 0;
	}

	if (pseqdesc->numframes <= 1)
	{
		f = 0;
	}
	else
	{
		f = (m_pCurrentEntity->curstate.frame * (pseqdesc->numframes - 1)) / 256.0;
	}

	f += dfdt;

	if (pseqdesc->flags & STUDIO_LOOPING)
	{
		if (pseqdesc->numframes > 1)
		{
			f -= (int)(f / (pseqdesc->numframes - 1)) * (pseqdesc->numframes - 1);
		}
		if (f < 0)
		{
			f += (pseqdesc->numframes - 1);
		}
	}
	else
	{
		if (f >= pseqdesc->numframes - 1.001)
		{
			f = pseqdesc->numframes - 1.001;
		}
		if (f < 0.0)
		{
			f = 0.0;
		}
	}
	return f;
}

/*
====================
StudioSetupBones

====================
*/
void CStudioModelRenderer::StudioSetupBones(void)
{
	int i;
	double f;

	mstudiobone_t* pbones;
	mstudioseqdesc_t* pseqdesc;
	mstudioanim_t* panim;

	static float pos[MAXSTUDIOBONES][3];
	static vec4_t q[MAXSTUDIOBONES];
	matrix3x4_t bonematrix;

	static float pos2[MAXSTUDIOBONES][3];
	static vec4_t q2[MAXSTUDIOBONES];
	static float pos3[MAXSTUDIOBONES][3];
	static vec4_t q3[MAXSTUDIOBONES];
	static float pos4[MAXSTUDIOBONES][3];
	static vec4_t q4[MAXSTUDIOBONES];

	if (m_pCurrentEntity->curstate.sequence >= m_pStudioHeader->numseq || m_pCurrentEntity->curstate.sequence < 0)
	{
		m_pCurrentEntity->curstate.sequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

	f = StudioEstimateFrame(pseqdesc);

	if (m_pCurrentEntity->latched.prevframe > f)
	{
		// Con_DPrintf("%f %f\n", m_pCurrentEntity->prevframe, f );
	}

	panim = StudioGetAnim(m_pRenderModel, pseqdesc);
	StudioCalcRotations(pos, q, pseqdesc, panim, f);

	if (pseqdesc->numblends > 1)
	{
		float s;
		float dadt;

		panim += m_pStudioHeader->numbones;
		StudioCalcRotations(pos2, q2, pseqdesc, panim, f);

		dadt = StudioEstimateInterpolant();
		s = (m_pCurrentEntity->curstate.blending[0] * dadt + m_pCurrentEntity->latched.prevblending[0] * (1.0 - dadt)) / 255.0;

		StudioSlerpBones(q, pos, q2, pos2, s);

		if (pseqdesc->numblends == 4)
		{
			panim += m_pStudioHeader->numbones;
			StudioCalcRotations(pos3, q3, pseqdesc, panim, f);

			panim += m_pStudioHeader->numbones;
			StudioCalcRotations(pos4, q4, pseqdesc, panim, f);

			s = (m_pCurrentEntity->curstate.blending[0] * dadt + m_pCurrentEntity->latched.prevblending[0] * (1.0 - dadt)) / 255.0;
			StudioSlerpBones(q3, pos3, q4, pos4, s);

			s = (m_pCurrentEntity->curstate.blending[1] * dadt + m_pCurrentEntity->latched.prevblending[1] * (1.0 - dadt)) / 255.0;
			StudioSlerpBones(q, pos, q3, pos3, s);
		}
	}

	if (bDoInterp &&
		m_pCurrentEntity->latched.sequencetime &&
		(m_pCurrentEntity->latched.sequencetime + 0.2 > engine_cl->time) &&
		(m_pCurrentEntity->latched.prevsequence < m_pStudioHeader->numseq))
	{
		// blend from last sequence
		static float pos1b[MAXSTUDIOBONES][3];
		static vec4_t q1b[MAXSTUDIOBONES];
		float s;

		pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->latched.prevsequence;
		panim = StudioGetAnim(m_pRenderModel, pseqdesc);
		// clip prevframe
		StudioCalcRotations(pos1b, q1b, pseqdesc, panim, m_pCurrentEntity->latched.prevframe);

		if (pseqdesc->numblends > 1)
		{
			panim += m_pStudioHeader->numbones;
			StudioCalcRotations(pos2, q2, pseqdesc, panim, m_pCurrentEntity->latched.prevframe);

			s = (m_pCurrentEntity->latched.prevseqblending[0]) / 255.0;
			StudioSlerpBones(q1b, pos1b, q2, pos2, s);

			if (pseqdesc->numblends == 4)
			{
				panim += m_pStudioHeader->numbones;
				StudioCalcRotations(pos3, q3, pseqdesc, panim, m_pCurrentEntity->latched.prevframe);

				panim += m_pStudioHeader->numbones;
				StudioCalcRotations(pos4, q4, pseqdesc, panim, m_pCurrentEntity->latched.prevframe);

				s = (m_pCurrentEntity->latched.prevseqblending[0]) / 255.0;
				StudioSlerpBones(q3, pos3, q4, pos4, s);

				s = (m_pCurrentEntity->latched.prevseqblending[1]) / 255.0;
				StudioSlerpBones(q1b, pos1b, q3, pos3, s);
			}
		}

		s = 1.0 - (engine_cl->time - m_pCurrentEntity->latched.sequencetime) / 0.2;
		StudioSlerpBones(q, pos, q1b, pos1b, s);
	}
	else
	{
		// Con_DPrintf("prevframe = %4.2f\n", f);
		m_pCurrentEntity->latched.prevframe = f;
	}

	pbones = (mstudiobone_t*)((byte*)m_pStudioHeader + m_pStudioHeader->boneindex);

	// calc gait animation
	if (pPlayerInfo && pPlayerInfo->gaitsequence != 0)
	{
		pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + pPlayerInfo->gaitsequence;

		panim = StudioGetAnim(m_pRenderModel, pseqdesc);
		StudioCalcRotations(pos2, q2, pseqdesc, panim, pPlayerInfo->gaitframe);

		for (i = 0; i < m_pStudioHeader->numbones; i++)
		{
			if (strcmp(pbones[i].name, "Bip01 Spine") == 0)
				break;
			memcpy(pos[i], pos2[i], sizeof(pos[i]));
			memcpy(q[i], q2[i], sizeof(q[i]));
		}
	}


	for (i = 0; i < m_pStudioHeader->numbones; i++)
	{
		QuaternionMatrix(q[i], bonematrix);

		bonematrix[0][3] = pos[i][0];
		bonematrix[1][3] = pos[i][1];
		bonematrix[2][3] = pos[i][2];

		if (pbones[i].parent == -1)
		{
			ConcatTransforms((*m_protationmatrix), bonematrix, (*m_pbonetransform)[i]);

			// Apply client-side effects to the transformation matrix
			StudioFxTransform(m_pCurrentEntity, (*m_pbonetransform)[i]);
		}
		else
		{
			ConcatTransforms((*m_pbonetransform)[pbones[i].parent], bonematrix, (*m_pbonetransform)[i]);
		}
	}
}


/*
====================
StudioSaveBones

====================
*/
void CStudioModelRenderer::StudioSaveBones(void)
{
	int i;

	mstudiobone_t* pbones;
	pbones = (mstudiobone_t*)((byte*)m_pStudioHeader + m_pStudioHeader->boneindex);

	m_nCachedBones = m_pStudioHeader->numbones;

	for (i = 0; i < m_pStudioHeader->numbones; i++)
	{
		strcpy(m_nCachedBoneNames[i], pbones[i].name);
		m_rgCachedBoneTransform[i] = (*m_pbonetransform)[i];
	}
}


/*
====================
StudioMergeBones

====================
*/
void CStudioModelRenderer::StudioMergeBones(model_t* m_pSubModel)
{
	int i, j;
	double f;
	int do_hunt = true;

	mstudiobone_t* pbones;
	mstudioseqdesc_t* pseqdesc;
	mstudioanim_t* panim;

	static float pos[MAXSTUDIOBONES][3];
	matrix3x4_t bonematrix;
	static vec4_t q[MAXSTUDIOBONES];

	m_pCurrentEntity->curstate.sequence = std::clamp(m_pCurrentEntity->curstate.sequence, 0, m_pStudioHeader->numseq);

	pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

	f = StudioEstimateFrame(pseqdesc);

	if (m_pCurrentEntity->latched.prevframe > f)
	{
		// Con_DPrintf("%f %f\n", m_pCurrentEntity->prevframe, f );
	}

	panim = StudioGetAnim(m_pSubModel, pseqdesc);
	StudioCalcRotations(pos, q, pseqdesc, panim, f);

	pbones = (mstudiobone_t*)((byte*)m_pStudioHeader + m_pStudioHeader->boneindex);


	for (i = 0; i < m_pStudioHeader->numbones; i++)
	{
		for (j = 0; j < m_nCachedBones; j++)
		{
			if (stricmp(pbones[i].name, m_nCachedBoneNames[j]) == 0)
			{
				(*m_pbonetransform)[i] = m_rgCachedBoneTransform[j];
				break;
			}
		}
		if (j >= m_nCachedBones)
		{
			QuaternionMatrix(q[i], bonematrix);

			bonematrix[0][3] = pos[i][0];
			bonematrix[1][3] = pos[i][1];
			bonematrix[2][3] = pos[i][2];

			if (pbones[i].parent == -1)
			{
				ConcatTransforms((*m_protationmatrix), bonematrix, (*m_pbonetransform)[i]);

				// Apply client-side effects to the transformation matrix
				StudioFxTransform(m_pCurrentEntity, (*m_pbonetransform)[i]);
			}
			else
			{
				ConcatTransforms((*m_pbonetransform)[pbones[i].parent], bonematrix, (*m_pbonetransform)[i]);
			}
		}
	}
}

/*
====================
StudioCalcAttachments

====================
*/
void CStudioModelRenderer::StudioClientEvents()
{
	mstudioseqdesc_t* pseqdesc;
	mstudioevent_t* pevent;
	cl_entity_t* e = m_pCurrentEntity;
	int i, sequence;
	float end, start;

	sequence = std::clamp(e->curstate.sequence, 0, m_pStudioHeader->numseq - 1);
	pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + sequence;

	// no events for this animation
	if (pseqdesc->numevents == 0)
		return;

	end = StudioEstimateFrame(pseqdesc);
	start = end - e->curstate.framerate * (engine_cl->time - engine_cl->oldtime) * pseqdesc->fps;
	pevent = (mstudioevent_t*)((byte*)m_pStudioHeader + pseqdesc->eventindex);

	if (e->latched.sequencetime == e->curstate.animtime)
	{
		if ((pseqdesc->flags & STUDIO_LOOPING) != 0)
			start = -0.01f;
	}

	for (i = 0; i < pseqdesc->numevents; i++)
	{
		// ignore all non-client-side events
		// if (pevent[i].event < 5000)
		//	continue;

		if ((float)pevent[i].frame > start && pevent[i].frame <= end)
			HUD_StudioEvent(&pevent[i], e);
	}
}

/*
====================
StudioDrawModel

====================
*/
void CStudioModelRenderer::StudioDrawModel(int flags)
{
	if (m_pCurrentEntity->curstate.renderfx == kRenderFxDeadPlayer)
	{
		if (m_pCurrentEntity->curstate.renderamt <= 0 || m_pCurrentEntity->curstate.renderamt > engine_cl->maxclients)
			return;

		StudioHandleDeadPlayer(flags);
		return;
	}

	(*m_protationmatrix) = m_pCurrentStudioEntData->rotationmatrix;

	if (flags & STUDIO_RENDER)
	{
		if (m_pStudioHeader->numbodyparts == 0)
			return;

		// see if the bounding box lets us trivially reject
		if (StudioCheckBBox())
			return;
	}

	//enable buffers here so we dont bind buffers of models we won't draw
#if !defined(GL_ONE_BIG_BUFFER_FOR_STUDIOMDLS)
	if (!m_pCurrentStudioMDL->IsMeshBound())
		m_pCurrentStudioMDL->BindMesh();
#endif

	//StudioSetupBones();
	memcpy((*m_pbonetransform), m_pCurrentStudioEntData->bonematrix, sizeof(matrix3x4_t) * m_pStudioHeader->numbones);

	pModel_BonesBuffer->Bind(GL_BufferHandler::UniformBuffer);
	pModel_BonesBuffer->BindRange(GL_BufferHandler::UniformBuffer, s_MDLShader.GetUBOIndex("BonesUBO"), m_pCurrentStudioEntData->bonearrayoffset, sizeof(matrix3x4_t) * m_pStudioHeader->numbones);

	if (flags & STUDIO_EVENTS)
	{
		StudioCalcAttachments();
		StudioClientEvents();

		// copy attachments into global entity array
		if (m_pCurrentEntity->index > 0)
		{
			cl_entity_t* ent = gEngfuncs.GetEntityByIndex(m_pCurrentEntity->index);
			memcpy(ent->attachment, m_pCurrentEntity->attachment, sizeof(Vector) * 4);
		}
	}

	StudioSetupLighting();
	StudioEntityLight();

	if (flags & STUDIO_RENDER && !(m_pCurrentEntity->curstate.effects & FL_NOMODEL))
	{
		StudioRenderModel();
	}

	return;
}

void CStudioModelRenderer::StudioHandleDeadPlayer(int flags)
{
	entity_state_t deadplayer;
	bool save_interp;

	// get copy of player
	deadplayer = *(R_StudioGetPlayerState(m_pCurrentEntity->curstate.renderamt - 1));

	// clear weapon, movement state
	deadplayer.number = m_pCurrentEntity->curstate.renderamt;
	deadplayer.weaponmodel = 0;
	deadplayer.gaitsequence = 0;

	deadplayer.movetype = MOVETYPE_NONE;
	deadplayer.angles = m_pCurrentEntity->curstate.angles;
	deadplayer.origin = m_pCurrentEntity->curstate.origin;

	bDoInterp = false;

	// draw as though it were a player
	StudioDrawPlayer(flags, &deadplayer);

	bDoInterp = true; //bDoInterp is always on
}

void CStudioModelRenderer::StudioFreeEntity()
{
	studioentity_data_t* data = (studioentity_data_t*)m_pCurrentEntity->efrag;
	if (!data)
		return;

	assert(data->entity_index == m_pCurrentEntity->index);
	
	for (auto& studiodecal : data->m_vStudioDecals)
	{
		if (studiodecal->numpolys)
		{
			for (int j = 0; j < studiodecal->numpolys; j++)
				delete[] studiodecal->polys[j].verts;

			delete[] studiodecal->polys;
		}

		if (studiodecal->numverts)
		{
			iNumStudioDecalVerts -= studiodecal->numverts;
			delete[] studiodecal->verts;
		}

		for (int i = 0; i < pStudioDecals.size(); i++)
		{
			studiodecal_t* decal = pStudioDecals[i].get();
			if (decal == studiodecal)
			{
				pStudioDecals.erase(pStudioDecals.begin() + i);
				break;
			}
		}
	}

	data->m_vStudioDecals.clear();

	for (int i = 0; i < pStudioEntityData.size(); i++)
	{
		studioentity_data_t* entdata = pStudioEntityData[i].get();
		if(entdata == data)
		{
			pStudioEntityData.erase(pStudioEntityData.begin() + i);
			break;
		}

	}

	m_pCurrentEntity->efrag = nullptr;

}

/*
====================
StudioEstimateGait

====================
*/
void CStudioModelRenderer::StudioEstimateGait(entity_state_t* pplayer)
{
	float dt;
	Vector est_velocity;

	dt = (engine_cl->time - engine_cl->oldtime);
	if (dt < 0)
		dt = 0;
	else if (dt > 1.0)
		dt = 1;

	if (dt == 0 || pPlayerInfo->renderframe == gBSPRenderer.m_iFrameCount)
	{
		m_pCurrentStudioEntData->m_flGaitMovement = 0;
		return;
	}

	// VectorAdd( pplayer->velocity, pplayer->prediction_error, est_velocity );
	if (bGaitEstimation)
	{
		VectorSubtract(m_pCurrentEntity->origin, pPlayerInfo->prevgaitorigin, est_velocity);
		VectorCopy(m_pCurrentEntity->origin, pPlayerInfo->prevgaitorigin);
		m_pCurrentStudioEntData->m_flGaitMovement = Length(est_velocity);
		if (dt <= 0 || m_pCurrentStudioEntData->m_flGaitMovement / dt < 5)
		{
			m_pCurrentStudioEntData->m_flGaitMovement = 0;
			est_velocity[0] = 0;
			est_velocity[1] = 0;
		}
	}
	else
	{
		VectorCopy(pplayer->velocity, est_velocity);
		m_pCurrentStudioEntData->m_flGaitMovement = Length(est_velocity) * dt;
	}

	if (est_velocity[1] == 0 && est_velocity[0] == 0)
	{
		float flYawDiff = m_pCurrentEntity->angles[YAW] - pPlayerInfo->gaityaw;
		flYawDiff = flYawDiff - (int)(flYawDiff / 360) * 360;
		if (flYawDiff > 180)
			flYawDiff -= 360;
		if (flYawDiff < -180)
			flYawDiff += 360;

		if (dt < 0.25)
			flYawDiff *= dt * 4;
		else
			flYawDiff *= dt;

		pPlayerInfo->gaityaw += flYawDiff;
		pPlayerInfo->gaityaw = pPlayerInfo->gaityaw - (int)(pPlayerInfo->gaityaw / 360) * 360;

		m_pCurrentStudioEntData->m_flGaitMovement = 0;
	}
	else
	{
		pPlayerInfo->gaityaw = (atan2(est_velocity[1], est_velocity[0]) * 180 / M_PI);
		if (pPlayerInfo->gaityaw > 180)
			pPlayerInfo->gaityaw = 180;
		if (pPlayerInfo->gaityaw < -180)
			pPlayerInfo->gaityaw = -180;
	}
}

/*
====================
StudioProcessGait

====================
*/
void CStudioModelRenderer::StudioProcessGait(entity_state_t* pplayer)
{
	mstudioseqdesc_t* pseqdesc;
	float dt;
	int iBlend;
	float flYaw; // view direction relative to movement

	pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

	StudioPlayerBlend(pseqdesc, &iBlend, &m_pCurrentEntity->angles[PITCH]);

	m_pCurrentEntity->latched.prevangles[PITCH] = m_pCurrentEntity->angles[PITCH];
	m_pCurrentEntity->curstate.blending[0] = iBlend;
	m_pCurrentEntity->latched.prevblending[0] = m_pCurrentEntity->curstate.blending[0];
	m_pCurrentEntity->latched.prevseqblending[0] = m_pCurrentEntity->curstate.blending[0];

	// Con_DPrintf("%f %d\n", m_pCurrentEntity->angles[PITCH], m_pCurrentEntity->blending[0] );

	dt = (engine_cl->time - engine_cl->oldtime);
	if (dt < 0)
		dt = 0;
	else if (dt > 1.0)
		dt = 1;

	StudioEstimateGait(pplayer);

	// Con_DPrintf("%f %f\n", m_pCurrentEntity->angles[YAW], pPlayerInfo->gaityaw );

	// calc side to side turning
	flYaw = m_pCurrentEntity->angles[YAW] - pPlayerInfo->gaityaw;
	flYaw = flYaw - (int)(flYaw / 360) * 360;
	if (flYaw < -180)
		flYaw = flYaw + 360;
	if (flYaw > 180)
		flYaw = flYaw - 360;

	if (flYaw > 120)
	{
		pPlayerInfo->gaityaw = pPlayerInfo->gaityaw - 180;
		m_pCurrentStudioEntData->m_flGaitMovement *= -1;
		flYaw = flYaw - 180;
	}
	else if (flYaw < -120)
	{
		pPlayerInfo->gaityaw = pPlayerInfo->gaityaw + 180;
		m_pCurrentStudioEntData->m_flGaitMovement *= -1;
		flYaw = flYaw + 180;
	}

	// adjust torso
	m_pCurrentEntity->curstate.controller[0] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->curstate.controller[1] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->curstate.controller[2] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->curstate.controller[3] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->latched.prevcontroller[0] = m_pCurrentEntity->curstate.controller[0];
	m_pCurrentEntity->latched.prevcontroller[1] = m_pCurrentEntity->curstate.controller[1];
	m_pCurrentEntity->latched.prevcontroller[2] = m_pCurrentEntity->curstate.controller[2];
	m_pCurrentEntity->latched.prevcontroller[3] = m_pCurrentEntity->curstate.controller[3];

	m_pCurrentEntity->angles[YAW] = pPlayerInfo->gaityaw;
	if (m_pCurrentEntity->angles[YAW] < -0)
		m_pCurrentEntity->angles[YAW] += 360;
	m_pCurrentEntity->latched.prevangles[YAW] = m_pCurrentEntity->angles[YAW];

	pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + pplayer->gaitsequence;

	// calc gait frame
	if (pseqdesc->linearmovement[0] > 0)
	{
		pPlayerInfo->gaitframe += (m_pCurrentStudioEntData->m_flGaitMovement / pseqdesc->linearmovement[0]) * pseqdesc->numframes;
	}
	else
	{
		pPlayerInfo->gaitframe += pseqdesc->fps * dt;
	}

	// do modulo
	pPlayerInfo->gaitframe = pPlayerInfo->gaitframe - (int)(pPlayerInfo->gaitframe / pseqdesc->numframes) * pseqdesc->numframes;
	if (pPlayerInfo->gaitframe < 0)
		pPlayerInfo->gaitframe += pseqdesc->numframes;
}

/*
====================
StudioDrawPlayer

====================
*/
void CStudioModelRenderer::StudioDrawPlayerSolid(entity_state_t* pplayer)
{
	nPlayerIndex = pplayer->number - 1;

	if (m_pStudioHeader->numbodyparts == 0)
		return;

	(*m_protationmatrix) = m_pCurrentStudioEntData->rotationmatrix;
	pModel_BonesBuffer->Bind(GL_BufferHandler::UniformBuffer);
	pModel_BonesBuffer->BindRange(GL_BufferHandler::UniformBuffer, s_MDLSolidShader.GetUBOIndex("BonesUBO"), m_pCurrentStudioEntData->bonearrayoffset, sizeof(matrix3x4_t) * m_pStudioHeader->numbones);

	// local player is always drawn
	if (StudioCheckBBox())
		return;

	if (m_pStudioHeader->numbodyparts == 0)
		return;

#if !defined(GL_ONE_BIG_BUFFER_FOR_STUDIOMDLS)
	//enable buffers here so we dont bind buffers of models we won't draw
	if (!m_pCurrentStudioMDL->IsMeshBound())
		m_pCurrentStudioMDL->BindMesh();
#endif

	pPlayerInfo = pfnPlayerInfo(nPlayerIndex);

	pPlayerInfo = NULL;

	if (!(m_pCurrentEntity->curstate.effects & FL_NOMODEL))
	{

		pPlayerInfo = pfnPlayerInfo(nPlayerIndex);

		for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
		{
			StudioMDL_BodyPart* bodypart = m_pCurrentStudioMDL->GetBodyPartbyIndex(i);
			StudioDrawPointsSolid(bodypart);
		}
		
		
		pPlayerInfo = NULL;
		
		if (pplayer->weaponmodel)
		{
			int playermdl_numbones = m_pStudioHeader->numbones;
			model_t* savedmdl = m_pRenderModel;
		
			model_t* pweaponmodel = CL_GetModelByIndex(pplayer->weaponmodel);
			SetCurrentMDL(pweaponmodel);
		
			m_pCurrentEntity->model = pweaponmodel;

			pModel_BonesBuffer->BindRange(GL_BufferHandler::UniformBuffer, s_MDLSolidShader.GetUBOIndex("BonesUBO"), m_pCurrentStudioEntData->bonearrayoffset + (playermdl_numbones * sizeof(matrix3x4_t)), sizeof(matrix3x4_t) * m_pStudioHeader->numbones);
		
			StudioMergeBones(pweaponmodel);
		
#if !defined(GL_ONE_BIG_BUFFER_FOR_STUDIOMDLS)
			m_pCurrentStudioMDL->BindMesh();
#endif

			for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
			{
				StudioMDL_BodyPart* bodypart = m_pCurrentStudioMDL->GetBodyPartbyIndex(i);
				StudioDrawPointsSolid(bodypart);
			}
			
			StudioCalcAttachments();
		
			m_pCurrentEntity->model = savedmdl;
		}

	}
}

/*
====================
StudioDrawPlayer

====================
*/
void CStudioModelRenderer::StudioDrawPlayer(int flags, entity_state_t* pplayer)
{
	nPlayerIndex = pplayer->number - 1;

	if (m_pCurrentEntity == gEngfuncs.GetLocalPlayer())
	{
		if (!CL_IsThirdPerson() && gBSPRenderer.m_bMainPass)
			return;
	}

	if (m_pStudioHeader->numbodyparts == 0)
		return;

	(*m_protationmatrix) = m_pCurrentStudioEntData->rotationmatrix;

	if (flags & STUDIO_RENDER)
	{
		// local player is always drawn
		if (StudioCheckBBox() && m_pCurrentEntity->index != gEngfuncs.GetLocalPlayer()->index)
			return;
	}

	pModel_BonesBuffer->BindRange(GL_BufferHandler::UniformBuffer, s_MDLShader.GetUBOIndex("BonesUBO"), m_pCurrentStudioEntData->bonearrayoffset, sizeof(matrix3x4_t) * m_pStudioHeader->numbones);

	//enable buffers here so we dont bind buffers of models we won't draw
#if !defined(GL_ONE_BIG_BUFFER_FOR_STUDIOMDLS)
	if (!m_pCurrentStudioMDL->IsMeshBound())
		m_pCurrentStudioMDL->BindMesh();
#endif

	pPlayerInfo = pfnPlayerInfo(nPlayerIndex);

	pPlayerInfo = NULL;

	if (flags & STUDIO_EVENTS)
	{
		StudioCalcAttachments();
		StudioClientEvents();
		// copy attachments into global entity array
		if (m_pCurrentEntity->index > 0)
		{
			cl_entity_t* ent = gEngfuncs.GetEntityByIndex(m_pCurrentEntity->index);

			memcpy(ent->attachment, m_pCurrentEntity->attachment, sizeof(Vector) * 4);
		}
	}

	if (flags & STUDIO_RENDER && !(m_pCurrentEntity->curstate.effects & FL_NOMODEL))
	{

		StudioSetupLighting();
		StudioEntityLight();

		pPlayerInfo = pfnPlayerInfo(nPlayerIndex);

		StudioRenderModel();

		if (pplayer->weaponmodel)
		{
			int playermdl_numbones = m_pStudioHeader->numbones;

			model_t* savedmdl = m_pRenderModel;

			model_t* pweaponmodel = CL_GetModelByIndex(pplayer->weaponmodel);
			SetCurrentMDL(pweaponmodel);

			pModel_BonesBuffer->BindRange(GL_BufferHandler::UniformBuffer, s_MDLShader.GetUBOIndex("BonesUBO"), m_pCurrentStudioEntData->bonearrayoffset + (playermdl_numbones * sizeof(matrix3x4_t)), sizeof(matrix3x4_t) * m_pStudioHeader->numbones);

			m_pCurrentEntity->model = pweaponmodel;

#if !defined(GL_ONE_BIG_BUFFER_FOR_STUDIOMDLS)
			m_pCurrentStudioMDL->BindMesh();
#endif

			StudioRenderModel();
			StudioCalcAttachments();

			m_pCurrentEntity->model = savedmdl;
		}
		pPlayerInfo = NULL;
	}

	return;
}

/*
====================
StudioCalcAttachments

====================
*/
void CStudioModelRenderer::StudioCalcAttachments(void)
{
	int i;
	mstudioattachment_t* pattachment;

	assert(m_pStudioHeader->numattachments <= 4);

	// calculate attachment points
	pattachment = (mstudioattachment_t*)((byte*)m_pStudioHeader + m_pStudioHeader->attachmentindex);
	for (i = 0; i < m_pStudioHeader->numattachments; i++)
	{
		VectorTransform(pattachment[i].org, (*m_pbonetransform)[pattachment[i].bone], m_pCurrentEntity->attachment[i]);
	}
}

/*
====================
V_CalcFov
====================
*/
static float V_CalcFov(float* fov_x, float width, float height)
{
	float x, half_fov_y;

	if (*fov_x < 1.0f || *fov_x > 179.0f)
		*fov_x = 90.0f; // default value

	x = width / tan(DEG2RAD(*fov_x) * 0.5f);
	half_fov_y = atan(height / x);

	return RAD2DEG(half_fov_y) * 2;
}


void AdjustFOV(float* fov_x, float* fov_y, float width, float height, qboolean lock_x)
{
	if (!gBSPRenderer.gl_widescreen_yfov->value || width * 3 == 4 * height || width * 4 == height * 5)
	{
		*fov_x = 2.0f * atan(tan(glm::radians(*fov_x) / 2.0f) / (width / height));
		return;
	}

	float x, y;

	if (lock_x)
	{
		*fov_y = 2 * atan((width * 3) / (height * 4) * tan(*fov_y * M_PI / 360.0f * 0.5f)) * 360 / M_PI;
		return;
	}

	y = V_CalcFov(fov_x, 640, 480);
	x = *fov_x;

	*fov_x = V_CalcFov(&y, height, width);
	if (*fov_x < x)
		*fov_x = x;
	else
		*fov_y = y;


	*fov_x = 2.0f * atan(tan(glm::radians(*fov_x) / 2.0f) / (width / height));
}

#define VM_NEARPLANE 4.f
#define VM_FARPLANE 1000.f

void CStudioModelRenderer::CheckVMProjection()
{
	//need to make bsp world use the same view projection matrix as we do otherwise models clip through things
	static float flLastVMFov = -1;

	bool changed_vm_fov = m_pCvarViewmodelFov->value != flLastVMFov;


	if (changed_vm_fov)
	{
		float fov = m_pCvarViewmodelFov->value;
		float fovy = fov;
		float aspect = ScreenWidth / (float)ScreenHeight;

		AdjustFOV(&fov, &fovy, ScreenWidth, ScreenHeight, 0);

		VMProjectionMatrix = glm::perspective(fov, aspect, VM_NEARPLANE, VM_FARPLANE);

		flLastVMFov = m_pCvarViewmodelFov->value;
	}
}

/*
====================
StudioRenderModel

====================
*/
void CStudioModelRenderer::StudioRenderModel(void)
{
	if (m_pCurrentEntity->curstate.renderfx == kRenderFxGlowShell)
	{
		int oldfx = m_pCurrentEntity->curstate.renderfx;
		m_pCurrentEntity->curstate.renderfx = kRenderFxNone;
		bChromeShell = false;
		StudioRenderFinal();

		bChromeShell = true;

		m_pCurrentEntity->curstate.renderfx = oldfx;
		StudioRenderFinal();

		bChromeShell = false;
	}
	else
	{
		StudioRenderFinal();
	}

	g_GlobalGLState.SetBlend(false);
	g_GlobalGLState.SetDepthWrite(true);
}

/*
====================
StudioRenderFinal

====================
*/
void CStudioModelRenderer::StudioRenderFinal(void)
{
	StudioSetupRenderer();

	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		StudioMDL_BodyPart* bodypart = m_pCurrentStudioMDL->GetBodyPartbyIndex(i);
		StudioDrawPoints(bodypart);
	}

	StudioDrawDecals();

	if (gBSPRenderer.m_pCvarWireFrame->value)
		StudioDrawWireframe();

	if (m_pCvarStudioModelBBox->value > 0)
		StudioDrawBBox();
}

/*
====================
StudioDrawWireframe

====================
*/
void CStudioModelRenderer::StudioDrawWireframe(void)
{
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	g_GlobalGLState.SetCullFace(false);
	glLineWidth(1);


	s_MDLShader.Uniform1i(eModel_ShaderLocs[mdlshader_wireframe], 1);

	if (gBSPRenderer.m_pCvarWireFrame->value > 2)
	{
		g_GlobalGLState.SetDepthTest(false);
	}

	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		StudioSetupModel(i);
		StudioMDL_BodyPart* bodypart = m_pCurrentStudioMDL->GetBodyPartbyIndex(i);
		StudioDrawPoints(bodypart);
	}

	studioentity_data_t* pentitydata = (studioentity_data_t*)m_pCurrentEntity->efrag;
	if (!pentitydata->m_vStudioDecals.empty())
	{
		s_MDLShader.Uniform1i(eModel_ShaderLocs[mdlshader_studiodecal], 1);

		glPolygonOffset(-1, -1);
		g_GlobalGLState.SetPolygonOffsetFill(true);
		GL_VertexArrayObject* prev_vao = GL_VertexArrayObject::GetBoundVAO();
		pModel_DecalVAO->BindVAO();

		int vbo;
		glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &vbo);
		printf("Attribute 0 uses VBO ID %d\n", vbo);

		for (auto& studiodecal : pentitydata->m_vStudioDecals)
		{
			int startvert = studiodecal->vertstart;
			glDrawArrays(GL_TRIANGLES, startvert, studiodecal->numverts);
		}
		prev_vao->BindVAO();
		g_GlobalGLState.SetPolygonOffsetFill(false);

		s_MDLShader.Uniform1i(eModel_ShaderLocs[mdlshader_studiodecal], 0);
	}

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	g_GlobalGLState.SetCullFace(true);

	if (gBSPRenderer.m_pCvarWireFrame->value > 2)
	{
		g_GlobalGLState.SetDepthTest(true);
	}

	s_MDLShader.Uniform1i(eModel_ShaderLocs[mdlshader_wireframe], 0);
}

void CStudioModelRenderer::SetCurrentEntity(cl_entity_t* pEnt, bool bStaticProp /* = false */)
{
	pVBOHeader = nullptr;
	m_pCurrentExtraData = nullptr;

	m_pCurrentEntity = pEnt;
	if (!pEnt)
	{
		m_pCurrentStudioEntData = nullptr;
		return;
	}
	m_pCurrentStudioEntData = (studioentity_data_t*)pEnt->efrag;
	if (m_pCurrentStudioEntData)
	{
		if (pEnt == &engine_cl->viewent)
			return;
		if (bStaticProp)
		{
			m_pCurrentExtraData = m_pCurrentStudioEntData->entity_extrainfo->pExtraData;
			pVBOHeader = &m_pCurrentExtraData->pModelData->pVBOHeader;
		}
		else if (m_pCurrentStudioEntData->entity_model != m_pCurrentEntity->model) // whyyyyyyyyy why does THIS HAPPEN
		{
			StudioFreeEntity();
			m_pCurrentStudioEntData = (studioentity_data_t*)(m_pCurrentEntity->efrag = (efrag_t*)StudioAllocEntityData());
			m_pCurrentStudioEntData->entity_index = m_pCurrentEntity->index;
			m_pCurrentStudioEntData->entity_model = m_pCurrentEntity->model;
		}
	}
	else
	{
		m_pCurrentStudioEntData = (studioentity_data_t*)(m_pCurrentEntity->efrag = (efrag_t*)StudioAllocEntityData());
		m_pCurrentStudioEntData->entity_index = m_pCurrentEntity->index;
		m_pCurrentStudioEntData->entity_model = m_pCurrentEntity->model;
	}
}
void CStudioModelRenderer::SetCurrentMDL(model_t* pMdl)
{
	m_pRenderModel = pMdl;
	if (!pMdl)
	{
		m_pCurrentStudioMDL = nullptr;
		m_pStudioHeader = nullptr;
		return;
	}
	m_pCurrentStudioMDL = (StudioMDL_Model*)pMdl->entities;
	m_pStudioHeader = (studiohdr_t*)pMdl->cache.data;
}

/*
====================
StudioSetupModel

====================
*/
void CStudioModelRenderer::StudioSetupModel(int bodypart)
{
	m_pBodyPart = (mstudiobodyparts_t*)((byte*)m_pStudioHeader + m_pStudioHeader->bodypartindex) + bodypart;

	int index = m_pCurrentEntity->curstate.body / m_pBodyPart->base;
	index = index % m_pBodyPart->nummodels;

	m_pSubModel = (mstudiomodel_t*)((byte*)m_pStudioHeader + m_pBodyPart->modelindex) + index;
}

/*
====================
StudioAllocExtraInfo

====================
*/
entextrainfo_t* CStudioModelRenderer::StudioAllocExtraInfo(void)
{
	if (iNumExtraInfo == MAXRENDERENTS)
		iNumExtraInfo = NULL;

	if (pExtraInfo[iNumExtraInfo].pEntity)
	{
		pExtraInfo[iNumExtraInfo].pEntity->topnode = NULL;
		pExtraInfo[iNumExtraInfo].pEntity = NULL;
		memset(&pExtraInfo[iNumExtraInfo], 0, sizeof(entextrainfo_t));
	}

	iNumExtraInfo++;
	return &pExtraInfo[iNumExtraInfo - 1];
}

/*
====================
StudioSwapEngineCache

====================
*/
void CStudioModelRenderer::StudioSwapEngineCache(void)
{
	char szFile[256];
	char szModelName[128];
	char szPath[128];
	char szTexture[64];

	for (int i = 0; i < engine_cl->model_precache_count; i++)
	{
		model_t* pModel = CL_GetModelByIndex((i + 1));
		StudioMDL_Model* model_cache = nullptr;

		if (!pModel)
			break;

		if (pModel->type != mod_studio)
			continue;

		if (!pModel->entities) //new: generate digested model data
			model_cache = new StudioMDL_Model(pModel);
		else
			model_cache = (StudioMDL_Model*)pModel->entities;

		SetCurrentMDL(pModel);

		FilenameFromPath(pModel->name, szModelName);
		strLower(szModelName);

		for (int j = 0; j < model_cache->GetNumTextures(); j++)
		{
			auto ptexture = model_cache->GetTextureByIndex(j);
			const auto textureinfo = ptexture->GetTextureInfo();

			FilenameFromPath(textureinfo.szName, szTexture);
			strLower(szTexture);
		
			if (!gTextureLoader.TextureHasFlag(szModelName, szTexture, TEXFLAG_ALTERNATE))
				continue;
		
			if (m_pCvarDeveloper->value > 1)
				gEngfuncs.Con_Printf("Model '%s' has '%s' marked as using an alternate texture.\n", pModel->name, textureinfo.szName);
		
			bool bNoMipMap = gTextureLoader.TextureHasFlag(szModelName, szTexture, TEXFLAG_NOMIPMAP);
			sprintf(szFile, "gfx/textures/models/%s/%s.dds", szModelName, szTexture);
			
			auto pNewTexture = gTextureLoader.LoadTexture(szFile, 0, false, bNoMipMap ? true : false);
		
			if (pNewTexture)
			{
				ptexture->ReplaceMDLTexture(pNewTexture, 0);

				if(m_pCvarDeveloper->value > 1)
					gEngfuncs.Con_Printf("Loaded '%s'\n", szFile);
			}
		}
	}
}

/*
====================
StudioSetupRenderer

====================
*/
void CStudioModelRenderer::StudioSetupRenderer()
{
	memset(&model_PerEntityData, 0, sizeof(model_PerEntityData));

	model_PerEntityData.modelmatrix = gBSPRenderer.m_ModelMatrix;
	model_PerEntityData.int_values.y = bChromeShell;
	model_PerEntityData.int_values.z = bExternalEntity;

	color24 colors = m_pCurrentEntity->curstate.rendercolor;

	model_PerEntityData.rendervalues = glm::vec4(colors.r / 255.f, colors.g / 255.f, colors.b / 255.f, m_pCurrentEntity->curstate.renderamt / 255.f);


	if (!bChromeShell) //dont bother with light data if doing chrome shell
	{
		// lightmap light
		model_PerEntityData.lightdir = glm::vec4(m_pLighting.lightdir.x, m_pLighting.lightdir.y, m_pLighting.lightdir.z, 0);
		model_PerEntityData.ambientlight = glm::vec4(m_pLighting.ambientlight.x, m_pLighting.ambientlight.y, m_pLighting.ambientlight.z, 0);
		model_PerEntityData.diffuselight = glm::vec4(m_pLighting.diffuselight.x, m_pLighting.diffuselight.y, m_pLighting.diffuselight.z, 0);
		//

		model_PerEntityData.int_values.x =  iNumModelLights;

		for (int i = 0; i < iNumModelLights; i++)
		{
			auto &mdlight = pModelLights[i];

			Vector flPosition(mdlight->origin[0], mdlight->origin[1], mdlight->origin[2]);
			Vector flForward(mdlight->forward[0], mdlight->forward[1], mdlight->forward[2]);

			bool spotlight = mdlight->spotcos > 0 ? 1 : 0;

			model_PerEntityData.modellight_info[i][0] = glm::vec4(flPosition.x, flPosition.y, flPosition.z, spotlight);
			model_PerEntityData.modellight_info[i][1] = glm::vec4(mdlight->color.x, mdlight->color.y, mdlight->color.z, mdlight->radius);
			model_PerEntityData.modellight_info[i][2] = glm::vec4(flForward[0], flForward[1], flForward[2], cos((mdlight->spotcos * 2) * 0.3 * (M_PI2 / 360)));
		}
	}

	pModel_PerEntityBuffer->Bind(GL_BufferHandler::UniformBuffer);
	pModel_PerEntityBuffer->BufferSubData(GL_BufferHandler::UniformBuffer, 0, sizeof(mdlshader_perentitydata_t), &model_PerEntityData);

	int rendermode = m_pCurrentEntity->curstate.rendermode;
	if (rendermode == kRenderTransTexture || rendermode == kRenderTransTexture)
	{
		g_GlobalGLState.SetBlend(true);
		g_GlobalGLState.SetBlendFunc(GL_SRC_ALPHA, rendermode == kRenderTransTexture ? GL_ONE_MINUS_SRC_ALPHA : GL_ONE);
		g_GlobalGLState.SetDepthWrite(false);
	}
}

/*
====================
StudioSetupLighting

====================
*/
void CStudioModelRenderer::StudioSetupLighting(void)
{
	int iret = 0;
	Vector color;
	Vector end;
	Vector point;

	entextrainfo_t* pInfo = m_pCurrentStudioEntData->entity_extrainfo;

	Vector eorigin;
	eorigin[0] = (*m_protationmatrix)[0][3];
	eorigin[1] = (*m_protationmatrix)[1][3];
	eorigin[2] = (*m_protationmatrix)[2][3];

	if (!pInfo)
	{
		if (m_pCurrentEntity->index > 0 && m_pCurrentEntity != &engine_cl->viewent)
		{
			pInfo = StudioAllocExtraInfo();
			pInfo->pEntity = m_pCurrentEntity;

			m_pCurrentStudioEntData->entity_extrainfo = pInfo;
		}
	}
	else
	{
		msurface_t* msurf = &engine_cl->worldmodel->surfaces[pInfo->surfindex];
		clientsurfdata_t* csurf = &gBSPRenderer.m_pSurfaces[pInfo->surfindex];

		int i = 0;
		for (; i < MAXLIGHTMAPS && msurf->styles[i] != 255; i++)
		{
			if (csurf->cached_light[i] != pInfo->lightstyles[i])
				break;
		}

		if (pInfo->prevpos == eorigin && i == MAXLIGHTMAPS)
		{
			memcpy(&m_pLighting, &pInfo->pLighting, sizeof(lighting_ext));
			return;
		}
	}

	if (m_pCurrentEntity->model)
	{
		if (m_pCurrentEntity->curstate.effects & EF_INVLIGHT)
			point = eorigin - Vector(0, 0, 5);
		else
			point = eorigin + Vector(0, 0, 5);
	}
	else
	{
		if (m_pCurrentEntity->curstate.effects & EF_INVLIGHT)
			point = pInfo->pExtraData->lightorigin - Vector(0, 0, 5);
		else
			point = pInfo->pExtraData->lightorigin + Vector(0, 0, 5);
	}

	end.x = point.x;
	end.y = point.y;

	if (m_pCurrentEntity->curstate.effects & EF_INVLIGHT)
		end.z = point.z + 8136;
	else
		end.z = point.z - 8136;

	if (engine_cl->worldmodel->lightdata && !gBSPRenderer.r_fullbright->value)
		iret = StudioRecursiveLightPoint(pInfo, BSPWorld_Model::m_pWorldNodes, point, end, color);
	else
	{
		iret = true;
		color = Vector(1, 1, 1);
	}

	if (!iret)
	{
		m_pLighting.diffuselight.x = ((float)m_pCvarSkyColorX->value / 255) * 0.55;
		m_pLighting.diffuselight.y = ((float)m_pCvarSkyColorY->value / 255) * 0.55;
		m_pLighting.diffuselight.z = ((float)m_pCvarSkyColorZ->value / 255) * 0.55;

		m_pLighting.ambientlight.x = ((float)m_pCvarSkyColorX->value / 255) * 0.45;
		m_pLighting.ambientlight.y = ((float)m_pCvarSkyColorY->value / 255) * 0.45;
		m_pLighting.ambientlight.z = ((float)m_pCvarSkyColorZ->value / 255) * 0.45;

		m_pLighting.lightdir.x = m_pCvarSkyVecX->value;
		m_pLighting.lightdir.y = m_pCvarSkyVecY->value;
		m_pLighting.lightdir.z = m_pCvarSkyVecZ->value;
		return;
	}

	m_pLighting.diffuselight = color * 0.55;

	m_pLighting.ambientlight = color * 0.45;

	m_pLighting.lightdir.x = 0;
	m_pLighting.lightdir.y = 0;
	m_pLighting.lightdir.z = -0.5f;

	if (pInfo)
	{
		memcpy(&pInfo->pLighting, &m_pLighting, sizeof(lighting_ext));
		if (!m_pCurrentEntity->model)
			pInfo->prevpos = eorigin;
		else
			pInfo->prevpos = m_pCurrentEntity->origin;
	}
}

/*
====================
StudioRecursiveLightPoint

====================
*/
bool CStudioModelRenderer::StudioRecursiveLightPoint(entextrainfo_t* ext, clientmnode_t* node, Vector start, Vector end, Vector& color)
{
	float front, back = 0, frac;
	bool side;
	mplane_t* plane;
	Vector mid;
	clientmsurface_t* surf;
	unsigned short s, t, ds, dt;
	unsigned short i;
	mtexinfo_t* tex;
	color24* lightmap;

	while (true)
	{

		if (node->contents < 0)
			return false; // didn't hit anything

		plane = node->plane;

		front = DotProduct(start, plane->normal) - plane->dist;
		back = DotProduct(end, plane->normal) - plane->dist;

		side = front < 0;

		if ((back < 0) == side)
			return StudioRecursiveLightPoint(ext, node->children[side], start, end, color);

		frac = front / (front - back);
		mid[0] = start[0] + (end[0] - start[0]) * frac;
		mid[1] = start[1] + (end[1] - start[1]) * frac;
		mid[2] = start[2] + (end[2] - start[2]) * frac;

		// go down front side
		int r = StudioRecursiveLightPoint(ext, node->children[side], start, mid, color);

		if (r)
			return true;

		if ((back < 0) == side)
			return false;

		model_t* world = engine_cl->worldmodel;
		surf = BSPWorld_Model::m_pWorldSurfaces + node->firstsurface;
		for (i = 0; i < node->numsurfaces; i++, surf++)
		{
			if ((surf->flags & (SURF_DRAWTILED | SURF_DRAWSKY)))
				continue; // no lightmaps

			int index = node->firstsurface + i;
			tex = surf->texinfo;
			s = DotProduct(mid, tex->vecs[0]) + tex->vecs[0][3];
			t = DotProduct(mid, tex->vecs[1]) + tex->vecs[1][3];

			if (s < surf->texturemins[0] ||
				t < surf->texturemins[1])
				continue;

			ds = s - surf->texturemins[0];
			dt = t - surf->texturemins[1];

			if (ds > surf->extents[0] || dt > surf->extents[1])
				continue;

			ds >>= 4;
			dt >>= 4;

			if (!surf->samples)
				continue;

			lightmap = surf->samples;

			int surfindex = node->firstsurface + i;
			int size = ((surf->extents[1] >> 4) + 1) * ((surf->extents[0] >> 4) + 1);
			lightmap += dt * ((surf->extents[0] >> 4) + 1) + ds;

			float flIntensity = (lightmap->r + lightmap->g + lightmap->b) / 3;
			float flScale = flIntensity / 50;

			if (flScale > 1.0)
				flScale = 1.0;

			//color.x = (float)(lightmap->r / 255.f * flScale);
			//color.y = (float)(lightmap->g / 255.f * flScale);
			//color.z = (float)(lightmap->b / 255.f * flScale);

			if (ext)
				ext->lightstyles[0] = gBSPRenderer.m_iLightStyleValue[surf->styles[0]];

			for (int style = 0; style < MAXLIGHTMAPS && surf->styles[style] != 255; style++)
			{
				float scale = (float)gBSPRenderer.m_iLightStyleValue[surf->styles[style]] / 255;

				color.x += ((float)lightmap->r / 255.f) * scale;
				color.y += ((float)lightmap->g / 255.f) * scale;
				color.z += ((float)lightmap->b / 255.f) * scale;


				if (ext)
					ext->lightstyles[style] = gBSPRenderer.m_iLightStyleValue[surf->styles[style]];

				lightmap += size; // skip to next lightmap
			}

			if (ext)
				ext->surfindex = node->firstsurface + i;

			return true;
		}

		node = node->children[!side];
		start = mid;
	}

	// go down back side
	//return StudioRecursiveLightPoint(ext, node->children[!side], mid, end, color);
}

/*
====================
StudioCullBBox

====================
*/
bool CStudioModelRenderer::StudioCullBBox(const Vector& mins, const Vector& maxs)
{
	if (v_bboxMins[0] > maxs[0])
		return true;

	if (v_bboxMins[1] > maxs[1])
		return true;

	if (v_bboxMins[2] > maxs[2])
		return true;

	if (v_bboxMaxs[0] < mins[0])
		return true;

	if (v_bboxMaxs[1] < mins[1])
		return true;

	if (v_bboxMaxs[2] < mins[2])
		return true;

	return false;
}

/*
====================
StudioEntityLight

====================
*/
void CStudioModelRenderer::StudioEntityLight(void)
{
	pmtrace_t pmtrace;
	EV_SetTraceHull(2);

	Vector vCenter;
	VectorAdd(v_bboxMins, v_bboxMaxs, vCenter);
	VectorScale(vCenter, 0.5, vCenter);

	iNumModelLights = NULL;
	mlight_t* mlight = &gBSPRenderer.m_pModelLights[0];

	for (int i = 0; i < gBSPRenderer.m_iNumModelLights; i++, mlight++)
	{
		if (!mlight->flashlight)
		{
			if (iNumModelLights == MAX_MODEL_LIGHTS)
				continue;
		}

		if (mlight->radius)
		{
			if (mlight->spotcos)
			{
				if (mlight->frustum->CullBox(v_bboxMins, v_bboxMaxs))
					continue;
			}
			else
			{
				if (StudioCullBBox(mlight->mins, mlight->maxs))
					continue;
			}

			// perform trace
			gEngfuncs.pEventAPI->EV_PlayerTrace(vCenter, mlight->origin, PM_WORLD_ONLY, -1, &pmtrace);

			if (pmtrace.fraction < 1.0 && !pmtrace.startsolid)
				continue; // blocked

			if (mlight->flashlight && iNumModelLights == MAX_MODEL_LIGHTS)
			{
				// Flashlight must get on this list.
				pModelLights[(iNumModelLights - 1)] = mlight;
			}
			else
			{
				pModelLights[iNumModelLights] = mlight;
				iNumModelLights++;
			}
		}
	}
}

/*
====================
StudioDrawPoints

====================
*/
void CStudioModelRenderer::StudioDrawPoints(StudioMDL_BodyPart* bodypart)
{
	int index = m_pCurrentEntity->curstate.body / bodypart->GetBase();
	index = index % bodypart->GetNumModels();

	StudioMDL_SubModel* submodel = bodypart->GetModelbyIndex(index);
	if (!submodel->GetMeshNum())
		return;

	int numskinfamilies = m_pCurrentStudioMDL->GetNumSkinIndexes();


	int skinindex = std::clamp((int)m_pCurrentEntity->curstate.skin, 0, numskinfamilies) * numskinfamilies;
	short* pskinref = m_pCurrentStudioMDL->GetSkinIndexes() + skinindex;

	//
	// Render normal textures
	//
	std::vector<int> specialmeshes;
	for (int i = 0; i < submodel->GetMeshNum(); i++)
	{
		StudioMDL_Mesh* pmesh = submodel->GetMeshbyIndex(i);

		int meshskinref = std::clamp(pmesh->SkinReference(), 0, m_pCurrentStudioMDL->GetNumTextures() - 1);

		auto ptex = m_pCurrentStudioMDL->GetTextureByIndex(pskinref[meshskinref]);
		if (ptex->GetTextureFlags() & STUDIO_NF_ADDITIVE)
		{
			specialmeshes.push_back(i);
			continue;
		}

		StudioDrawMesh(pmesh, ptex);

	}

	if (specialmeshes.empty())
		return;

	//render textures that require blending

	g_GlobalGLState.SetBlend(true);
	g_GlobalGLState.SetBlendFunc(GL_ONE, GL_ONE);
	g_GlobalGLState.SetDepthWrite(false);

	//render textures that require blending
	for (int i = 0; i < specialmeshes.size(); i++)
	{
		StudioMDL_Mesh* pmesh = submodel->GetMeshbyIndex(specialmeshes[i]);

		int meshskinref = std::clamp(pmesh->SkinReference(), 0, m_pCurrentStudioMDL->GetNumTextures() - 1);

		auto ptex = m_pCurrentStudioMDL->GetTextureByIndex(pskinref[meshskinref]);
		StudioDrawMesh(pmesh, ptex);
	}

	g_GlobalGLState.SetBlend(false);
	g_GlobalGLState.SetDepthWrite(true);
}

/*
====================
StudioDrawMesh

====================
*/
void CStudioModelRenderer::StudioDrawMesh(StudioMDL_Mesh* pmesh, StudioMDL_Texture *ptex)
{
	s_MDLShader.Uniform1i(eModel_ShaderLocs[mdlshader_texture_flags], ptex->GetTextureFlags());

	if (!bChromeShell)
		gBSPRenderer.BindGLTexture(GL_TEXTURE0, ptex->GetTextureInfo().iIndex);

	pmesh->DrawMesh();

	gBSPRenderer.m_iStudioPolyCounter += pmesh->NumIndices();
}

void R_ComputeBBox(const Vector& origin,int sequence, Vector& mins, Vector& maxs)
{
	// Fake bboxes for models.
	static const Vector gFakeHullMins(-16, -16, -16);
	static const Vector gFakeHullMaxs(16, 16, 16);

	if (!VectorCompare(vec3_origin, m_pStudioHeader->bbmin))
	{
		// clipping bounding box
		VectorAdd(origin, m_pStudioHeader->bbmin, mins);
		VectorAdd(origin, m_pStudioHeader->bbmin, maxs);
	}
	else if (!VectorCompare(vec3_origin, m_pStudioHeader->min))
	{
		// movement bounding box
		VectorAdd(origin, m_pStudioHeader->min, mins);
		VectorAdd(origin, m_pStudioHeader->max, maxs);
	}
	else
	{
		// fake bounding box
		VectorAdd(origin, gFakeHullMins, mins);
		VectorAdd(origin, gFakeHullMaxs, maxs);
	}

	// construct the base bounding box for this frame
	if (sequence >= m_pStudioHeader->numseq)
	{
		sequence = 0;
	}

	mstudioseqdesc_t* pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + sequence;

	Vector localCenter = (pseqdesc->bbmin + pseqdesc->bbmax) * 0.5;

	Vector localExtents = pseqdesc->bbmax - localCenter;

	Vector worldCenter;
	VectorTransform(localCenter, (*g_StudioRenderer.m_protationmatrix), worldCenter);
	Vector worldExtents;

	VectorRotateAbs(localExtents, (*g_StudioRenderer.m_protationmatrix), worldExtents);

	//use worldCenter, not the entity's origin, to account for un-centered bounding boxes
	VectorMin(mins, worldCenter - worldExtents, mins);
	VectorMax(maxs, worldCenter + worldExtents, maxs);
}

/*
====================
StudioCheckBBox

====================
*/
qboolean CStudioModelRenderer::StudioCheckBBox(void)
{
	// View entity is always present
	if (m_pCurrentEntity == &engine_cl->viewent)
		return false;

	v_bboxMins = v_bboxMaxs = vec3_origin;

	R_ComputeBBox(m_pCurrentEntity->origin, m_pCurrentEntity->curstate.sequence, v_bboxMins, v_bboxMaxs);

	// Copy it over to the entity
	VectorCopy(v_bboxMins, m_pCurrentEntity->curstate.mins);
	VectorCopy(v_bboxMaxs, m_pCurrentEntity->curstate.maxs);

	return gHUD.viewFrustum.CullBox(v_bboxMins, v_bboxMaxs);
}

/*
====================
StudioDrawBBox

====================
*/
void CStudioModelRenderer::StudioDrawBBox(void)
{
	Vector v[8];

	v[0][0] = v_bboxMins[0];
	v[0][1] = v_bboxMaxs[1];
	v[0][2] = v_bboxMins[2];
	v[1][0] = v_bboxMins[0];
	v[1][1] = v_bboxMins[1];
	v[1][2] = v_bboxMins[2];
	v[2][0] = v_bboxMaxs[0];
	v[2][1] = v_bboxMaxs[1];
	v[2][2] = v_bboxMins[2];
	v[3][0] = v_bboxMaxs[0];
	v[3][1] = v_bboxMins[1];
	v[3][2] = v_bboxMins[2];

	v[4][0] = v_bboxMaxs[0];
	v[4][1] = v_bboxMaxs[1];
	v[4][2] = v_bboxMaxs[2];
	v[5][0] = v_bboxMaxs[0];
	v[5][1] = v_bboxMins[1];
	v[5][2] = v_bboxMaxs[2];
	v[6][0] = v_bboxMins[0];
	v[6][1] = v_bboxMaxs[1];
	v[6][2] = v_bboxMaxs[2];
	v[7][0] = v_bboxMins[0];
	v[7][1] = v_bboxMins[1];
	v[7][2] = v_bboxMaxs[2];

	g_GlobalGLState.SetCullFace(false);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	GL_ShaderProgram::ResetShaderBind();

	glBegin(GL_QUAD_STRIP);
	for (int i = 0; i < 10; i++)
	{
		glColor4f(0, 0, 1.0, 0.5);
		glVertex3fv(v[i & 7]);
	}
	glEnd();

	glBegin(GL_QUAD_STRIP);
	glColor4f(0, 0, 1.0, 0.5);
	glVertex3fv(v[6]);
	glColor4f(0, 0, 1.0, 0.5);
	glVertex3fv(v[0]);
	glColor4f(0, 0, 1.0, 0.5);
	glVertex3fv(v[4]);
	glColor4f(0, 0, 1.0, 0.5);
	glVertex3fv(v[2]);
	glEnd();

	glBegin(GL_QUAD_STRIP);
	glColor4f(0, 0, 1.0, 0.5);
	glVertex3fv(v[1]);
	glColor4f(0, 0, 1.0, 0.5);
	glVertex3fv(v[7]);
	glColor4f(0, 0, 1.0, 0.5);
	glVertex3fv(v[3]);
	glColor4f(0, 0, 1.0, 0.5);
	glVertex3fv(v[5]);
	glEnd();

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	g_GlobalGLState.SetCullFace(true);

	s_MDLShader.Bind();
}

/*
====================
StudioDrawPropEntity

====================
*/
void CStudioModelRenderer::StudioDrawPropEntity(cl_entity_t* pEntity, bool bSkybox)
{
	Vector vRealOrigin;
	Vector vTransOrigin;

	SetCurrentEntity(pEntity, true);
	SetCurrentMDL(pEntity->model);

	if (!m_pStudioHeader || !m_pCurrentStudioMDL || !pVBOHeader)
		return;

	if (m_pStudioHeader->numbodyparts == 0)
		return;

	VectorCopy(m_pCurrentExtraData->absmin, v_bboxMins);
	VectorCopy(m_pCurrentExtraData->absmax, v_bboxMaxs);

	if (gHUD.viewFrustum.CullBox(v_bboxMins, v_bboxMaxs) && !bSkybox)
		return;

	StudioSetupPropLighting();
	StudioEntityLight();
	StudioDrawPropModel();
}

/*
====================
StudioSetupLighting

====================
*/
void CStudioModelRenderer::StudioSetupPropLighting(void)
{
	int iret = 0;
	Vector color;
	Vector end;
	Vector point;

	entextrainfo_t* pInfo = m_pCurrentStudioEntData->entity_extrainfo;

	Vector eorigin = m_pCurrentEntity->origin;

	if (!pInfo)
	{
		if (m_pCurrentEntity->index > 0 && m_pCurrentEntity != &engine_cl->viewent)
		{
			pInfo = StudioAllocExtraInfo();
			pInfo->pEntity = m_pCurrentEntity;

			m_pCurrentStudioEntData->entity_extrainfo = pInfo;
		}
	}
	else
	{
		msurface_t* msurf = &engine_cl->worldmodel->surfaces[pInfo->surfindex];
		clientsurfdata_t* csurf = &gBSPRenderer.m_pSurfaces[pInfo->surfindex];

		int i = 0;
		for (; i < MAXLIGHTMAPS && msurf->styles[i] != 255; i++)
		{
			if (csurf->cached_light[i] != pInfo->lightstyles[i])
				break;
		}

		if (i == MAXLIGHTMAPS)
		{
			memcpy(&m_pLighting, &pInfo->pLighting, sizeof(lighting_ext));
			return;
		}
	}

	if (m_pCurrentEntity->curstate.effects & EF_INVLIGHT)
			point = pInfo->pExtraData->lightorigin - Vector(0, 0, 5);
	else
		point = pInfo->pExtraData->lightorigin + Vector(0, 0, 5);

	end.x = point.x;
	end.y = point.y;

	if (m_pCurrentEntity->curstate.effects & EF_INVLIGHT)
		end.z = point.z + 8136;
	else
		end.z = point.z - 8136;

	if (engine_cl->worldmodel->lightdata && !gBSPRenderer.r_fullbright->value)
		iret = StudioRecursiveLightPoint(pInfo, BSPWorld_Model::m_pWorldNodes, point, end, color);
	else
	{
		iret = true;
		color = Vector(1, 1, 1);
	}

	if (!iret)
	{
		m_pLighting.diffuselight.x = ((float)m_pCvarSkyColorX->value / 255) * 0.55;
		m_pLighting.diffuselight.y = ((float)m_pCvarSkyColorY->value / 255) * 0.55;
		m_pLighting.diffuselight.z = ((float)m_pCvarSkyColorZ->value / 255) * 0.55;

		m_pLighting.ambientlight.x = ((float)m_pCvarSkyColorX->value / 255) * 0.45;
		m_pLighting.ambientlight.y = ((float)m_pCvarSkyColorY->value / 255) * 0.45;
		m_pLighting.ambientlight.z = ((float)m_pCvarSkyColorZ->value / 255) * 0.45;

		m_pLighting.lightdir.x = m_pCvarSkyVecX->value;
		m_pLighting.lightdir.y = m_pCvarSkyVecY->value;
		m_pLighting.lightdir.z = m_pCvarSkyVecZ->value;
		return;
	}

	m_pLighting.diffuselight = color * 0.55;

	m_pLighting.ambientlight = color * 0.45;

	m_pLighting.lightdir.x = 0;
	m_pLighting.lightdir.y = 0;
	m_pLighting.lightdir.z = -0.5f;

	if (pInfo)
	{
		memcpy(&pInfo->pLighting, &m_pLighting, sizeof(lighting_ext));
		pInfo->prevpos = eorigin;
	}
}

/*
====================
StudioSaveModelData

====================
*/
void CStudioModelRenderer::StudioSaveModelData(modeldata_t* pExtraData)
{
	mstudiobodyparts_t* bp = (mstudiobodyparts_t*)((byte*)m_pStudioHeader + m_pStudioHeader->bodypartindex);

	int n = 0;
	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
		n += bp[i].nummodels;

	if (n == 0)
		return;

	SetCurrentMDL(pExtraData->pMdl);

	pVBOHeader = &pExtraData->pVBOHeader;
	pVBOHeader->numsubmodels = n;
	pVBOHeader->submodels = new vbosubmodel_t[n];
	memset(pVBOHeader->submodels, 0, sizeof(vbosubmodel_t) * n);

	n = 0;
	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		m_pSubModel = (mstudiomodel_t*)((byte*)m_pStudioHeader + bp[i].modelindex);
		StudioMDL_BodyPart* bodypart = m_pCurrentStudioMDL->GetBodyPartbyIndex(i);
		for (int k = 0; k < bp[i].nummodels; k++)
		{
			vbosubmodel_t* pvbosubmodel = &pExtraData->pVBOHeader.submodels[n];
			n++;
			mstudiomesh_t* pmeshes = (mstudiomesh_t*)((byte*)m_pStudioHeader + m_pSubModel[k].meshindex);
			iCurStart = pRefArray.size();

			pvbosubmodel->nummeshes = m_pSubModel[k].nummesh;
			pvbosubmodel->meshes = new vbomesh_t[m_pSubModel[k].nummesh];
			memset(pvbosubmodel->meshes, 0, sizeof(vbomesh_t) * m_pSubModel[k].nummesh);

			byte* pvertbone = ((byte*)m_pStudioHeader + m_pSubModel[k].vertinfoindex);
			byte* pnormbone = ((byte*)m_pStudioHeader + m_pSubModel[k].norminfoindex);

			Vector* pstudionorms = (Vector*)((byte*)m_pStudioHeader + m_pSubModel[k].normindex);
			Vector* pstudioverts = (Vector*)((byte*)m_pStudioHeader + m_pSubModel[k].vertindex);

			for (int j = 0; j < m_pSubModel[k].numnorms; j++)
				VectorRotate(pstudionorms[j], (*m_pbonetransform)[pnormbone[j]], vNormalTransform[j]);

			for (int j = 0; j < m_pSubModel[k].numverts; j++)
				VectorTransform(pstudioverts[j], (*m_pbonetransform)[pvertbone[j]], vVertexTransform[j]);

			for (int l = 0; l < m_pSubModel[k].nummesh; l++)
			{
				vbomesh_t* pvbomesh = &pvbosubmodel->meshes[l];
				pvbomesh->start_vertex = usIndexes.size();

				short* pskinref = m_pCurrentStudioMDL->GetSkinIndexes();

				int meshskinref = bodypart->GetModelbyIndex(k)->GetMeshbyIndex(l)->SkinReference();

				meshskinref = std::clamp(meshskinref, 0, m_pCurrentStudioMDL->GetNumTextures() - 1);

				auto ptex = m_pCurrentStudioMDL->GetTextureByIndex(pskinref[meshskinref]);
				auto ptexinfo = ptex->GetTextureInfo();

				float basewidth = ptexinfo.iWidth;
				float baseheight = ptexinfo.iHeight;



				int j = 0;
				short* ptricmds = (short*)((byte*)m_pStudioHeader + pmeshes[l].triindex);
				while (j = *(ptricmds++))
				{
					if (j > 0)
					{
						// convert triangle strip
						j -= 3;
						studiovert_t indices[3];
						for (int i = 0; i < 3; i++, ptricmds += 4)
						{
							indices[i].vertindex = ptricmds[0];
							indices[i].normindex = ptricmds[1];

							float texcoord1 = static_cast<float>(ptricmds[2]);
							float texcoord2 = static_cast<float>(ptricmds[3]);

							texcoord1 /= basewidth;
							texcoord2 /= baseheight;


							indices[i].texcoord[0] = texcoord1;
							indices[i].texcoord[1] = texcoord2;
							StudioManageVertex(&indices[i]);
						}

						bool reverse = false;
						for (; j > 0; j--, ptricmds += 4)
						{
							indices[0] = indices[1];
							indices[1] = indices[2];
							indices[2].vertindex = ptricmds[0];
							indices[2].normindex = ptricmds[1];

							float texcoord1 = static_cast<float>(ptricmds[2]);
							float texcoord2 = static_cast<float>(ptricmds[3]);

							texcoord1 /= basewidth;
							texcoord2 /= baseheight;

							if (texcoord1 < 0)
								texcoord1 = 0.0f;

							if (texcoord2 < 0)
								texcoord2 = 0.0f;

							indices[2].texcoord[0] = texcoord1;
							indices[2].texcoord[1] = texcoord2;

							if (!reverse)
							{
								StudioManageVertex(&indices[2]);
								StudioManageVertex(&indices[1]);
								StudioManageVertex(&indices[0]);
							}
							else
							{
								StudioManageVertex(&indices[0]);
								StudioManageVertex(&indices[1]);
								StudioManageVertex(&indices[2]);
							}
							reverse = !reverse;
						}
					}
					else
					{
						// convert triangle fan
						j = -j - 3;
						studiovert_t indices[3];
						for (int i = 0; i < 3; i++, ptricmds += 4)
						{
							indices[i].vertindex = ptricmds[0];
							indices[i].normindex = ptricmds[1];

							float texcoord1 = static_cast<float>(ptricmds[2]);
							float texcoord2 = static_cast<float>(ptricmds[3]);

							texcoord1 /= basewidth;
							texcoord2 /= baseheight;

							if (texcoord1 < 0)
								texcoord1 = 0.0f;

							if (texcoord2 < 0)
								texcoord2 = 0.0f;

							indices[i].texcoord[0] = texcoord1;
							indices[i].texcoord[1] = texcoord2;
							StudioManageVertex(&indices[i]);
						}

						for (; j > 0; j--, ptricmds += 4)
						{
							indices[1] = indices[2];
							indices[2].vertindex = ptricmds[0];
							indices[2].normindex = ptricmds[1];

							float texcoord1 = static_cast<float>(ptricmds[2]);
							float texcoord2 = static_cast<float>(ptricmds[3]);

							texcoord1 /= basewidth;
							texcoord2 /= baseheight;

							if (texcoord1 < 0)
								texcoord1 = 0.0f;

							if (texcoord2 < 0)
								texcoord2 = 0.0f;

							indices[2].texcoord[0] = texcoord1;
							indices[2].texcoord[1] = texcoord2;

							StudioManageVertex(&indices[0]);
							StudioManageVertex(&indices[1]);
							StudioManageVertex(&indices[2]);
						}
					}
				}

				// Number of indexes to cache
				pvbomesh->num_vertexes = usIndexes.size() - pvbomesh->start_vertex;
			}
		}
	}

	// Copy over all VBO vertexes
	pVBOHeader->numverts = pVBOVerts.size();
	pVBOHeader->pBufferData = new Default3DVert_t[pVBOVerts.size()];
	memcpy(pVBOHeader->pBufferData, pVBOVerts.data(), sizeof(Default3DVert_t) * pVBOVerts.size());

	// Copy over index array
	pVBOHeader->numindexes = usIndexes.size();
	pVBOHeader->indexes = new unsigned int[usIndexes.size()];
	memcpy(pVBOHeader->indexes, usIndexes.data(), sizeof(unsigned int) * usIndexes.size());

	// Reset this
	pRefArray.clear();
	pVBOVerts.clear();
	usIndexes.clear();
}

/*
====================
StudioManageVertex

====================
*/
void CStudioModelRenderer::StudioManageVertex(studiovert_t* pvert)
{
	for (int i = iCurStart; i < pRefArray.size(); i++)
	{
		if (pvert->normindex == pRefArray[i].normindex && pvert->vertindex == pRefArray[i].vertindex && pvert->texcoord[0] == pRefArray[i].texcoord[0] && pvert->texcoord[1] == pRefArray[i].texcoord[1])
		{
			usIndexes.push_back(i);
			return;
		}
	}

	usIndexes.push_back(pRefArray.size());

	pRefArray.push_back({
		pvert->vertindex,
		pvert->normindex,
		{pvert->texcoord[0], pvert->texcoord[1]},
		pvert->boneindex});

	Default3DVert_t vert{};
	vert.pos = vVertexTransform[pvert->vertindex];
	vert.normal = vNormalTransform[pvert->normindex];
	vert.texcoord[0] = pvert->texcoord[0];
	vert.texcoord[1] = pvert->texcoord[1];

	pVBOVerts.push_back(vert);
}

/*
====================
StudioSaveUniqueData

====================
*/
void CStudioModelRenderer::StudioSaveUniqueData(entextradata_t* pExtraData)
{
	Vector vBounds[8];

	Vector vTemp;
	Vector vMins(9999, 9999, 9999);
	Vector vMaxs(-9999, -9999, -9999);

	pVBOHeader = &pExtraData->pModelData->pVBOHeader;
	SetCurrentMDL(pExtraData->pModelData->pMdl);

	m_pCurrentEntity->angles[PITCH] = -m_pCurrentEntity->angles[PITCH];
	AngleMatrix(m_pCurrentEntity->angles, (*m_protationmatrix));
	m_pCurrentEntity->angles[PITCH] = -m_pCurrentEntity->angles[PITCH];

	int baseindex = 0;
	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		m_pBodyPart = (mstudiobodyparts_t*)((byte*)m_pStudioHeader + m_pStudioHeader->bodypartindex) + i;
		int index = (m_pCurrentEntity->curstate.body / m_pBodyPart->base) % m_pBodyPart->nummodels;

		m_pSubModel = (mstudiomodel_t*)((byte*)m_pStudioHeader + m_pBodyPart->modelindex) + index;
		pVBOSubModel = &pVBOHeader->submodels[baseindex + index];

		for (int j = 0; j < pVBOSubModel->nummeshes; j++)
		{
			vbomesh_t* pvbomesh = &pVBOSubModel->meshes[j];
			for (int k = 0; k < pvbomesh->num_vertexes; k++)
			{
				VectorRotate(pVBOHeader->pBufferData[pVBOHeader->indexes[(pvbomesh->start_vertex + k)]].pos, (*m_protationmatrix), vTemp);

				if (m_pCurrentEntity->curstate.scale)
					VectorScale(vTemp, m_pCurrentEntity->curstate.scale, vTemp);

				if (vTemp.x < vMins.x)
					vMins.x = vTemp.x;
				if (vTemp.y < vMins.y)
					vMins.y = vTemp.y;
				if (vTemp.z < vMins.z)
					vMins.z = vTemp.z;

				if (vTemp.x > vMaxs.x)
					vMaxs.x = vTemp.x;
				if (vTemp.y > vMaxs.y)
					vMaxs.y = vTemp.y;
				if (vTemp.z > vMaxs.z)
					vMaxs.z = vTemp.z;
			}
		}

		baseindex += m_pBodyPart->nummodels;
	}

	bExternalEntity = true;
	StudioSetUpTransform(0);
	StudioSetupBones();

	//memcpy(pExtraData->pbones, m_pbonetransform, sizeof(float) * 12 * pExtraData->pModelData->pHdr->numbones);
	bExternalEntity = false;

	VectorCopy(vMins, m_pCurrentEntity->curstate.mins);
	VectorCopy(vMaxs, m_pCurrentEntity->curstate.maxs);

	VectorAdd(vMins, m_pCurrentEntity->origin, pExtraData->absmin);
	VectorAdd(vMaxs, m_pCurrentEntity->origin, pExtraData->absmax);

	glm::vec3 entityangles = glm::vec3(-m_pCurrentEntity->angles.x, m_pCurrentEntity->angles.y, m_pCurrentEntity->angles.z);

	float scale = m_pCurrentEntity->curstate.scale ? m_pCurrentEntity->curstate.scale : 1;

	glm::mat4 modelview = glm::translate(glm::mat4(1.0f), glm::vec3(m_pCurrentEntity->origin.x, m_pCurrentEntity->origin.y, m_pCurrentEntity->origin.z));
	modelview = glm::rotate(modelview, glm::radians(entityangles.y), glm::vec3(0.0f, 0.0f, 1.0f));
	modelview = glm::rotate(modelview, glm::radians(entityangles.x), glm::vec3(0.0f, 1.0f, 0.0f));
	modelview = glm::rotate(modelview, glm::radians(entityangles.z), glm::vec3(1.0f, 0.0f, 0.0f));
	modelview = glm::scale(modelview, glm::vec3(scale));

	pExtraData->modelmatrix = modelview;

	SV_FindTouchedLeafs(pExtraData, BSPWorld_Model::m_pWorldNodes);
}

/*
====================
StudioDrawPropModel

====================
*/
void CStudioModelRenderer::StudioDrawPropModel(void)
{

	glm::mat4 oldmdlmatrix = gBSPRenderer.m_ModelMatrix;

	gBSPRenderer.m_ModelMatrix = m_pCurrentExtraData->modelmatrix;

	StudioSetupRenderer();

	gBSPRenderer.m_ModelMatrix = oldmdlmatrix;

	int baseindex = 0;
	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		int index;
		m_pBodyPart = (mstudiobodyparts_t*)((byte*)m_pStudioHeader + m_pStudioHeader->bodypartindex) + i;

		index = m_pCurrentEntity->curstate.body / m_pBodyPart->base;
		index = index % m_pBodyPart->nummodels;

		m_pSubModel = (mstudiomodel_t*)((byte*)m_pStudioHeader + m_pBodyPart->modelindex) + index;
		pVBOSubModel = &pVBOHeader->submodels[baseindex + index];

		StudioDrawPointsProp();
		baseindex += m_pBodyPart->nummodels;
	}

	StudioDrawDecals();

	if (gBSPRenderer.m_pCvarWireFrame->value)
		StudioDrawWireframeProp();

	if (m_pCvarStudioModelBBox->value > 0)
		StudioDrawBBox();
}

/*
====================
StudioDrawPointsProp

====================
*/
void CStudioModelRenderer::StudioDrawPointsProp(void)
{

	int skinnum = m_pCurrentEntity->curstate.skin; // for short..
	if (skinnum < 0)
		skinnum = 0;

	short* pskinref = m_pCurrentStudioMDL->GetSkinIndexes();

	if (skinnum != 0 && skinnum < m_pCurrentStudioMDL->GetNumSkinFamilies())
		pskinref += (skinnum * m_pCurrentStudioMDL->GetNumSkinIndexes());

	mstudiomesh_t* pmesh = (mstudiomesh_t*)((byte*)m_pStudioHeader + m_pSubModel->meshindex);

	bool hasadditive = false;

	for (int i = 0; i < m_pSubModel->nummesh; i++)
	{
		int meshskinref = std::clamp(pmesh[i].skinref, 0, m_pCurrentStudioMDL->GetNumTextures() - 1);

		vbomesh_t* pvbomesh = &pVBOSubModel->meshes[i];
		auto ptex = m_pCurrentStudioMDL->GetTextureByIndex(pskinref[meshskinref]);
		if (ptex->GetTextureFlags() & STUDIO_NF_ADDITIVE)
		{
			hasadditive = true;
			continue;
		}

		StudioDrawMeshProp(ptex, pvbomesh);
		gBSPRenderer.m_iStudioPolyCounter += pmesh[i].numtris;

	}

	if (!hasadditive)
		return;

	//render textures that require blending

	g_GlobalGLState.SetBlend(true);
	g_GlobalGLState.SetBlendFunc(GL_ONE, GL_ONE);
	g_GlobalGLState.SetDepthWrite(false);

	for (int i = 0; i < m_pSubModel->nummesh; i++)
	{
		int meshskinref = std::clamp(pmesh[i].skinref, 0, m_pCurrentStudioMDL->GetNumTextures() - 1);

		vbomesh_t* pvbomesh = &pVBOSubModel->meshes[i];
		auto ptex = m_pCurrentStudioMDL->GetTextureByIndex(pskinref[meshskinref]);
		if (!(ptex->GetTextureFlags() & STUDIO_NF_ADDITIVE))
			continue;

		StudioDrawMeshProp(ptex, pvbomesh);
		gBSPRenderer.m_iStudioPolyCounter += pmesh[i].numtris;

	}

	g_GlobalGLState.SetBlend(false);
	g_GlobalGLState.SetDepthWrite(true);
}

#define BUFFER_OFFSET(i) ((unsigned int*)NULL + (i))

/*
====================
StudioDrawMeshProp

====================
*/
void CStudioModelRenderer::StudioDrawMeshProp(StudioMDL_Texture *ptex, vbomesh_t* pmesh)
{
	s_MDLShader.Uniform1i(eModel_ShaderLocs[mdlshader_texture_flags], ptex->GetTextureFlags());

	gBSPRenderer.BindGLTexture(GL_TEXTURE0, ptex->GetTextureInfo().iIndex);

	g_pStaticMeshBuffer->DrawElements(pmesh->start_vertex, pmesh->num_vertexes);
}

/*
====================
StudioDrawWireframeProp

====================
*/
void CStudioModelRenderer::StudioDrawWireframeProp(void)
{
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	g_GlobalGLState.SetCullFace(false);
	glLineWidth(1);

	s_MDLShader.Uniform1i(eModel_ShaderLocs[mdlshader_wireframe], 1);

	if (gBSPRenderer.m_pCvarWireFrame->value >= 3)
	{
		g_GlobalGLState.SetDepthTest(false);
	}

	int baseindex = 0;
	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		int index;
		m_pBodyPart = (mstudiobodyparts_t*)((byte*)m_pStudioHeader + m_pStudioHeader->bodypartindex) + i;

		index = m_pCurrentEntity->curstate.body / m_pBodyPart->base;
		index = index % m_pBodyPart->nummodels;

		m_pSubModel = (mstudiomodel_t*)((byte*)m_pStudioHeader + m_pBodyPart->modelindex) + index;
		pVBOSubModel = &pVBOHeader->submodels[baseindex + index];

		StudioDrawPointsProp();
		baseindex += m_pBodyPart->nummodels;
	}

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	g_GlobalGLState.SetCullFace(true);

	if (gBSPRenderer.m_pCvarWireFrame->value >= 2)
	{
		g_GlobalGLState.SetDepthTest(true);
	}

	s_MDLShader.Uniform1i(eModel_ShaderLocs[mdlshader_wireframe], 0);
}

studioentity_data_t* CStudioModelRenderer::StudioAllocEntityData(void)
{
	auto ret = std::make_unique<studioentity_data_t>();
	studioentity_data_t* pStudioData = pStudioEntityData.emplace_back(std::move(ret)).get();
	pStudioData->staticprop = false;

	return pStudioData;
}

/*
====================
StudioAllocDecalSlot

====================
*/
studiodecal_t* CStudioModelRenderer::StudioAllocDecalSlot(void)
{
	auto ret = std::make_unique<studiodecal_t>();
	studiodecal_t* pDecal = pStudioDecals.emplace_back(std::move(ret)).get();

	return pDecal;
};

/*
====================
StudioAllocDecal

====================
*/
studiodecal_t* CStudioModelRenderer::StudioAllocDecal(void)
{
	studiodecal_t* pDecal = StudioAllocDecalSlot();

	if (!m_pCurrentEntity->efrag)
	{
		studioentity_data_t* pEntityData = StudioAllocEntityData();
		pEntityData->entity_index = m_pCurrentEntity->index;
		pEntityData->entity_model = m_pCurrentEntity->model;
		m_pCurrentEntity->efrag = (struct efrag_s*)pEntityData;
	}

	((studioentity_data_t*)m_pCurrentEntity->efrag)->m_vStudioDecals.push_back(pDecal);
	return pDecal;

}

/*
====================
StudioDecalForEntity

====================
*/
void CStudioModelRenderer::StudioDecalForEntity(Vector position, Vector normal, const char* szName, cl_entity_t* pEntity)
{
	if (!pEntity->model)
		return;

	if (pEntity->model->type != mod_studio)
		return;

	if (pEntity == &engine_cl->viewent)
		return;

	decalgroup_t* group = gBSPRenderer.FindGroup(szName);

	if (!group)
		return;

	const decalgroupentry_t* texptr = gBSPRenderer.GetRandomDecal(group);

	if (!texptr)
		return;

	SetCurrentEntity(pEntity);
	SetCurrentMDL(pEntity->model);

	studiodecal_t* pDecal = StudioAllocDecal();

	if (!pDecal)
		return;

	pDecal->texture = texptr;

	StudioSetUpTransform(0);
	StudioSetupBones();

	for (int i = 0; i < m_pCurrentStudioMDL->GetNumBodyParts(); i++)
	{
		StudioSetupModel(i);
		StudioDecalForSubModel(position, normal, pDecal);
	}

	std::vector<studiomdl_vertbufferdata_t> decalverts;

	int numverts = 0;

	for (int i = 0; i < pDecal->numpolys; i++)
	{
		decalvert_t* verts = &pDecal->polys[i].verts[0];
		for (int j = 1; j < pDecal->polys[i].numverts - 1; j++)
		{
			studiomdl_vertbufferdata_t v0, v1, v2;
			v0.pos = pDecal->verts[verts[0].vertindex].position;
			v1.pos = pDecal->verts[verts[j].vertindex].position;
			v2.pos = pDecal->verts[verts[j + 1].vertindex].position;

			int width = pDecal->texture->xsize;
			int height = pDecal->texture->ysize;

			v0.texcoord[0] = static_cast<unsigned short>(verts[0].texcoord[0] * width);
			v1.texcoord[0] = static_cast<unsigned short>(verts[j].texcoord[0] * width);
			v2.texcoord[0] = static_cast<unsigned short>(verts[j + 1].texcoord[0] * width);

			v0.texcoord[1] = static_cast<unsigned short>(verts[0].texcoord[1] * height);
			v1.texcoord[1] = static_cast<unsigned short>(verts[j].texcoord[1] * height);
			v2.texcoord[1] = static_cast<unsigned short>(verts[j + 1].texcoord[1] * height);

			v0.bonedata = (pDecal->verts[verts[0].vertindex].boneindex & 0xFF) | (0 & (0xFF << 8));
			v1.bonedata = (pDecal->verts[verts[j].vertindex].boneindex & 0xFF) | (0 & (0xFF << 8));
			v2.bonedata = (pDecal->verts[verts[j + 1].vertindex].boneindex & 0xFF) | (0 & (0xFF << 8));

			v0.normal.x = 0;
			v0.normal.y = 0;
			v0.normal.z = 32768;

			v1.normal.x = 0;
			v1.normal.y = 0;
			v1.normal.z = 32768;

			v2.normal.x = 0;
			v2.normal.y = 0;
			v2.normal.z = 32768;

			decalverts.push_back(v0);
			decalverts.push_back(v1);
			decalverts.push_back(v2);

			numverts += 3;
		}
	}

	pDecal->numverts = numverts; //sorry

	int structsize = sizeof(studiomdl_vertbufferdata_t);

	if ((iNumStudioDecalVerts + numverts) >= (3 * 65536))
	{
		iNumStudioDecalVerts = 0; //uh oh, this is bad, ran out of space ! start from beginning. gonna cause some problems :(
	}

	pModel_DecalBuffer->Bind(GL_BufferHandler::ArrayBuffer);
	pModel_DecalBuffer->BufferSubData(GL_BufferHandler::ArrayBuffer, iNumStudioDecalVerts * structsize, numverts * structsize, decalverts.data());
	pDecal->vertstart = iNumStudioDecalVerts;
	iNumStudioDecalVerts += numverts;

	GL_BufferHandler::ResetBufferBinding(GL_BufferHandler::ArrayBuffer);
}

/*
====================
StudioDecalTriangle

====================
*/
void CStudioModelRenderer::StudioDecalTriangle(studiotri_t* tri, Vector position, Vector normal, studiodecal_t* decal)
{
	Vector dverts1[10];
	Vector dverts2[10];

	Vector norm, v1, v2, v3;
	VectorSubtract(vVertexTransform[tri->verts[1].vertindex], vVertexTransform[tri->verts[0].vertindex], v1);
	VectorSubtract(vVertexTransform[tri->verts[2].vertindex], vVertexTransform[tri->verts[1].vertindex], v2);
	CrossProduct(v2, v1, norm);

	if (DotProduct(normal, norm) < 0.0)
		return;

	Vector right, up;
	gBSPRenderer.GetUpRight(normal, up, right);
	float texc_orig_x = DotProduct(position, right);
	float texc_orig_y = DotProduct(position, up);

	int xsize = decal->texture->xsize;
	int ysize = decal->texture->ysize;

	for (int i = 0; i < 3; i++)
		VectorCopy(vVertexTransform[tri->verts[i].vertindex], dverts1[i]);

	int nv;
	Vector planepoint;
	VectorMA(position, -xsize, right, planepoint);
	nv = gBSPRenderer.ClipPolygonByPlane(dverts1, 3, right, planepoint, dverts2);

	if (nv < 3)
		return;

	VectorMA(position, xsize, right, planepoint);
	nv = gBSPRenderer.ClipPolygonByPlane(dverts2, nv, right * -1, planepoint, dverts1);

	if (nv < 3)
		return;

	VectorMA(position, -ysize, up, planepoint);
	nv = gBSPRenderer.ClipPolygonByPlane(dverts1, nv, up, planepoint, dverts2);

	if (nv < 3)
		return;

	VectorMA(position, ysize, up, planepoint);
	nv = gBSPRenderer.ClipPolygonByPlane(dverts2, nv, up * -1, planepoint, dverts1);

	if (nv < 3)
		return;

	// Only allow cut polys if the poly is only transformed by one bone
	if (nv > 3 && (tri->verts[0].boneindex != tri->verts[1].boneindex || tri->verts[0].boneindex != tri->verts[2].boneindex || tri->verts[1].boneindex != tri->verts[2].boneindex))
		return;

	// Check if the poly was cut
	if ((dverts1[0] != vVertexTransform[tri->verts[2].vertindex] || dverts1[1] != vVertexTransform[tri->verts[0].vertindex] || dverts1[2] != vVertexTransform[tri->verts[1].vertindex]) && (tri->verts[0].boneindex != tri->verts[1].boneindex || tri->verts[0].boneindex != tri->verts[2].boneindex || tri->verts[1].boneindex != tri->verts[2].boneindex))
		return;

	byte indexes[10];
	if (nv == 3 && dverts1[0] == vVertexTransform[tri->verts[2].vertindex] && dverts1[1] == vVertexTransform[tri->verts[0].vertindex] && dverts1[2] == vVertexTransform[tri->verts[1].vertindex])
	{
		indexes[0] = tri->verts[2].boneindex;
		indexes[1] = tri->verts[0].boneindex;
		indexes[2] = tri->verts[1].boneindex;
	}
	else
	{
		for (int i = 0; i < nv; i++)
			indexes[i] = tri->verts[0].boneindex;
	}

	decal->polys = (decalpoly_t*)ResizeArray((byte*)decal->polys, sizeof(decalpoly_t), decal->numpolys);
	decalpoly_t* polygon = &decal->polys[decal->numpolys];
	decal->numpolys++;

	polygon->verts = new decalvert_t[nv];
	polygon->numverts = nv;

	for (int i = 0; i < nv; i++)
	{
		float texc_x = (DotProduct(dverts1[i], right) - texc_orig_x) / xsize;
		float texc_y = (DotProduct(dverts1[i], up) - texc_orig_y) / ysize;
		polygon->verts[i].texcoord[0] = ((texc_x + 1) / 2);
		polygon->verts[i].texcoord[1] = ((texc_y + 1) / 2);

		Vector temp, fpos; // PINGAS
		temp[0] = dverts1[i][0] - (*m_pbonetransform)[indexes[i]][0][3];
		temp[1] = dverts1[i][1] - (*m_pbonetransform)[indexes[i]][1][3];
		temp[2] = dverts1[i][2] - (*m_pbonetransform)[indexes[i]][2][3];
		VectorIRotate(temp, (*m_pbonetransform)[indexes[i]], fpos);

		int j = 0;
		for (; j < decal->numverts; j++)
		{
			if (decal->verts[j].position == fpos)
			{
				polygon->verts[i].vertindex = j;
				break;
			}
		}

		if (j == decal->numverts)
		{
			decal->verts = (decalvertinfo_t*)ResizeArray((byte*)decal->verts, sizeof(decalvertinfo_t), decal->numverts);
			decal->verts[decal->numverts].boneindex = indexes[i];
			decal->verts[decal->numverts].position = fpos;

			polygon->verts[i].vertindex = decal->numverts;
			decal->numverts++;
		}
	}
}

/*
====================
StudioDecalForSubModel

====================
*/
void CStudioModelRenderer::StudioDecalForSubModel(Vector position, Vector normal, studiodecal_t* decal)
{
	byte* pvertbone = ((byte*)m_pStudioHeader + m_pSubModel->vertinfoindex);
	Vector* pstudioverts = (Vector*)((byte*)m_pStudioHeader + m_pSubModel->vertindex);

	for (int i = 0; i < m_pSubModel->numverts; i++)
		VectorTransform(pstudioverts[i], (*m_pbonetransform)[pvertbone[i]], vVertexTransform[i]);

	for (int i = 0; i < m_pSubModel->nummesh; i++)
	{
		mstudiomesh_t* pmesh = (mstudiomesh_t*)((byte*)m_pStudioHeader + m_pSubModel->meshindex) + i;
		short* ptricmds = (short*)((byte*)m_pStudioHeader + pmesh->triindex);

		int j;
		while (j = *(ptricmds++))
		{
			if (j > 0)
			{
				// convert triangle strip
				j -= 3;
				studiotri_t triangle;
				triangle.verts[0].vertindex = ptricmds[0];
				triangle.verts[0].boneindex = pvertbone[ptricmds[0]];
				ptricmds += 4;

				triangle.verts[1].vertindex = ptricmds[0];
				triangle.verts[1].boneindex = pvertbone[ptricmds[0]];
				ptricmds += 4;

				triangle.verts[2].vertindex = ptricmds[0];
				triangle.verts[2].boneindex = pvertbone[ptricmds[0]];
				ptricmds += 4;

				StudioDecalTriangle(&triangle, position, normal, decal);

				bool reverse = false;
				for (; j > 0; j--, ptricmds += 4)
				{
					studiotri_t tritemp;
					triangle.verts[0] = triangle.verts[1];
					triangle.verts[1] = triangle.verts[2];

					triangle.verts[2].vertindex = ptricmds[0];
					triangle.verts[2].boneindex = pvertbone[ptricmds[0]];

					if (!reverse)
					{
						tritemp.verts[0] = triangle.verts[2];
						tritemp.verts[1] = triangle.verts[1];
						tritemp.verts[2] = triangle.verts[0];
					}
					else
					{
						tritemp.verts[0] = triangle.verts[0];
						tritemp.verts[1] = triangle.verts[1];
						tritemp.verts[2] = triangle.verts[2];
					}
					StudioDecalTriangle(&tritemp, position, normal, decal);
					reverse = !reverse;
				}
			}
			else
			{
				// convert triangle fan
				j = -j - 3;
				studiotri_t triangle;
				triangle.verts[0].vertindex = ptricmds[0];
				triangle.verts[0].boneindex = pvertbone[ptricmds[0]];
				ptricmds += 4;

				triangle.verts[1].vertindex = ptricmds[0];
				triangle.verts[1].boneindex = pvertbone[ptricmds[0]];
				ptricmds += 4;

				triangle.verts[2].vertindex = ptricmds[0];
				triangle.verts[2].boneindex = pvertbone[ptricmds[0]];
				ptricmds += 4;

				StudioDecalTriangle(&triangle, position, normal, decal);

				for (; j > 0; j--, ptricmds += 4)
				{
					triangle.verts[1] = triangle.verts[2];
					triangle.verts[2].vertindex = ptricmds[0];
					triangle.verts[2].boneindex = pvertbone[ptricmds[0]];

					StudioDecalTriangle(&triangle, position, normal, decal);
				}
			}
		}
	}
}

/*
====================
StudioDrawDecals

====================
*/
void CStudioModelRenderer::StudioDrawDecals(void)
{
	if (m_pCvarStudioModelDecals->value < 1)
		return;

	if (!m_pCurrentEntity->efrag)
		return;

	studioentity_data_t* pentitydata = (studioentity_data_t*)m_pCurrentEntity->efrag;

	if (pentitydata->m_vStudioDecals.empty())
		return;

	// Just to make sure
	if (m_pCurrentEntity == &engine_cl->viewent)
		return;

	g_GlobalGLState.SetBlend(true);
	g_GlobalGLState.SetDepthWrite(false);
	g_GlobalGLState.SetBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	glPolygonOffset(-1, -1);
	g_GlobalGLState.SetPolygonOffsetFill(true);

	auto prev_vao = GL_VertexArrayObject::GetBoundVAO();
	pModel_DecalVAO->BindVAO();

	s_MDLShader.Uniform1i(eModel_ShaderLocs[mdlshader_studiodecal], 1);
	for (auto& studiodecal : pentitydata->m_vStudioDecals)
	{
		s_MDLShader.Uniform2f(eModel_ShaderLocs[mdlshader_decalsize], studiodecal->texture->xsize, studiodecal->texture->ysize);
		gBSPRenderer.BindGLTexture(GL_TEXTURE0, studiodecal->texture->gl_texid);

		int startvert = studiodecal->vertstart;
		glDrawArrays(GL_TRIANGLES, startvert, studiodecal->numverts);
		
	}
	s_MDLShader.Uniform1i(eModel_ShaderLocs[mdlshader_studiodecal], 0);

	prev_vao->BindVAO();

	g_GlobalGLState.SetPolygonOffsetFill(false);
	g_GlobalGLState.SetBlend(false);
	g_GlobalGLState.SetDepthWrite(true);
}

/*
====================
StudioDecalExternal

====================
*/
void CStudioModelRenderer::StudioDecalExternal(Vector vpos, Vector vnorm, const char* name)
{
	int nv;

	Vector vtemp;
	Vector planepoint, right, up;
	Vector norm, v1, v2, v3;
	Vector vdecalmins, vdecalmaxs;
	Vector vtranspos, vtransnorm;

	Vector dverts1[10];
	Vector dverts2[10];

	decalgroup_t* group = gBSPRenderer.FindGroup(name);

	if (!group)
		return;

	const decalgroupentry_t* texptr = gBSPRenderer.GetRandomDecal(group);

	if (!texptr)
		return;

	float radius = (texptr->xsize > texptr->ysize) ? texptr->xsize : texptr->ysize;

	vdecalmins[0] = vpos[0] - radius;
	vdecalmins[1] = vpos[1] - radius;
	vdecalmins[2] = vpos[2] - radius;
	vdecalmaxs[0] = vpos[0] + radius;
	vdecalmaxs[1] = vpos[1] + radius;
	vdecalmaxs[2] = vpos[2] + radius;

	//SALSATOBIAS: if decals for external models dont work look at code below plis its pretty messy

	for (auto propent : gPropManager.m_pEntities)
	{
		if(!propent.efrag)
			propent.efrag = (efrag_t*)StudioAllocEntityData();

		m_pCurrentStudioEntData = (studioentity_data_t*)propent.efrag;
		m_pCurrentStudioEntData->staticprop = true;

		if (!m_pCurrentStudioEntData->entity_extrainfo)
			continue;

		entextrainfo_t* pInfo = m_pCurrentStudioEntData->entity_extrainfo;
		VectorCopy(pInfo->pExtraData->absmax, v_bboxMaxs);
		VectorCopy(pInfo->pExtraData->absmin, v_bboxMins);

		if (StudioCullBBox(vdecalmins, vdecalmaxs))
			continue;

		propent.angles[PITCH] = -propent.angles[PITCH];
		AngleMatrix(propent.angles, (*m_protationmatrix));
		propent.angles[PITCH] = -propent.angles[PITCH];

		VectorSubtract(vpos, propent.origin, vtemp);
		VectorIRotate(vtemp, (*m_protationmatrix), vtranspos);
		VectorIRotate(vnorm, (*m_protationmatrix), vtransnorm);

		SetCurrentEntity(&propent, true);
		studiodecal_t* pDecal = StudioAllocDecal();

		if (!pDecal)
			continue;

		pDecal->texture = texptr;
		SetCurrentMDL(pInfo->pExtraData->pModelData->pMdl);
		pVBOHeader = &pInfo->pExtraData->pModelData->pVBOHeader;

		int baseindex = 0;
		for (int j = 0; j < m_pStudioHeader->numbodyparts; j++)
		{
			int index;
			m_pBodyPart = (mstudiobodyparts_t*)((byte*)m_pStudioHeader + m_pStudioHeader->bodypartindex) + j;

			index = propent.curstate.body / m_pBodyPart->base;
			index = index % m_pBodyPart->nummodels;

			m_pSubModel = (mstudiomodel_t*)((byte*)m_pStudioHeader + m_pBodyPart->modelindex) + index;
			pVBOSubModel = &pVBOHeader->submodels[baseindex + index];
			baseindex += m_pBodyPart->nummodels;

			int skinnum = m_pCurrentEntity->curstate.skin; // for short..
			if (skinnum < 0)
				skinnum = 0;

			short* pskinref = m_pCurrentStudioMDL->GetSkinIndexes();

			if (skinnum != 0 && skinnum < m_pCurrentStudioMDL->GetNumSkinFamilies())
				pskinref += (skinnum * m_pCurrentStudioMDL->GetNumSkinIndexes());

			mstudiomesh_t* pmesh = (mstudiomesh_t*)((byte*)m_pStudioHeader + m_pSubModel->meshindex);

			for (int k = 0; k < pVBOSubModel->nummeshes; k++)
			{
				int meshskinref = std::clamp(pmesh[k].skinref, 0, m_pCurrentStudioMDL->GetNumTextures() - 1);

				auto ptexture = m_pCurrentStudioMDL->GetTextureByIndex(pskinref[meshskinref]);

				if (ptexture->GetTextureFlags() & STUDIO_NF_MASKED)
					continue;

				vbomesh_t* pmesh = &pVBOSubModel->meshes[k];
				for (int l = 0; l < pmesh->num_vertexes; l += 3)
				{
					Default3DVert_t* pv1 = &gPropManager.m_pVertexData[gPropManager.m_pIndexBuffer[(pmesh->start_vertex + l)]];
					Default3DVert_t* pv2 = &gPropManager.m_pVertexData[gPropManager.m_pIndexBuffer[(pmesh->start_vertex + l + 1)]];
					Default3DVert_t* pv3 = &gPropManager.m_pVertexData[gPropManager.m_pIndexBuffer[(pmesh->start_vertex + l + 2)]];

					VectorSubtract(pv2->pos, pv1->pos, v1);
					VectorSubtract(pv3->pos, pv2->pos, v2);
					CrossProduct(v2, v1, norm);

					if (DotProduct(vtransnorm, norm) < 0.0)
						continue;

					gBSPRenderer.GetUpRight(vtransnorm, up, right);
					float texc_orig_x = DotProduct(vtranspos, right);
					float texc_orig_y = DotProduct(vtranspos, up);

					int xsize = texptr->xsize;
					int ysize = texptr->ysize;

					VectorCopy(&pv1->pos, &dverts1[0]);
					VectorCopy(&pv2->pos, &dverts1[1]);
					VectorCopy(&pv3->pos, &dverts1[2]);

					VectorMA(vtranspos, -xsize, right, planepoint);
					nv = gBSPRenderer.ClipPolygonByPlane(dverts1, 3, right, planepoint, dverts2);

					if (nv < 3)
						continue;

					VectorMA(vtranspos, xsize, right, planepoint);
					nv = gBSPRenderer.ClipPolygonByPlane(dverts2, nv, right * -1, planepoint, dverts1);

					if (nv < 3)
						continue;

					VectorMA(vtranspos, -ysize, up, planepoint);
					nv = gBSPRenderer.ClipPolygonByPlane(dverts1, nv, up, planepoint, dverts2);

					if (nv < 3)
						continue;

					VectorMA(vtranspos, ysize, up, planepoint);
					nv = gBSPRenderer.ClipPolygonByPlane(dverts2, nv, up * -1, planepoint, dverts1);

					if (nv < 3)
						continue;

					pDecal->polys = (decalpoly_t*)ResizeArray((byte*)pDecal->polys, sizeof(decalpoly_t), pDecal->numpolys);
					decalpoly_t* polygon = &pDecal->polys[pDecal->numpolys];
					pDecal->numpolys++;
					polygon->verts = new decalvert_t[nv];
					polygon->numverts = nv;

					for (int m = 0; m < nv; m++)
					{
						float texc_x = (DotProduct(dverts1[m], right) - texc_orig_x) / xsize;
						float texc_y = (DotProduct(dverts1[m], up) - texc_orig_y) / ysize;
						polygon->verts[m].texcoord[0] = ((texc_x + 1) / 2);
						polygon->verts[m].texcoord[1] = ((texc_y + 1) / 2);

						int n = 0;
						for (; n < pDecal->numverts; n++)
						{
							if (pDecal->verts[n].position == dverts1[m])
							{
								polygon->verts[m].vertindex = n;
								break;
							}
						}

						if (n == pDecal->numverts)
						{
							pDecal->verts = (decalvertinfo_t*)ResizeArray((byte*)pDecal->verts, sizeof(decalvertinfo_t), pDecal->numverts);
							pDecal->verts[pDecal->numverts].boneindex = NULL;
							pDecal->verts[pDecal->numverts].position = dverts1[m];

							polygon->verts[m].vertindex = pDecal->numverts;
							pDecal->numverts++;
						}
					}
				}
			}
		}
	}
}

/*
====================
StudioDrawModelSolid

====================
*/
void CStudioModelRenderer::StudioDrawModelSolid(void)
{
	if (m_pStudioHeader->numbodyparts == 0)
		return;

	if(IsEntityTransparent(m_pCurrentEntity))
		return;

	(*m_protationmatrix) = m_pCurrentStudioEntData->rotationmatrix;

	if (StudioCheckBBox())
		return;

	//enable buffers here so we dont bind buffers of models we won't draw
#if !defined(GL_ONE_BIG_BUFFER_FOR_STUDIOMDLS)
	if (!m_pCurrentStudioMDL->IsMeshBound())
		m_pCurrentStudioMDL->BindMesh();
#endif

	pModel_BonesBuffer->BindRange(GL_BufferHandler::UniformBuffer, s_MDLSolidShader.GetUBOIndex("BonesUBO"), m_pCurrentStudioEntData->bonearrayoffset, sizeof(matrix3x4_t) * m_pStudioHeader->numbones);

	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		StudioMDL_BodyPart *bodypart = m_pCurrentStudioMDL->GetBodyPartbyIndex(i);
		StudioDrawPointsSolid(bodypart);
	}
}

/*
====================
StudioDrawPointsSolid

====================
*/
void CStudioModelRenderer::StudioDrawPointsSolid(StudioMDL_BodyPart* bodypart)
{
	int index = m_pCurrentEntity->curstate.body / bodypart->GetBase();
	index = index % bodypart->GetNumModels();

	StudioMDL_SubModel* submodel = bodypart->GetModelbyIndex(index);
	if (!submodel->GetMeshNum())
		return;

	int numskinfamilies = m_pCurrentStudioMDL->GetNumSkinIndexes();
	int skinindex = std::clamp((int)m_pCurrentEntity->curstate.skin, 0, numskinfamilies) * numskinfamilies;
	short* pskinref = m_pCurrentStudioMDL->GetSkinIndexes() + skinindex;

	//
	// Render meshes
	//

	bool hasalpha_oradditive = false;
	int indexcount = 0;

	for (int i = 0; i < submodel->GetMeshNum(); i++)
	{
		StudioMDL_Mesh* pmesh = submodel->GetMeshbyIndex(i);

		int meshskinref = std::clamp(pmesh->SkinReference(), 0, m_pCurrentStudioMDL->GetNumTextures() - 1);

		StudioMDL_Texture* ptex = m_pCurrentStudioMDL->GetTextureByIndex(pskinref[meshskinref]);
		int ptexflags = ptex->GetTextureFlags();

		if ((ptexflags & STUDIO_NF_MASKED) || (ptexflags & STUDIO_NF_ADDITIVE))
		{
			hasalpha_oradditive = true;
			break;
		}

		indexcount += pmesh->NumIndices();
	}

	if (!hasalpha_oradditive)
	{
		s_MDLSolidShader.Uniform1i(eModel_ShaderSolidLocs[mdlshadersolid_texture_flags], 0);
		m_pCurrentStudioMDL->DrawElements(submodel->GetMeshbyIndex(0)->MeshBufferOffset(), indexcount);
		return;
	}


	for (int j = 0; j < submodel->GetMeshNum(); j++)
	{
		StudioMDL_Mesh* pmesh = submodel->GetMeshbyIndex(j);

		int meshskinref = std::clamp(pmesh->SkinReference(), 0, m_pCurrentStudioMDL->GetNumTextures() - 1);

		StudioMDL_Texture* ptex = m_pCurrentStudioMDL->GetTextureByIndex(pskinref[meshskinref]);
		int ptexflags = ptex->GetTextureFlags();

		if (ptexflags & STUDIO_NF_ADDITIVE)
			continue;

		s_MDLSolidShader.Uniform1i(eModel_ShaderSolidLocs[mdlshadersolid_texture_flags], ptexflags);

		if (ptexflags & STUDIO_NF_MASKED)
			gBSPRenderer.BindGLTexture(GL_TEXTURE0, ptex->GetTextureInfo().iIndex);

		pmesh->DrawMesh();
	}
	
}

void CStudioModelRenderer::StudioSetupPropDraw(bool solidpass)
{
	bExternalEntity = true;

	if (solidpass)
	{
		s_MDLSolidShader.Bind();

		model_SolidData.projviewmatrix = gBSPRenderer.m_ProjectionMatrix * gBSPRenderer.m_ViewMatrix;

		auto dynl = gBSPRenderer.m_pCurrentDynLight;
		model_SolidData.light_pos = glm::vec4(dynl->origin.x, dynl->origin.y, dynl->origin.z, dynl->radius);
		model_SolidData.int_values.x = 1;

		pModel_SolidBuffer->Bind(GL_BufferHandler::UniformBuffer);

		if (gBSPRenderer.m_bSunShadowMapPass)
		{
			// flip
			s_MDLSolidShader.Uniform1i(eModel_ShaderSolidLocs[mdlshadersolid_sunshadow], 1);
			glCullFace(GL_BACK);
		}
	}
	else
	{
		s_MDLShader.Bind();
	}
}

void CStudioModelRenderer::StudioFinishPropDraw(bool solidpass)
{
	bExternalEntity = false;

	if(solidpass)
	{
		if (gBSPRenderer.m_bSunShadowMapPass)
		{
			// flip
			s_MDLSolidShader.Uniform1i(eModel_ShaderSolidLocs[mdlshadersolid_sunshadow], 0);
			glCullFace(GL_FRONT);
		}
		model_SolidData.int_values.x = 0;
	}
}

/*
====================
StudioDrawPropModelSolid

====================
*/
void CStudioModelRenderer::StudioDrawPropModelSolid(cl_entity_t* pEntity)
{
	SetCurrentEntity(pEntity, true);
	SetCurrentMDL(pEntity->model);

	if (!m_pStudioHeader || !m_pCurrentStudioMDL || !pVBOHeader)
		return;

	v_bboxMins = m_pCurrentExtraData->absmin;
	v_bboxMaxs = m_pCurrentExtraData->absmax;

	if (gHUD.viewFrustum.CullBox(v_bboxMins, v_bboxMaxs))
		return;

	model_SolidData.modelmatrix = m_pCurrentExtraData->modelmatrix;

	pModel_SolidBuffer->Bind(GL_BufferHandler::UniformBuffer);
	pModel_SolidBuffer->BindRange(GL_BufferHandler::UniformBuffer, s_MDLSolidShader.GetUBOIndex("StudioSolidUBO"), 0, sizeof(mdlshadersolid_data_t));
	pModel_SolidBuffer->BufferSubData(GL_BufferHandler::UniformBuffer, 0, sizeof(mdlshadersolid_data_t), &model_SolidData);

	int baseindex = 0;
	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		StudioMDL_BodyPart* bodypart = m_pCurrentStudioMDL->GetBodyPartbyIndex(i);

		StudioDrawPointsSolidEXT(bodypart, baseindex);
		baseindex += bodypart->GetNumModels();
	}
}


/*
====================
StudioDrawPointsSolidEXT

====================
*/
void CStudioModelRenderer::StudioDrawPointsSolidEXT(StudioMDL_BodyPart* bodypart, int baseindex)
{
	int skinnum = m_pCurrentEntity->curstate.skin; // for short..
	if (skinnum < 0)
		skinnum = 0;

	short* pskinref = m_pCurrentStudioMDL->GetSkinIndexes();

	if (skinnum != 0 && skinnum < m_pCurrentStudioMDL->GetNumSkinFamilies())
		pskinref += (skinnum * m_pCurrentStudioMDL->GetNumSkinIndexes());

	int index = m_pCurrentEntity->curstate.body / bodypart->GetBase();
	index = index % bodypart->GetNumModels();

	StudioMDL_SubModel* submodel = bodypart->GetModelbyIndex(index);
	pVBOSubModel = &pVBOHeader->submodels[baseindex + index];

	//optimization attempt: check if submodel has any alpha/additive textures

	bool hasalpha_oradditive = false;
	int numverts = 0;

	for (int i = 0; i < submodel->GetMeshNum(); i++)
	{
		StudioMDL_Mesh* pmesh = submodel->GetMeshbyIndex(i);

		int meshskinref = std::clamp(pmesh->SkinReference(), 0, m_pCurrentStudioMDL->GetNumTextures() - 1);

		auto ptex = m_pCurrentStudioMDL->GetTextureByIndex(pskinref[meshskinref]);
		auto ptexinfo = ptex->GetTextureInfo();
		auto ptexflags = ptex->GetTextureFlags();

		vbomesh_t* pvbomesh = &pVBOSubModel->meshes[i];
		numverts += pvbomesh->num_vertexes;

		if ((ptexflags & STUDIO_NF_MASKED) || (ptexflags & STUDIO_NF_ADDITIVE))
		{
			hasalpha_oradditive = true;
			break;
		}

	}

	if (!hasalpha_oradditive)
	{
		//optimization attempt: just draw the entire submodel with 1 draw call
		s_MDLSolidShader.Uniform1i(eModel_ShaderSolidLocs[mdlshadersolid_texture_flags], 0);
		g_pStaticMeshBuffer->DrawElements(pVBOSubModel->meshes[0].start_vertex, numverts);
	}
	else
	{
		for (int i = 0; i < submodel->GetMeshNum(); i++)
		{
			StudioMDL_Mesh* pmesh = submodel->GetMeshbyIndex(i);

			int meshskinref = std::clamp(pmesh->SkinReference(), 0, m_pCurrentStudioMDL->GetNumTextures() - 1);

			vbomesh_t* pvbomesh = &pVBOSubModel->meshes[i];

			auto ptex = m_pCurrentStudioMDL->GetTextureByIndex(pskinref[meshskinref]);
			auto ptexinfo = ptex->GetTextureInfo();
			auto ptexflags = ptex->GetTextureFlags();

			s_MDLSolidShader.Uniform1i(eModel_ShaderSolidLocs[mdlshadersolid_texture_flags], ptexflags);


			if (ptexflags & STUDIO_NF_ADDITIVE)
				continue;

			if (ptexflags & STUDIO_NF_MASKED)
			{
				gBSPRenderer.BindGLTexture(GL_TEXTURE0, ptex->GetTextureInfo().iIndex);
			}

			g_pStaticMeshBuffer->DrawElements(pvbomesh->start_vertex, pvbomesh->num_vertexes);
		}
	}
}