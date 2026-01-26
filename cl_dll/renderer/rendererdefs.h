/*
Trinity Rendering Engine - Copyright Andrew Lucas 2009-2012

The Trinity Engine is free software, distributed in the hope th-
at it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU Lesser General Public License for more det-
ails.

Renderer base definitions and functions
Written by Andrew Lucas, Richard Rohac, BUzer, Laurie, Botman and Id Software
*/

#ifndef RENDERERDEFS_H
#define RENDERERDEFS_H

#include "PlatformHeaders.h"

#include "gl/glew.h"

#include "gl/gl.h"
#include "gl/glu.h"

#include "dlight.h"
#include "com_model.h"
#include "cl_entity.h"
#include <assert.h>
#include "r_studioint.h"
#include "frustum.h"
#include "studio.h"
#include "pm_defs.h"

#include <vector>
#include <map>
#include <string>

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#undef clamp

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class GL_FBOHandler;
class GL_StateHandler;
class GL_ShadowMap;
class GL_TextureHandler;
class StudioMDL_Model;
struct entextradata_t;
struct decalgroupentry_t;

extern GL_StateHandler g_GlobalGLState;

constexpr char WATER_PASS_TIME[] = "Water_RenderTime";
constexpr char MIRROR_PASS_TIME[] = "Mirror_RenderTime";
constexpr char SHADOWMAP_PASS_TIME[] = "ShadowMap_Pass_RenderTime";
constexpr char MAINWORLDSCENE_PASS_TIME[] = "MainWorldScene_RenderTime";
constexpr char STUDIOMDL_PASS_TIME[] = "StudioMDL_RenderTime";

//==============================
//		SHARED DEFS
//
//==============================
#define MAXRENDERENTS 4096

#ifndef M_PI
#define M_PI 3.14159265358979323846 // matches value in gcc v2 math.h
#endif

struct short_3dvector
{
	short x, y, z;
};
struct short_4dvector
{
	short x, y, z, w;
};

//==============================
//		STUDIO RENDERER DEFS
//
//==============================
#define MAX_MODEL_LIGHTS 12 // 2x(up, down, left, right, front, back)
#define MAX_MODEL_DECALS 16
#define MAX_CACHE_MODELS 2048
#define MAX_MODEL_SHADERS 14

#define TEXFLAG_NONE 1
#define TEXFLAG_FULLBRIGHT 1
#define TEXFLAG_ALTERNATE 2
#define TEXFLAG_NOMIPMAP 4
#define TEXFLAG_ERASE 8

//========================================
//				STUDIO RENDERER STRUCTS
//
//========================================
struct decalvert_t
{
	int vertindex;
	float texcoord[2];
};

struct decalvertinfo_t
{
	Vector position;
	byte boneindex;
};

struct decalpoly_t
{
	decalvert_t* verts;
	int numverts;
};

struct studiodecal_t
{
	decalpoly_t* polys;
	int numpolys;

	decalvertinfo_t* verts;
	int numverts;

	const decalgroupentry_t* texture;

	int vertstart; //index into m_pModelDecals_Buffer
};

struct studioentity_data_t //structure that holds info which is generated per frame per entity
{
	uint32_t entity_index;
	model_t* entity_model;
	matrix3x4_t rotationmatrix;
	matrix3x4_t bonematrix[MAXSTUDIOBONES];
	struct entextrainfo_t* entity_extrainfo;
	int bonearrayoffset;
	float m_flGaitMovement;
	std::vector<studiodecal_t*> m_vStudioDecals;
	bool staticprop;
};

struct studiovert_t
{
	int vertindex; //index into m_VertexTransforms
	int normindex; //index into m_NormalTransforms
	float texcoord[2]; //s, t
	byte boneindex; //index into m_pbonetransforms
};

struct studiotri_t
{
	studiovert_t verts[3];
};

struct mlight_t
{
	Vector origin;
	float radius;
	Vector color;

	bool flashlight;
	Vector forward;
	float spotcos;
	bool justspawned;

	FrustumCheck* frustum;

	Vector mins;
	Vector maxs;
};

struct texentry_t
{
	char szModel[64];
	char szTexture[32];

	int iFlags;
};

struct lighting_ext
{
	Vector ambientlight;
	Vector diffuselight;
	Vector lightdir;
};

//========================================
//				GLOBAL FUNCTION CALLS
//
//========================================
extern engine_studio_api_t IEngineStudio;

extern void ClampColor(int r, int g, int b, color24* out);
extern void FilenameFromPath(const char* szin, char* szout);

extern clientmleaf_t* Mod_PointInLeaf(Vector p);
extern byte* Mod_LeafPVS(clientmleaf_t* leaf);
extern void R_MarkLeaves(clientmleaf_t* pLeaf);

extern void HUD_PrintSpeeds(void);
extern void RenderersDumpInfo(void);
extern void SetupFlashlight(Vector origin, Vector angles, float time, float frametime);

extern unsigned short ByteToUShort(byte* byte);
extern unsigned int ByteToUInt(byte* byte);
extern int ByteToInt(byte* byte);

extern void R_SetupView(ref_params_t* pparams);
extern void R_CalcRefDef(ref_params_t* pparams);
extern void R_DrawNormalTriangles(void);
extern void R_DrawTransparentTriangles(void);

extern int IsEntityMoved(cl_entity_t* e);
extern int IsEntityTransparent(cl_entity_t* e);
extern int IsPitchReversed(float pitch);
extern int BoxOnPlaneSide(Vector emins, Vector emaxs, mplane_t* p);

extern char* strLower(char* str);
extern char* stristr(const char* string, const char* string2);

extern inline void DotProductSub(float* result, Vector* v0, Vector* v1, float* subval);

extern void VectorRotate(const Vector& in1, const matrix3x4_t &in2, Vector& out);
extern void VectorIRotate(const Vector& in1, const matrix3x4_t &in2, Vector& out);
extern void VectorRotateAbs(const Vector& in1, const matrix3x4_t& in2, Vector& out);
extern void SV_FindTouchedLeafs(entextradata_t* ent, clientmnode_t* node);

extern byte* ResizeArray(byte* pOriginal, int iSize, int iCount);

extern void R_Init(void);
extern void R_VidInit(void);
extern void R_Shutdown(void);


//
//		GOLDSRC'S STUDIO FUNCTIONS
//

/*
===============
pfnGetPlayerState

===============
*/
__forceinline entity_state_t* R_StudioGetPlayerState(int index) {
	return (index < 0 || index >= engine_cl->maxclients) ? nullptr : &engine_cl->frames[engine_cl->parsecountmod].playerstate[index];
}

/*
===============
pfnPlayerInfo

===============
*/
__forceinline player_info_t* pfnPlayerInfo(int index) {
	return (index < 0 || index >= engine_cl->maxclients) ? nullptr : &engine_cl->players[index];
}


/*
===============
GetModelByIndex

===============
*/
__forceinline model_t* CL_GetModelByIndex(int modelindex) {
	return modelindex >= 0 && modelindex < 512 ? engine_cl->model_precache[modelindex] : NULL;
}

extern Vector g_vecFull;
#endif